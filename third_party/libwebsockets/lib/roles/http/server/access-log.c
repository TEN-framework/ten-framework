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

/*
 * Produce Apache-compatible log string for wsi, like this:
 *
 * 2.31.234.19 - - [27/Mar/2016:03:22:44 +0800]
 * "GET /aep-screen.png HTTP/1.1"
 * 200 152987 "https://libwebsockets.org/index.html"
 * "Mozilla/5.0 (Macint... Chrome/49.0.2623.87 Safari/537.36"
 *
 */

extern const char * const method_names[];

static const char * const hver[] = {
	"HTTP/1.0", "HTTP/1.1", "HTTP/2"
};

void
lws_prepare_access_log_info(struct lws *wsi, char *uri_ptr, int uri_len, int meth)
{
	char da[64], uri[256], ta[64];
	time_t t = time(NULL);
	struct lws *nwsi;
	const char *me;
	int l = 256, m;
	struct tm *ptm = NULL;
#if defined(LWS_HAVE_LOCALTIME_R)
	struct tm tm;
#endif

	if (!wsi->a.vhost)
		return;

	/* only worry about preparing it if we store it */
	if (wsi->a.vhost->log_fd == (int)LWS_INVALID_FILE)
		return;

	if (wsi->access_log_pending)
		lws_access_log(wsi);

	wsi->http.access_log.header_log = lws_malloc((unsigned int)l, "access log");
	if (!wsi->http.access_log.header_log)
		return;

#if defined(LWS_HAVE_LOCALTIME_R)
	ptm = localtime_r(&t, &tm);
#else
	ptm = localtime(&t);
#endif
	if (ptm)
		strftime(da, sizeof(da), "%d/%b/%Y:%H:%M:%S %z", ptm);
	else
		strcpy(da, "01/Jan/1970:00:00:00 +0000");

#if defined(LWS_ROLE_H2)
	if (wsi->mux_substream)
		me = lws_hdr_simple_ptr(wsi, WSI_TOKEN_HTTP_COLON_METHOD);
	else
#endif
		me = method_names[meth];

	if (!me)
		me = "(null)";

	m = uri_len;
	if (m > (int)sizeof(uri) - 1)
		m = sizeof(uri) - 1;

	strncpy(uri, uri_ptr, (unsigned int)m);
	uri[m] = '\0';

	nwsi = lws_get_network_wsi(wsi);

	if (nwsi->sa46_peer.sa4.sin_family)
		lws_sa46_write_numeric_address(&nwsi->sa46_peer, ta, sizeof(ta));
	else
		strncpy(ta, "unknown", sizeof(ta));

	lws_snprintf(wsi->http.access_log.header_log, (size_t)l,
		     "%s - - [%s] \"%s %s %s\"",
		     ta, da, me, uri, hver[wsi->http.request_version]);

	//lwsl_notice("%s\n", wsi->http.access_log.header_log);

	l = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_USER_AGENT);
	if (l) {
		wsi->http.access_log.user_agent =
				lws_malloc((unsigned int)l + 5, "access log");
		if (!wsi->http.access_log.user_agent) {
			lwsl_err("OOM getting user agent\n");
			lws_free_set_NULL(wsi->http.access_log.header_log);
			return;
		}
		wsi->http.access_log.user_agent[0] = '\0';

		if (lws_hdr_copy(wsi, wsi->http.access_log.user_agent, l + 4,
				 WSI_TOKEN_HTTP_USER_AGENT) >= 0)
			for (m = 0; m < l; m++)
				if (wsi->http.access_log.user_agent[m] == '\"')
					wsi->http.access_log.user_agent[m] = '\'';
	}
	l = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_REFERER);
	if (l) {
		wsi->http.access_log.referrer = lws_malloc((unsigned int)l + 5, "referrer");
		if (!wsi->http.access_log.referrer) {
			lwsl_err("OOM getting referrer\n");
			lws_free_set_NULL(wsi->http.access_log.user_agent);
			lws_free_set_NULL(wsi->http.access_log.header_log);
			return;
		}
		wsi->http.access_log.referrer[0] = '\0';
		if (lws_hdr_copy(wsi, wsi->http.access_log.referrer,
				l + 4, WSI_TOKEN_HTTP_REFERER) >= 0)

			for (m = 0; m < l; m++)
				if (wsi->http.access_log.referrer[m] == '\"')
					wsi->http.access_log.referrer[m] = '\'';
	}
	wsi->access_log_pending = 1;
}


int
lws_access_log(struct lws *wsi)
{
	char *p = wsi->http.access_log.user_agent, ass[512],
	     *p1 = wsi->http.access_log.referrer;
	int l;

	if (!wsi->a.vhost)
		return 0;

	if (wsi->a.vhost->log_fd == (int)LWS_INVALID_FILE)
		return 0;

	if (!wsi->access_log_pending)
		return 0;

	if (!wsi->http.access_log.header_log)
		return 0;

	if (!p)
		p = "";

	if (!p1)
		p1 = "";

	/*
	 * We do this in two parts to restrict an oversize referrer such that
	 * we will always have space left to append an empty useragent, while
	 * maintaining the structure of the log text
	 */
	l = lws_snprintf(ass, sizeof(ass) - 7, "%s %d %lu \"%s",
			 wsi->http.access_log.header_log,
			 wsi->http.access_log.response,
			 wsi->http.access_log.sent, p1);
	if (strlen(p) > sizeof(ass) - 6 - (unsigned int)l) {
		p[sizeof(ass) - 6 - (unsigned int)l] = '\0';
		l--;
	}
	l += lws_snprintf(ass + (unsigned int)l, sizeof(ass) - 1 - (unsigned int)l, "\" \"%s\"\n", p);

	ass[sizeof(ass) - 1] = '\0';

	if ((int)write(wsi->a.vhost->log_fd, ass, (size_t)l) != l)
		lwsl_err("Failed to write log\n");

	if (wsi->http.access_log.header_log) {
		lws_free(wsi->http.access_log.header_log);
		wsi->http.access_log.header_log = NULL;
	}
	if (wsi->http.access_log.user_agent) {
		lws_free(wsi->http.access_log.user_agent);
		wsi->http.access_log.user_agent = NULL;
	}
	if (wsi->http.access_log.referrer) {
		lws_free(wsi->http.access_log.referrer);
		wsi->http.access_log.referrer = NULL;
	}
	wsi->access_log_pending = 0;

	return 0;
}

