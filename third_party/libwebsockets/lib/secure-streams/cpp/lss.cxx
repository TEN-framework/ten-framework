/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2020 Andy Green <andy@warmcat.com>
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
 *
 * C++ classes for Secure Streams
 */

#include <libwebsockets.hxx>

static const char *pcols[] = {
	"http://",	/* LWSSSP_H1 */
	"https://",
	"h2://",	/* LWSSSP_H2 */
	"h2s://",
	"ws://",	/* LWSSSP_WS */
	"wss://",
	"mqtt://",	/* LWSSSP_MQTT */
	"mqtts://",
	"raw://",	/* LWSSSP_RAW */
	"raws://",
};

static const uint8_t pcols_len[] = {
	7, 8, 5, 6, 5, 6, 7, 8, 6, 7
};

static const uint16_t pcols_port[] = {
	80, 443, 443, 443, 80, 443, 1883, 8883, 80, 443
};

lss::lss(lws_ctx_t _ctx, std::string _uri, lsscomp_t _comp, bool _psh,
	 lws_sscb_rx rx, lws_sscb_tx tx, lws_sscb_state state)
{
	const char *p, *urlpath;
	lws_ss_info_t ssi;
	int n, port;

	memset(&ssi, 0, sizeof(ssi));
	memset(&pol, 0, sizeof(pol));

	ctx		= _ctx;
	comp		= _comp;
	comp_done	= 0;
	rxlen		= 0;

	/*
	 * We have a common stub userdata, our "real" userdata is in the
	 * derived class members.   The Opaque user pointer points to the
	 * lss itself.
	 */

	ssi.handle_offset	    = offsetof(lssPriv, lssPriv::m_ss);
	ssi.opaque_user_data_offset = offsetof(lssPriv, lssPriv::m_plss);

	ssi.user_alloc	= sizeof(lssPriv);
	ssi.rx		= rx;
	ssi.tx		= tx;
	ssi.state	= state;
	ssi.policy	= &pol; /* we will provide our own policy */

	/*
	 * _uri is like "https://warmcat.com:443/index.html"... we need to
	 * deconstruct it into its policy implications
	 */

	uri = strdup(_uri.c_str());

	for (n = 0; n < LWS_ARRAY_SIZE(pcols); n++)
		if (!strncmp(uri, pcols[n], pcols_len[n]))
			break;

	if (n == LWS_ARRAY_SIZE(pcols))
		throw lssException("unknown uri protocol://");

	pol.protocol = n >> 1;
	if (n & 1)
		pol.flags |= LWSSSPOLF_TLS;

	n = pcols_port[n];

	if (lws_parse_uri(uri, &p, &pol.endpoint, &n, &urlpath))
		throw lssException("unable to parse uri://");

	pol.port = (uint16_t)n;

	if (pol.protocol <= LWSSSP_WS) {
		pol.u.http.url = urlpath;

		/*
		 * These are workarounds for common h2 server noncompliances
		 */

		pol.flags |= LWSSSPOLF_QUIRK_NGHTTP2_END_STREAM |
			     LWSSSPOLF_H2_QUIRK_OVERFLOWS_TXCR |
			     LWSSSPOLF_H2_QUIRK_UNCLEAN_HPACK_STATE;

		if (pol.protocol < LWSSSP_WS)
			pol.u.http.method = _psh ? "POST" : "GET";
	}

	us_start = lws_now_usecs();

	if (lws_ss_create(ctx, 0, &ssi, (void *)this, &m_ss, NULL, NULL))
		goto blow;

	if (pol.protocol <= LWSSSP_WS)
		lws_ss_client_connect(m_ss);

	return;

blow:
	if (uri)
		free(uri);
	throw lssException("ss creation failed");
}

lss::~lss()
{
	if (uri)
		free(uri);
	if (m_ss)
		lws_ss_destroy(&m_ss);
}

int lss::call_completion(lws_ss_constate_t state)
{
	if (comp_done)
		return 0;
	if (!comp)
		return 0;

	comp_done = 1;

	return comp(this, state, NULL);
}
