/*
Copyright (c) 2009-2021 Roger Light <roger@atchoo.org>

All rights reserved. This program and the accompanying materials
are made available under the terms of the Eclipse Public License 2.0
and Eclipse Distribution License v1.0 which accompany this distribution.

The Eclipse Public License is available at
   https://www.eclipse.org/legal/epl-2.0/
and the Eclipse Distribution License is available at
  http://www.eclipse.org/org/documents/edl-v10.php.

SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

Contributors:
   Roger Light - initial implementation and documentation.
*/

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#ifdef WITH_BROKER
#  include "mosquitto_broker_internal.h"
#  if defined(WITH_WEBSOCKETS) && WITH_WEBSOCKETS == WS_IS_LWS
#    include <libwebsockets.h>
#  endif
#else
#  include "read_handle.h"
#endif

#include "callbacks.h"
#include "memory_mosq.h"
#include "mosquitto/mqtt_protocol.h"
#include "net_mosq.h"
#include "packet_mosq.h"
#include "read_handle.h"
#include "util_mosq.h"
#ifdef WITH_BROKER
#  include "sys_tree.h"
#  include "send_mosq.h"
#else
#  define metrics__int_inc(stat, val)
#  define metrics__int_dec(stat, val)
#endif

int packet__alloc(struct mosquitto__packet **packet, uint8_t command, uint32_t remaining_length)
{
	uint8_t remaining_bytes[5], byte;
	int8_t remaining_count;
	uint32_t packet_length;
	uint32_t remaining_length_stored;
	int i;

	assert(packet);

	remaining_length_stored = remaining_length;
	remaining_count = 0;
	do{
		byte = remaining_length % 128;
		remaining_length = remaining_length / 128;
		/* If there are more digits to encode, set the top bit of this digit */
		if(remaining_length > 0){
			byte = byte | 0x80;
		}
		remaining_bytes[remaining_count] = byte;
		remaining_count++;
	}while(remaining_length > 0 && remaining_count < 5);
	if(remaining_count == 5) return MOSQ_ERR_PAYLOAD_SIZE;

	packet_length = remaining_length_stored + 1 + (uint8_t)remaining_count;
	(*packet) = mosquitto__malloc(sizeof(struct mosquitto__packet) + packet_length + WS_PACKET_OFFSET);
	if((*packet) == NULL) return MOSQ_ERR_NOMEM;

	/* Clear memory for everything but the payload - that will be set to valid
	 * values when the actual payload is copied in. */
	memset((*packet), 0, sizeof(struct mosquitto__packet));
	(*packet)->command = command;
	(*packet)->remaining_length = remaining_length_stored;
	(*packet)->remaining_count = remaining_count;
	(*packet)->packet_length = packet_length + WS_PACKET_OFFSET;

	(*packet)->payload[WS_PACKET_OFFSET] = (*packet)->command;
	for(i=0; i<(*packet)->remaining_count; i++){
		(*packet)->payload[WS_PACKET_OFFSET+i+1] = remaining_bytes[i];
	}
	(*packet)->pos = WS_PACKET_OFFSET + 1U + (uint8_t)(*packet)->remaining_count;

	return MOSQ_ERR_SUCCESS;
}

void packet__cleanup(struct mosquitto__packet_in *packet)
{
	if(!packet) return;

	/* Free data and reset values */
	packet->command = 0;
	packet->remaining_count = 0;
	packet->remaining_mult = 1;
	packet->remaining_length = 0;
	mosquitto__FREE(packet->payload);
	packet->to_process = 0;
	packet->pos = 0;
}


void packet__cleanup_all_no_locks(struct mosquitto *mosq)
{
	struct mosquitto__packet *packet;

	/* Out packet cleanup */
	while(mosq->out_packet){
		packet = mosq->out_packet;
		/* Free data and reset values */
		mosq->out_packet = mosq->out_packet->next;

		mosquitto__FREE(packet);
	}
	metrics__int_dec(mosq_gauge_out_packets, mosq->out_packet_count);
	metrics__int_dec(mosq_gauge_out_packet_bytes, mosq->out_packet_bytes);
	mosq->out_packet_count = 0;
	mosq->out_packet_bytes = 0;
	mosq->out_packet_last = NULL;

	packet__cleanup(&mosq->in_packet);
}

