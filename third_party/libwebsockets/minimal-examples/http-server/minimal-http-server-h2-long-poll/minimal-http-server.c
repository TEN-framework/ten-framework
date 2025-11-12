/*
 * lws-minimal-http-server-h2-long-poll
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates an h2 server that supports "long poll"
 * immortal client connections.  For simplicity it doesn't serve
 * any regular files, you can add a mount to do it if you want.
 *
 * The protocol keeps the long poll h2 stream open, and sends
 * the time on the stream once per minute.  Normally idle h2
 * connections are closed by default within 30s, so this demonstrates
 * the stream and network connection are operating as "immortal"
 * on both sides.
 *
 * See http-client/minimal-http-client-h2-long-poll for the
 * client example that connects and transitions the stream to the
 * immortal long poll mode.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

static int interrupted;

struct pss {
	struct lws *wsi;
	lws_sorted_usec_list_t sul;
	char pending;
};

static const lws_retry_bo_t retry = {
	.secs_since_valid_ping = 5,
	.secs_since_valid_hangup = 10,
};

static void
sul_cb(lws_sorted_usec_list_t *sul)
{
	struct pss *pss = (struct pss *)lws_container_of(sul, struct pss, sul);

	pss->pending = 1;
	lws_callback_on_writable(pss->wsi);
	/* interval 1min... longer than any normal timeout */
	lws_sul_schedule(lws_get_context(pss->wsi), 0, &pss->sul, sul_cb,
				60 * LWS_US_PER_SEC);
}

static int
callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	      void *in, size_t len)
{
	struct pss * pss = (struct pss *)user;
	uint8_t buf[LWS_PRE + LWS_RECOMMENDED_MIN_HEADER_SPACE],
		*start = &buf[LWS_PRE], *p = start,
		*end = buf + sizeof(buf) - 1;
	int m, n;

	switch (reason) {
	case LWS_CALLBACK_HTTP:
		lwsl_user("%s: connect\n", __func__);
		pss->wsi = wsi;

		if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
				"text/html",
				LWS_ILLEGAL_HTTP_CONTENT_LEN, /* no content len */
				&p, end))
			return 1;
		if (lws_finalize_write_http_header(wsi, start, &p, end))
			return 1;

		sul_cb(&pss->sul);
		return 0;

	case LWS_CALLBACK_CLOSED_HTTP:
		if (!pss)
			break;
		lws_sul_cancel(&pss->sul);
		break;

	case LWS_CALLBACK_HTTP_WRITEABLE:
		if (!pss->pending)
			break;
		n = lws_snprintf((char *)p, sizeof(buf) - LWS_PRE, "%llu",
				 (unsigned long long)lws_now_usecs());
		m = lws_write(wsi, p, (unsigned int)n, LWS_WRITE_HTTP);
		if (m < n) {
			lwsl_err("ERROR %d writing to socket\n", n);
			return -1;
		}
		break;
	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static struct lws_protocols protocols[] = {
	{ "http", callback_http, sizeof(struct pss), 0, 0, NULL, 0 },
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
	lwsl_user("LWS minimal http server h2 long poll\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = 7681;
#if defined(LWS_WITH_TLS)
	info.ssl_cert_filepath = "localhost-100y.cert";
	info.ssl_private_key_filepath = "localhost-100y.key";
#endif
	info.protocols = protocols;
	info.options =
		LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
		LWS_SERVER_OPTION_VH_H2_HALF_CLOSED_LONG_POLL |
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	/* the default validity check is 5m / 5m10s... -v = 5s / 10s */

	if (lws_cmdline_option(argc, argv, "-v"))
		info.retry_and_idle_policy = &retry;

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
