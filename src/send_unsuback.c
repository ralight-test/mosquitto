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

#include "mosquitto_broker_internal.h"
#include "mosquitto/mqtt_protocol.h"
#include "memory_mosq.h"
#include "packet_mosq.h"
#include "property_mosq.h"
#include "sys_tree.h"


int send__unsuback(struct mosquitto *mosq, uint16_t mid, int reason_code_count, uint8_t *reason_codes, const mosquitto_property *properties)
{
	struct mosquitto__packet *packet = NULL;
	int rc;
	uint32_t remaining_length;

	assert(mosq);

	remaining_length = 2;
	if(mosq->protocol == mosq_p_mqtt5){
		remaining_length += property__get_remaining_length(properties);
		remaining_length += (uint32_t)reason_code_count;
	}

	rc = packet__alloc(&packet, CMD_UNSUBACK, remaining_length);
	if(rc){
		return rc;
	}

	packet__write_uint16(packet, mid);

	if(mosq->protocol == mosq_p_mqtt5){
		property__write_all(packet, properties, true);
        packet__write_bytes(packet, reason_codes, (uint32_t)reason_code_count);
	}

	metrics__int_inc(mosq_counter_mqtt_unsuback_sent, 1);
	return packet__queue(mosq, packet);
}
