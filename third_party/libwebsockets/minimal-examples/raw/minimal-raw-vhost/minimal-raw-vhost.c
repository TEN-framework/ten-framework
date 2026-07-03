/*
 * lws-minimal-raw-vhost
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates integrating a raw tcp listener into the lws event loop.
 *
 * This demo doesn't have any http or ws support.  You can connect to it
 * using netcat.  If you make multiple connections to it, things typed in one
 * netcat session are broadcast to all netcat connections.
 *
 * $ nc localhost 7681
 *
 * You can add more vhosts with things like http or ws support, it's as it is
 * for clarity.
 *
 * The main point is the apis and ways of managing raw sockets are almost
 * identical to http or ws mode sockets in lws.  The callback names for raw
 * wsi are changed to be specific to RAW mode is all.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct raw_pss {
	struct raw_pss *pss_list;
	struct lws *wsi;
};

/* one of these is created for each vhost our protocol is used with */

struct raw_vhd {
	struct raw_pss *pss_list; /* linked-list of live pss*/

	int len;
	uint8_t buf[4096];
};

static int
callback_raw_test(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct raw_pss *pss = (struct raw_pss *)user;
	struct raw_vhd *vhd = (struct raw_vhd *)lws_protocol_vh_priv_get(
				     lws_get_vhost(wsi), lws_get_protocol(wsi));

	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi), sizeof(struct raw_vhd));
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		break;

	/* callbacks related to raw socket descriptor */

        case LWS_CALLBACK_RAW_ADOPT:
		lwsl_user("LWS_CALLBACK_RAW_ADOPT\n");
		pss->wsi = wsi;
		lws_ll_fwd_insert(pss, pss_list, vhd->pss_list);
                break;

	case LWS_CALLBACK_RAW_CLOSE:
		lwsl_user("LWS_CALLBACK_RAW_CLOSE\n");
		lws_ll_fwd_remove(struct raw_pss, pss_list, pss, vhd->pss_list);
		break;

	case LWS_CALLBACK_RAW_RX:
		lwsl_user("LWS_CALLBACK_RAW_RX: %d\n", (int)len);
		vhd->len = (int)len;
		if (vhd->len > (int)sizeof(vhd->buf))
			vhd->len = sizeof(vhd->buf);
		memcpy(vhd->buf, in, (unsigned int)vhd->len);
		lws_start_foreach_llp(struct raw_pss **, ppss, vhd->pss_list) {
			lws_callback_on_writable((*ppss)->wsi);
		} lws_end_foreach_llp(ppss, pss_list);
		break;

	case LWS_CALLBACK_RAW_WRITEABLE:
		if (lws_write(wsi, vhd->buf, (unsigned int)vhd->len, LWS_WRITE_RAW) !=
		    vhd->len) {
			lwsl_notice("%s: raw write failed\n", __func__);
			return 1;
		}
		break;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static struct lws_protocols protocols[] = {
	{ "raw-test", callback_raw_test, sizeof(struct raw_pss), 0, 0, NULL, 0 },
	LWS_PROTOCOL_LIST_TERM
};

static int interrupted;

void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	const char *p;
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
			/* for LLL_ verbosity above NOTICE to be built into lws,
			 * lws must have been configured and built with
			 * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
			/* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
			/* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
			/* | LLL_DEBUG */;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal raw vhost | nc localhost 7681\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = 7681;
	info.protocols = protocols;
	info.options = LWS_SERVER_OPTION_ONLY_RAW; /* vhost accepts RAW */

#if defined(LWS_WITH_TLS)
	if (lws_cmdline_option(argc, argv, "-s")) {
		info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
		info.ssl_cert_filepath = "localhost-100y.cert";
		info.ssl_private_key_filepath = "localhost-100y.key";
	}
#endif

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);

	return 0;
}