void packet__cleanup_all(struct mosquitto *mosq)
{
	pthread_mutex_lock(&mosq->out_packet_mutex);
	packet__cleanup_all_no_locks(mosq);
	pthread_mutex_unlock(&mosq->out_packet_mutex);
}


static void packet__queue_append(struct mosquitto *mosq, struct mosquitto__packet *packet)
{
#ifdef WITH_BROKER
	if(db.config->max_queued_messages > 0 && mosq->out_packet_count >= db.config->max_queued_messages){
		mosquitto__free(packet);
		if(mosq->is_dropping == false){
			mosq->is_dropping = true;
			log__printf(NULL, MOSQ_LOG_NOTICE,
					"Outgoing messages are being dropped for client %s.",
					mosq->id);
		}
		metrics__int_inc(mosq_counter_mqtt_publish_dropped, 1);
		return;
	}
#endif

	pthread_mutex_lock(&mosq->out_packet_mutex);
	if(mosq->out_packet){
		mosq->out_packet_last->next = packet;
	}else{
		mosq->out_packet = packet;
	}
	mosq->out_packet_last = packet;
	mosq->out_packet_count++;
	mosq->out_packet_bytes += packet->packet_length;
	metrics__int_inc(mosq_gauge_out_packets, 1);
	metrics__int_inc(mosq_gauge_out_packet_bytes, packet->packet_length);
	pthread_mutex_unlock(&mosq->out_packet_mutex);
}


int packet__queue(struct mosquitto *mosq, struct mosquitto__packet *packet)
{
#ifndef WITH_BROKER
	char sockpair_data = 0;
#endif
	assert(mosq);
	assert(packet);

#if defined(WITH_BROKER) && defined(WITH_WEBSOCKETS) && WITH_WEBSOCKETS == WS_IS_LWS
	if(mosq->wsi){
		packet->next = NULL;
		packet->pos = WS_PACKET_OFFSET;
		packet->to_process = packet->packet_length - WS_PACKET_OFFSET;

		packet__queue_append(mosq, packet);

		lws_callback_on_writable(mosq->wsi);
		return MOSQ_ERR_SUCCESS;
	}else
#elif defined(WITH_WEBSOCKETS) && WITH_WEBSOCKETS == WS_IS_BUILTIN
	if(mosq->transport == mosq_t_ws){
		ws__prepare_packet(mosq, packet);
	}else
#endif
	{
		/* Normal TCP */
		packet->next = NULL;
		packet->pos = WS_PACKET_OFFSET;
		packet->to_process = packet->packet_length - WS_PACKET_OFFSET;
	}

	packet__queue_append(mosq, packet);

#ifdef WITH_BROKER
	return packet__write(mosq);
#else
	/* Write a single byte to sockpairW (connected to sockpairR) to break out
	 * of select() if in threaded mode. */
	if(mosq->sockpairW != INVALID_SOCKET){
#  ifndef WIN32
		if(write(mosq->sockpairW, &sockpair_data, 1)){
		}
#  else
		send(mosq->sockpairW, &sockpair_data, 1, 0);
#  endif
	}

	if(mosq->callback_depth == 0 && mosq->threaded == mosq_ts_none){
		return packet__write(mosq);
	}else{
		return MOSQ_ERR_SUCCESS;
	}
#endif
}


int packet__check_oversize(struct mosquitto *mosq, uint32_t remaining_length)
{
	uint32_t len;

	if(mosq->maximum_packet_size == 0) return MOSQ_ERR_SUCCESS;

	len = remaining_length + packet__varint_bytes(remaining_length);
	if(len > mosq->maximum_packet_size){
		return MOSQ_ERR_OVERSIZE_PACKET;
	}else{
		return MOSQ_ERR_SUCCESS;
	}
}

struct mosquitto__packet *packet__get_next_out(struct mosquitto *mosq)
{
	struct mosquitto__packet *packet = NULL;

