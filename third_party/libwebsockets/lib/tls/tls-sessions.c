/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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

void
lws_tls_session_tag_discrete(const char *vhname, const char *host,
			      uint16_t port, char *buf, size_t len)
{
	/*
	 * We have to include the vhost name in the session tag, since
	 * different vhosts may make connections to the same endpoint using
	 * different client certs.
	 */

	lws_snprintf(buf, len, "%s_%s_%u", vhname, host, port);
}

int
lws_tls_session_tag_from_wsi(struct lws *wsi, char *buf, size_t len)
{
	const char *host;

	if (!wsi)
		return 1;

	if (!wsi->stash)
		return 1;

	host = wsi->stash->cis[CIS_HOST];
	if (!host)
		host = wsi->stash->cis[CIS_ADDRESS];

	if (!host)
		return 1;

	lws_tls_session_tag_discrete(wsi->a.vhost->name, host, wsi->c_port,
				     buf, len);

	return 0;
}


