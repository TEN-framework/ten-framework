/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
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

#if defined(LWS_CLIENT_HTTP_PROXYING)

int
lws_set_proxy(struct lws_vhost *vhost, const char *proxy)
{
	char authstring[96];
	int brackets = 0;
	char *p;

	if (!proxy)
		return -1;

	/* we have to deal with a possible redundant leading http:// */
	if (!strncmp(proxy, "http://", 7))
		proxy += 7;

	p = strrchr(proxy, '@');
	if (p) { /* auth is around */

		if (lws_ptr_diff_size_t(p, proxy) > sizeof(authstring) - 1)
			goto auth_too_long;

		lws_strncpy(authstring, proxy, lws_ptr_diff_size_t(p, proxy) + 1);
		// null termination not needed on input
		if (lws_b64_encode_string(authstring, lws_ptr_diff(p, proxy),
				vhost->proxy_basic_auth_token,
		    sizeof vhost->proxy_basic_auth_token) < 0)
			goto auth_too_long;

		lwsl_vhost_info(vhost, " Proxy auth in use");

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
		proxy = p + 1;
#endif
	} else
		vhost->proxy_basic_auth_token[0] = '\0';

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)

#if defined(LWS_WITH_IPV6)
	/*
	 * isolating the address / port is complicated by IPv6 overloading
	 * the meaning of : in the address.  The convention to solve it is to
	 * put [] around the ipv6 address part, eg, "[::1]:443".  This must be
	 * parsed to "::1" as the address and the port as 443.
	 *
	 * IPv4 addresses like myproxy:443 continue to be parsed as normal.
	 */

	if (proxy[0] == '[')
		brackets = 1;
#endif

	lws_strncpy(vhost->http.http_proxy_address, proxy + brackets,
		    sizeof(vhost->http.http_proxy_address));

	p = vhost->http.http_proxy_address;

#if defined(LWS_WITH_IPV6)
	if (brackets) {
		/* original is IPv6 format "[::1]:443" */

		p = strchr(vhost->http.http_proxy_address, ']');
		if (!p) {
			lwsl_vhost_err(vhost, "malformed proxy '%s'", proxy);

			return -1;
		}
		*p++ = '\0';
	}
#endif

	p = strchr(p, ':');
	if (!p && !vhost->http.http_proxy_port) {
		lwsl_vhost_err(vhost, "http_proxy needs to be ads:port");

		return -1;
	}
	if (p) {
		*p = '\0';
		vhost->http.http_proxy_port = (unsigned int)atoi(p + 1);
	}

	lwsl_vhost_info(vhost, " Proxy %s:%u", vhost->http.http_proxy_address,
					    vhost->http.http_proxy_port);
#endif

	return 0;

auth_too_long:
	lwsl_vhost_err(vhost, "proxy auth too long");

	return -1;
}
#endif