	pthread_mutex_lock(&mosq->out_packet_mutex);
	if(mosq->out_packet){
		mosq->out_packet_count--;
		mosq->out_packet_bytes -= mosq->out_packet->packet_length;
		metrics__int_dec(mosq_gauge_out_packets, 1);
		metrics__int_dec(mosq_gauge_out_packet_bytes, mosq->out_packet->packet_length);

		mosq->out_packet = mosq->out_packet->next;
		if(!mosq->out_packet){
			mosq->out_packet_last = NULL;
		}
		packet = mosq->out_packet;
	}
	pthread_mutex_unlock(&mosq->out_packet_mutex);

	return packet;
}


int packet__write(struct mosquitto *mosq)
{
	ssize_t write_length;
	struct mosquitto__packet *packet, *next_packet;
	enum mosquitto_client_state state;

	if(!mosq) return MOSQ_ERR_INVAL;
	if(!net__is_connected(mosq)){
		return MOSQ_ERR_NO_CONN;
	}

	pthread_mutex_lock(&mosq->out_packet_mutex);
	packet = mosq->out_packet;
	pthread_mutex_unlock(&mosq->out_packet_mutex);

	if(packet == NULL){
		return MOSQ_ERR_SUCCESS;
	}

#ifdef WITH_BROKER
	mux__add_out(mosq);
#endif

	state = mosquitto__get_state(mosq);
	if(state == mosq_cs_connect_pending){
		return MOSQ_ERR_SUCCESS;
	}

	while(packet){
		while(packet->to_process > 0){
			write_length = net__write(mosq, &(packet->payload[packet->pos]), packet->to_process);
			if(write_length > 0){
				metrics__int_inc(mosq_counter_bytes_sent, write_length);
				packet->to_process -= (uint32_t)write_length;
				packet->pos += (uint32_t)write_length;
			}else{
#ifdef WIN32
				errno = WSAGetLastError();
#endif
				if(errno == EAGAIN || errno == COMPAT_EWOULDBLOCK
#ifdef WIN32
						|| errno == WSAENOTCONN
#endif
						){
					return MOSQ_ERR_SUCCESS;
				}else{
					switch(errno){
						case COMPAT_ECONNRESET:
							return MOSQ_ERR_CONN_LOST;
						case COMPAT_EINTR:
							return MOSQ_ERR_SUCCESS;
						case EPROTO:
							return MOSQ_ERR_TLS;
						default:
							return MOSQ_ERR_ERRNO;
					}
				}
			}
		}

		metrics__int_inc(mosq_counter_messages_sent, 1);
		if(((packet->command)&0xF6) == CMD_PUBLISH){
#ifndef WITH_BROKER
			callback__on_publish(mosq, packet->mid, 0, NULL);
		}else if(((packet->command)&0xF0) == CMD_DISCONNECT){
			net__socket_shutdown(mosq);
			return MOSQ_ERR_SUCCESS;
#endif
		}

		next_packet = packet__get_next_out(mosq);
		mosquitto__FREE(packet);
		packet = next_packet;

#ifdef WITH_BROKER
		mosq->next_msg_out = db.now_s + mosq->keepalive;
#else
		pthread_mutex_lock(&mosq->msgtime_mutex);
		mosq->next_msg_out = mosquitto_time() + mosq->keepalive;
		pthread_mutex_unlock(&mosq->msgtime_mutex);
#endif
	}
#ifdef WITH_BROKER
	if (mosq->out_packet == NULL) {
		mux__remove_out(mosq);
	}
#endif
	return MOSQ_ERR_SUCCESS;
}


