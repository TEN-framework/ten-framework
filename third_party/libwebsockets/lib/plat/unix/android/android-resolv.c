/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2019 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "private-lib-core.h"
#include <sys/system_properties.h>

lws_async_dns_server_check_t
lws_plat_asyncdns_init(struct lws_context *context, lws_sockaddr46 *sa46)
{
	char d[PROP_VALUE_MAX], *p;
	uint32_t ip32;
	uint8_t i[4];
	int n;

	d[0] = '\0';
	if (__system_property_get("net.dns1", d) <= 0)
		return LADNS_CONF_SERVER_UNKNOWN;

	for (n = 0; n < 4; n++) {
		i[n] = (uint8_t)atoi(d);
		p = strchr(d, '.');
		if (n != 3 && !p)
			return LADNS_CONF_SERVER_UNKNOWN;
	}

	ip32 = (uint32_t) ((i[0] << 24) | (i[1] << 16) | (i[2] << 8) | i[3]);
	n = ip32 == sa46->sa4.sin_addr.s_addr;
	sa46->sa4.sin_family = AF_INET;
	sa46->sa4.sin_addr.s_addr = ip32;

	return n ? LADNS_CONF_SERVER_SAME : LADNS_CONF_SERVER_CHANGED;
}

