/*
Copyright (c) 2010-2021 Roger Light <roger@atchoo.org>

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

#ifndef MEMORY_MOSQ_H
#define MEMORY_MOSQ_H

#include <stdio.h>
#include <sys/types.h>

#if defined(WITH_MEMORY_TRACKING) && defined(WITH_BROKER)
#  if defined(__APPLE__) || defined(__FreeBSD__) || defined(__GLIBC__)
#    define REAL_WITH_MEMORY_TRACKING
#  endif
#endif

void *mosquitto__calloc(size_t nmemb, size_t size);
void mosquitto__free(void *mem);
void *mosquitto__malloc(size_t size);
#ifdef REAL_WITH_MEMORY_TRACKING
unsigned long mosquitto__memory_used(void);
unsigned long mosquitto__max_memory_used(void);
#endif
void *mosquitto__realloc(void *ptr, size_t size);
char *mosquitto__strdup(const char *s);
char *mosquitto__strndup(const char *s, size_t n);

#ifdef WITH_BROKER
void memory__set_limit(size_t lim);
#endif

#define mosquitto__FREE(A) do { mosquitto__free(A); (A) = NULL;} while(0)
#define SAFE_FREE(A) do { free(A); (A) = NULL;} while(0)

#endif