int packet__read(struct mosquitto *mosq)
{
	uint8_t byte;
	ssize_t read_length;
	int rc = 0;
	enum mosquitto_client_state state;
	ssize_t (*local__read)(struct mosquitto *, void *, size_t);

	if(!mosq){
		return MOSQ_ERR_INVAL;
	}
	if(!net__is_connected(mosq)){
		return MOSQ_ERR_NO_CONN;
	}

	state = mosquitto__get_state(mosq);
	if(state == mosq_cs_connect_pending){
		return MOSQ_ERR_SUCCESS;
	}
#if defined(WITH_WEBSOCKETS) && WITH_WEBSOCKETS == WS_IS_BUILTIN
	if(mosq->transport == mosq_t_ws){
		local__read = net__read_ws;
	}else
#endif
	{
		local__read = net__read;
	}

	/* This gets called if pselect() indicates that there is network data
	 * available - ie. at least one byte.  What we do depends on what data we
	 * already have.
	 * If we've not got a command, attempt to read one and save it. This should
	 * always work because it's only a single byte.
	 * Then try to read the remaining length. This may fail because it is may
	 * be more than one byte - will need to save data pending next read if it
	 * does fail.
	 * Then try to read the remaining payload, where 'payload' here means the
	 * combined variable header and actual payload. This is the most likely to
	 * fail due to longer length, so save current data and current position.
	 * After all data is read, send to mosquitto__handle_packet() to deal with.
	 * Finally, free the memory and reset everything to starting conditions.
	 */
	if(!mosq->in_packet.command){
		read_length = local__read(mosq, &byte, 1);
		if(read_length == 1){
			mosq->in_packet.command = byte;
#ifdef WITH_BROKER
			metrics__int_inc(mosq_counter_bytes_received, 1);
			/* Clients must send CONNECT as their first command. */
			if(!(mosq->bridge) && state == mosq_cs_new && (byte&0xF0) != CMD_CONNECT){
				return MOSQ_ERR_PROTOCOL;
			}
#endif
		}else{
			if(read_length == 0){
				return MOSQ_ERR_CONN_LOST; /* EOF */
			}
#ifdef WIN32
			errno = WSAGetLastError();
#endif
			if(errno == EAGAIN || errno == COMPAT_EWOULDBLOCK){
				return MOSQ_ERR_SUCCESS;
			}else{
				switch(errno){
					case COMPAT_ECONNRESET:
						return MOSQ_ERR_CONN_LOST;
					case COMPAT_EINTR:
						return MOSQ_ERR_SUCCESS;
					default:
						return MOSQ_ERR_ERRNO;
				}
			}
		}
	}
	/* remaining_count is the number of bytes that the remaining_length
	 * parameter occupied in this incoming packet. We don't use it here as such
	 * (it is used when allocating an outgoing packet), but we must be able to
	 * determine whether all of the remaining_length parameter has been read.
	 * remaining_count has three states here:
	 *   0 means that we haven't read any remaining_length bytes
	 *   <0 means we have read some remaining_length bytes but haven't finished
	 *   >0 means we have finished reading the remaining_length bytes.
	 */
	if(mosq->in_packet.remaining_count <= 0){
		do{
			read_length = local__read(mosq, &byte, 1);
			if(read_length == 1){
				mosq->in_packet.remaining_count--;
				/* Max 4 bytes length for remaining length as defined by protocol.
				 * Anything more likely means a broken/malicious client.
				 */
				if(mosq->in_packet.remaining_count < -4){
					return MOSQ_ERR_MALFORMED_PACKET;
				}

				metrics__int_inc(mosq_counter_bytes_received, 1);
				mosq->in_packet.remaining_length += (byte & 127) * mosq->in_packet.remaining_mult;
				mosq->in_packet.remaining_mult *= 128;
			}else{
				if(read_length == 0){
					return MOSQ_ERR_CONN_LOST; /* EOF */
				}
#ifdef WIN32
				errno = WSAGetLastError();
#endif
				if(errno == EAGAIN || errno == COMPAT_EWOULDBLOCK){
					return MOSQ_ERR_SUCCESS;
				}else{
					switch(errno){
						case COMPAT_ECONNRESET:
							return MOSQ_ERR_CONN_LOST;
						case COMPAT_EINTR:
							return MOSQ_ERR_SUCCESS;
						default:
							return MOSQ_ERR_ERRNO;
					}
				}
			}
		}while((byte & 128) != 0);
		/* We have finished reading remaining_length, so make remaining_count
		 * positive. */
		mosq->in_packet.remaining_count = (int8_t)(mosq->in_packet.remaining_count * -1);

#ifdef WITH_BROKER
		switch(mosq->in_packet.command & 0xF0){
			case CMD_CONNECT:
				if(mosq->in_packet.remaining_length > 100000){ /* Arbitrary limit, make configurable */
					return MOSQ_ERR_MALFORMED_PACKET;
				}
				break;

			case CMD_PUBACK:
			case CMD_PUBREC:
			case CMD_PUBREL:
			case CMD_PUBCOMP:
			case CMD_UNSUBACK:
				if(mosq->protocol != mosq_p_mqtt5 && mosq->in_packet.remaining_length != 2){
					return MOSQ_ERR_MALFORMED_PACKET;
				}
				break;

			case CMD_PINGREQ:
			case CMD_PINGRESP:
				if(mosq->in_packet.remaining_length != 0){
					return MOSQ_ERR_MALFORMED_PACKET;
				}
				break;

			case CMD_DISCONNECT:
				if(mosq->protocol != mosq_p_mqtt5 && mosq->in_packet.remaining_length != 0){
					return MOSQ_ERR_MALFORMED_PACKET;
				}
				break;
		}

		if(db.config->max_packet_size > 0 && mosq->in_packet.remaining_length+1 > db.config->max_packet_size){
			if(mosq->protocol == mosq_p_mqtt5){
				send__disconnect(mosq, MQTT_RC_PACKET_TOO_LARGE, NULL);
			}
			return MOSQ_ERR_OVERSIZE_PACKET;
		}
#else
		/* FIXME - client case for incoming message received from broker too large */
#endif
		if(mosq->in_packet.remaining_length > 0){
			mosq->in_packet.payload = mosquitto__malloc(mosq->in_packet.remaining_length*sizeof(uint8_t));
			if(!mosq->in_packet.payload){
				return MOSQ_ERR_NOMEM;
			}
			mosq->in_packet.to_process = mosq->in_packet.remaining_length;
		}
	}
	while(mosq->in_packet.to_process>0){
		read_length = local__read(mosq, &(mosq->in_packet.payload[mosq->in_packet.pos]), mosq->in_packet.to_process);
		if(read_length > 0){
			metrics__int_inc(mosq_counter_bytes_received, read_length);
			mosq->in_packet.to_process -= (uint32_t)read_length;
			mosq->in_packet.pos += (uint32_t)read_length;
		}else{
#ifdef WIN32
			errno = WSAGetLastError();
#endif
			if(errno == EAGAIN || errno == COMPAT_EWOULDBLOCK){
				if(mosq->in_packet.to_process > 1000){
					/* Update last_msg_in time if more than 1000 bytes left to
					 * receive. Helps when receiving large messages.
					 * This is an arbitrary limit, but with some consideration.
					 * If a client can't send 1000 bytes in a second it
					 * probably shouldn't be using a 1 second keep alive. */
#ifdef WITH_BROKER
					keepalive__update(mosq);
#else
					pthread_mutex_lock(&mosq->msgtime_mutex);
					mosq->last_msg_in = mosquitto_time();
					pthread_mutex_unlock(&mosq->msgtime_mutex);
#endif
				}
				return MOSQ_ERR_SUCCESS;
			}else{
				switch(errno){
					case COMPAT_ECONNRESET:
						return MOSQ_ERR_CONN_LOST;
					case COMPAT_EINTR:
						return MOSQ_ERR_SUCCESS;
					default:
						return MOSQ_ERR_ERRNO;
				}
			}
		}
	}

	/* All data for this packet is read. */
	mosq->in_packet.pos = 0;
#ifdef WITH_BROKER
	metrics__int_inc(mosq_counter_messages_received, 1);
#endif
	rc = handle__packet(mosq);

	/* Free data and reset values */
	packet__cleanup(&mosq->in_packet);

#ifdef WITH_BROKER
	keepalive__update(mosq);
#else
	pthread_mutex_lock(&mosq->msgtime_mutex);
	mosq->last_msg_in = mosquitto_time();
	pthread_mutex_unlock(&mosq->msgtime_mutex);
#endif
	return rc;
}
