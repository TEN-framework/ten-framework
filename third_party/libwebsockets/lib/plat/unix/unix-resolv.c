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

lws_async_dns_server_check_t
lws_plat_asyncdns_init(struct lws_context *context, lws_sockaddr46 *sa46)
{
	lws_async_dns_server_check_t s = LADNS_CONF_SERVER_CHANGED;
	lws_sockaddr46 sa46t;
	lws_tokenize_t ts;
	char ads[48], *r;
	int fd, ns = 0;
	ssize_t n;

	r = (char *)context->pt[0].serv_buf;

	/* grab the first chunk of /etc/resolv.conf */

	fd = open("/etc/resolv.conf", LWS_O_RDONLY);
	if (fd < 0)
		return LADNS_CONF_SERVER_UNKNOWN;

	n = read(fd, r, context->pt_serv_buf_size - 1);
	close(fd);
	if (n < 0)
		return LADNS_CONF_SERVER_UNKNOWN;

	r[n] = '\0';
	lws_tokenize_init(&ts, r, LWS_TOKENIZE_F_DOT_NONTERM |
				  LWS_TOKENIZE_F_NO_FLOATS |
				  LWS_TOKENIZE_F_NO_INTEGERS |
				  LWS_TOKENIZE_F_MINUS_NONTERM |
				  LWS_TOKENIZE_F_HASH_COMMENT);
	do {
		ts.e = (int8_t)lws_tokenize(&ts);
		if (ts.e != LWS_TOKZE_TOKEN) {
			ns = 0;
			continue;
		}

		if (!ns && !strncmp("nameserver", ts.token, ts.token_len)) {
			ns = 1;
			continue;
		}
		if (!ns)
			continue;

		/* we are a token just after the "nameserver" token */

		ns = 0;
		if (ts.token_len > (int)sizeof(ads) - 1)
			continue;

		memcpy(ads, ts.token, ts.token_len);
		ads[ts.token_len] = '\0';
		if (lws_sa46_parse_numeric_address(ads, &sa46t) < 0)
			continue;

		if (!lws_sa46_compare_ads(sa46, &sa46t))
			s = LADNS_CONF_SERVER_SAME;

		*sa46 = sa46t;

		return s;

	} while (ts.e > 0);

	return LADNS_CONF_SERVER_UNKNOWN;
}
