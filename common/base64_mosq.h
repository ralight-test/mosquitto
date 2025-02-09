/*
Copyright (c) 2012-2020 Roger Light <roger@atchoo.org>

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
#ifndef BASE64_MOSQ_H
#define BASE64_MOSQ_H

#ifdef __cplusplus
extern "C" {
#endif

int base64__encode(const unsigned char *in, size_t in_len, char **encoded);
int base64__decode(const char *in, unsigned char **decoded, unsigned int *decoded_len);

#ifdef __cplusplus
}
#endif

#endif
