/*
 * lws-minimal-raw-fallback http-server
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates the most minimal http server you can make with lws.
 *
 * To keep it simple, it serves stuff from the subdirectory 
 * "./mount-origin" of the directory it was started in.
 * You can change that by changing mount.origin below.
 *
 * In addition, if the connection does to seem to be talking http, then it
 * falls back to a raw echo protocol.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

struct pss__raw_echo {
	uint8_t buf[2048];
	int len;
};

static int interrupted;

static const struct lws_http_mount mount = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */			"./mount-origin", /* serve from dir */
	/* .def */			"index.html",	/* default filename */
	/* .protocol */			NULL,
	/* .cgienv */			NULL,
	/* .extra_mimetypes */		NULL,
	/* .interpret */		NULL,
	/* .cgi_timeout */		0,
	/* .cache_max_age */		0,
	/* .auth_mask */		0,
	/* .cache_reusable */		0,
	/* .cache_revalidate */		0,
	/* .cache_intermediaries */	0,
	/* .origin_protocol */		LWSMPRO_FILE,	/* files in a dir */
	/* .mountpoint_len */		1,		/* char count */
	/* .basic_auth_login_file */	NULL,
};

static int
callback_raw_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user,
		  void *in, size_t len)
{
	struct pss__raw_echo *pss = (struct pss__raw_echo *)user;

	switch (reason) {
	case LWS_CALLBACK_RAW_ADOPT:
		lwsl_notice("LWS_CALLBACK_RAW_ADOPT\n");
		break;

	case LWS_CALLBACK_RAW_RX:
		lwsl_notice("LWS_CALLBACK_RAW_RX %ld\n", (long)len);
		if (len > sizeof(pss->buf))
			len = sizeof(pss->buf);
		memcpy(pss->buf, in, len);
		pss->len = (int)len;
		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_RAW_CLOSE:
		lwsl_notice("LWS_CALLBACK_RAW_CLOSE\n");
		break;

	case LWS_CALLBACK_RAW_WRITEABLE:
		lwsl_notice("LWS_CALLBACK_RAW_WRITEABLE\n");
		lws_write(wsi, pss->buf, (unsigned int)pss->len, LWS_WRITE_HTTP);
		break;
	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
	{ "raw-echo", callback_raw_echo, sizeof(struct pss__raw_echo), 2048, 0, NULL, 0 },
	LWS_PROTOCOL_LIST_TERM
};

void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	const char *p;
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal raw fallback http server | "
		  "visit http://localhost:7681\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = 7681;
	info.protocols = protocols;
	info.mounts = &mount;
	info.error_document_404 = "/404.html";
	info.options =
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE |
		LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG;
	info.listen_accept_role = "raw-skt";
	info.listen_accept_protocol = "raw-echo";

#if defined(LWS_WITH_TLS)
	if (lws_cmdline_option(argc, argv, "-s")) {
		info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
				LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT;
		info.ssl_cert_filepath = "localhost-100y.cert";
		info.ssl_private_key_filepath = "localhost-100y.key";

		if (lws_cmdline_option(argc, argv, "-u"))
			info.options |= LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS;

		if (lws_cmdline_option(argc, argv, "-h"))
			info.options |= LWS_SERVER_OPTION_ALLOW_HTTP_ON_HTTPS_LISTENER;
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
