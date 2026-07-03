/*
 * ws protocol handler plugin for "client_loopback_test"
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The person who associated a work with this deed has dedicated
 * the work to the public domain by waiving all of his or her rights
 * to the work worldwide under copyright law, including all related
 * and neighboring rights, to the extent allowed by law. You can copy,
 * modify, distribute and perform the work, even for commercial purposes,
 * all without asking permission.
 *
 * These test plugins are intended to be adapted for use in your code, which
 * may be proprietary.  So unlike the library itself, they are licensed
 * Public Domain.
 */

#if !defined(LWS_DLL)
#define LWS_DLL
#endif
#if !defined(LWS_INTERNAL)
#define LWS_INTERNAL
#endif
#include <libwebsockets.h>
#include <string.h>

struct per_session_data__client_loopback_test {
	struct lws *wsi;
};

/*
 * This is a bit fiddly...
 *
 * 0) If you want the wss:// test to work, make sure the vhost is marked with
 *    enable-client-ssl if using lwsws, or call lws_init_vhost_client_ssl() on
 *    the vhost if you're doing it by hand.
 *
 * 1) enable the protocol on a vhost
 *
 *      "ws-protocols": [{
 *     "client-loopback-test": {
 *      "status": "ok"
 *     },  ...
 *
 *     the vhost should listen on 80 (ws://) or 443 (wss://)
 *
 * 2) mount the http part of the test one level down on the same vhost, eg
 *   {
 *      "mountpoint": "/c",
 *      "origin": "callback://client-loopback-test"
 *   }
 *
 * 3) Use a browser to visit the mountpoint with a URI attached for looping
 *    back, eg, if testing on localhost
 *
 *    http://localhost/c/ws://localhost
 *    https://localhost/c/wss://localhost
 *
 * 4) The HTTP part of this test protocol will try to do the requested
 *    ws client connection, to the same test protocol on the same
 *    server.
 */

static int
callback_client_loopback_test(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct lws_client_connect_info i;
	struct per_session_data__client_loopback_test *pss =
			(struct per_session_data__client_loopback_test *)user;
	const char *p = (const char *)in;
	char buf[100];
	int n;

	switch (reason) {

	/* HTTP part */

	case LWS_CALLBACK_HTTP:
		if (len < 10)
			return -1;

		p++;
		while (*p && *p != '/')
			p++;
		if (!*p) {
			lws_return_http_status(wsi, 400, "Arg needs to be in format ws://xxx or wss://xxx");
			return -1;
		}
		p++;

		memset(&i, 0, sizeof(i));
		i.context = lws_get_context(wsi);

		// stacked /// get resolved to /

		if (strncmp(p, "ws:/", 4) == 0) {
			i.ssl_connection = 0;
			i.port = 80;
			p += 4;
		} else
			if (strncmp(p, "wss:/", 5) == 0) {
				i.port = 443;
				i.ssl_connection = 1;
				p += 5;
			} else {
				sprintf(buf, "Arg %s is not in format ws://xxx or wss://xxx\n", p);
				lws_return_http_status(wsi, 400, buf);
				return -1;
			}

		i.address = p;
		i.path = "";
		i.host = p;
		i.origin = p;
		i.ietf_version_or_minus_one = -1;
		i.protocol = "client-loopback-test";

		pss->wsi = lws_client_connect_via_info(&i);
		if (!pss->wsi)
			lws_return_http_status(wsi, 401, "client-loopback-test: connect failed\n");
		else {
			lwsl_notice("client connection to %s:%d with ssl: %d started\n",
				    i.address, i.port, i.ssl_connection);
			lws_return_http_status(wsi, 200, "OK");
		}

		/* either way, close the triggering http link */

		return -1;

	case LWS_CALLBACK_CLOSED_HTTP:
		lwsl_notice("Http part closed\n");
		break;

	/* server part */

	case LWS_CALLBACK_ESTABLISHED:
		lwsl_notice("server part: LWS_CALLBACK_ESTABLISHED\n");
		strcpy(buf + LWS_PRE, "Made it");
		n = lws_write(wsi, (unsigned char *)buf + LWS_PRE,
			      7, LWS_WRITE_TEXT);
		if (n < 7)
			return -1;
		break;

	/* client part */

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lwsl_notice("Client connection established\n");
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		lws_strncpy(buf, in, sizeof(buf));
		lwsl_notice("Client connection received %ld from server '%s'\n",
			    (long)len, buf);

		/* OK we are done with the client connection */
		return -1;

	default:
		break;
	}

	return 0;
}

LWS_VISIBLE const struct lws_protocols client_loopback_test_protocols[] = {
	{
		"client-loopback-test",
		callback_client_loopback_test,
		sizeof(struct per_session_data__client_loopback_test),
		1024, /* rx buf size must be >= permessage-deflate rx size */
		0, NULL, 0
	},
};

LWS_VISIBLE const lws_plugin_protocol_t client_loopback_test = {
	.hdr = {
		"client loopback test",
		"lws_protocol_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.protocols = client_loopback_test_protocols,
	.count_protocols = LWS_ARRAY_SIZE(client_loopback_test_protocols),
	.extensions = NULL,
	.count_extensions = 0,
};
