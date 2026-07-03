/*
 * lws-minimal-http-server-sse
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a minimal http server that can serve both normal static
 * content and server-side event connections.
 *
 * To keep it simple, it serves the static stuff from the subdirectory
 * "./mount-origin" of the directory it was started in.
 *
 * You can change that by changing mount.origin below.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#if defined(WIN32)
#define HAVE_STRUCT_TIMESPEC
#if defined(pid_t)
#undef pid_t
#endif
#endif
#include <pthread.h>

/*
 * Unlike ws, http is a stateless protocol.  This pss only exists for the
 * duration of a single http transaction.  With http/1.1 keep-alive and http/2,
 * that is unrelated to (shorter than) the lifetime of the network connection.
 */
struct pss {
	time_t established;
};

static int interrupted;

#define SECS_REPORT 3

static int
callback_sse(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	     void *in, size_t len)
{
	struct pss *pss = (struct pss *)user;
	uint8_t buf[LWS_PRE + LWS_RECOMMENDED_MIN_HEADER_SPACE], *start = &buf[LWS_PRE],
		*p = start, *end = &buf[sizeof(buf) - 1];

	switch (reason) {
	case LWS_CALLBACK_HTTP:
		/*
		 * `in` contains the url part after our mountpoint /sse, if any
		 * you can use this to determine what data to return and store
		 * that in the pss
		 */
		lwsl_notice("%s: LWS_CALLBACK_HTTP: '%s'\n", __func__,
			    (const char *)in);

		pss->established = time(NULL);

		/* SSE requires a response with this content-type */

		if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
						"text/event-stream",
						LWS_ILLEGAL_HTTP_CONTENT_LEN,
						&p, end))
			return 1;

		if (lws_finalize_write_http_header(wsi, start, &p, end))
			return 1;

		/*
		 * This tells lws we are no longer a normal http stream,
		 * but are an "immortal" (plus or minus whatever timeout you
		 * set on it afterwards) SSE stream.  In http/2 case that also
		 * stops idle timeouts being applied to the network connection
		 * while this wsi is still open.
		 */
		lws_http_mark_sse(wsi);

		/* write the body separately */

		lws_callback_on_writable(wsi);

		return 0;

	case LWS_CALLBACK_HTTP_WRITEABLE:

		lwsl_notice("%s: LWS_CALLBACK_HTTP_WRITEABLE\n", __func__);

		if (!pss)
			break;

		/*
		 * to keep this demo as simple as possible, each client has his
		 * own private data and timer.
		 */

		p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
				  "data: %llu\x0d\x0a\x0d\x0a",
				  (unsigned long long)(time(NULL) -
				  pss->established));

		if (lws_write(wsi, (uint8_t *)start, lws_ptr_diff_size_t(p, start),
			      LWS_WRITE_HTTP) != lws_ptr_diff(p, start))
			return 1;

		lws_set_timer_usecs(wsi, SECS_REPORT * LWS_USEC_PER_SEC);

		return 0;

	case LWS_CALLBACK_TIMER:

		lwsl_notice("%s: LWS_CALLBACK_TIMER\n", __func__);
		lws_callback_on_writable(wsi);

		return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static struct lws_protocols protocols[] = {
	{ "http", lws_callback_http_dummy, 0, 0, 0, NULL, 0 },
	{ "sse", callback_sse, sizeof(struct pss), 0, 0, NULL, 0 },
	LWS_PROTOCOL_LIST_TERM
};

/* override the default mount for /sse in the URL space */

static const struct lws_http_mount mount_sse = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/sse",		/* mountpoint URL */
	/* .origin */			NULL,		/* protocol */
	/* .def */			NULL,
	/* .protocol */			"sse",
	/* .cgienv */			NULL,
	/* .extra_mimetypes */		NULL,
	/* .interpret */		NULL,
	/* .cgi_timeout */		0,
	/* .cache_max_age */		0,
	/* .auth_mask */		0,
	/* .cache_reusable */		0,
	/* .cache_revalidate */		0,
	/* .cache_intermediaries */	0,
	/* .origin_protocol */		LWSMPRO_CALLBACK, /* dynamic */
	/* .mountpoint_len */		4,		  /* char count */
	/* .basic_auth_login_file */	NULL,
};

/* default mount serves the URL space from ./mount-origin */

static const struct lws_http_mount mount = {
	/* .mount_next */		&mount_sse,	/* linked-list "next" */
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
	lwsl_user("LWS minimal http Server-Side Events | visit http://localhost:7681\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */

	info.protocols = protocols;
	info.mounts = &mount;
	info.options =
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
	info.port = 7681;

#if defined(LWS_WITH_TLS)
	if (lws_cmdline_option(argc, argv, "-s")) {
		info.port = 443;
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
