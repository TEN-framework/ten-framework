/*
 * lws-minimal-http-server-dynamic
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a minimal http server that can produce dynamic http
 * content as well as static content.
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

/*
 * Unlike ws, http is a stateless protocol.  This pss only exists for the
 * duration of a single http transaction.  With http/1.1 keep-alive and http/2,
 * that is unrelated to (shorter than) the lifetime of the network connection.
 */
struct pss {
	char path[128];

	int times;
	int budget;

	int content_lines;
};

static int interrupted;

static int
callback_dynamic_http(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct pss *pss = (struct pss *)user;
	uint8_t buf[LWS_PRE + 2048], *start = &buf[LWS_PRE], *p = start,
		*end = &buf[sizeof(buf) - 1];
	time_t t;
	int n;
#if defined(LWS_HAVE_CTIME_R)
	char date[32];
#endif

	switch (reason) {
	case LWS_CALLBACK_HTTP:

		/*
		 * If you want to know the full url path used, you can get it
		 * like this
		 *
		 * n = lws_hdr_copy(wsi, buf, sizeof(buf), WSI_TOKEN_GET_URI);
		 *
		 * The base path is the first (n - strlen((const char *)in))
		 * chars in buf.
		 */

		/*
		 * In contains the url part after the place the mount was
		 * positioned at, eg, if positioned at "/dyn" and given
		 * "/dyn/mypath", in will contain /mypath
		 */
		lws_snprintf(pss->path, sizeof(pss->path), "%s",
				(const char *)in);

		lws_get_peer_simple(wsi, (char *)buf, sizeof(buf));
		lwsl_notice("%s: HTTP: connection %s, path %s\n", __func__,
				(const char *)buf, pss->path);

		/*
		 * Demonstrates how to retreive a urlarg x=value
		 */

		{
			char value[100];
			int z = lws_get_urlarg_by_name_safe(wsi, "x", value,
					   sizeof(value) - 1);

			if (z >= 0)
				lwsl_hexdump_notice(value, (size_t)z);
		}

		/*
		 * prepare and write http headers... with regards to content-
		 * length, there are three approaches:
		 *
		 *  - http/1.0 or connection:close: no need, but no pipelining
		 *  - http/1.1 or connected:keep-alive
		 *     (keep-alive is default for 1.1): content-length required
		 *  - http/2: no need, LWS_WRITE_HTTP_FINAL closes the stream
		 *
		 * giving the api below LWS_ILLEGAL_HTTP_CONTENT_LEN instead of
		 * a content length forces the connection response headers to
		 * send back "connection: close", disabling keep-alive.
		 *
		 * If you know the final content-length, it's always OK to give
		 * it and keep-alive can work then if otherwise possible.  But
		 * often you don't know it and avoiding having to compute it
		 * at header-time makes life easier at the server.
		 */
		if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
				"text/html",
				LWS_ILLEGAL_HTTP_CONTENT_LEN, /* no content len */
				&p, end))
			return 1;
		if (lws_finalize_write_http_header(wsi, start, &p, end))
			return 1;

		pss->times = 0;
		pss->budget = atoi((char *)in + 1);
		pss->content_lines = 0;
		if (!pss->budget)
			pss->budget = 10;

		/* write the body separately */
		lws_callback_on_writable(wsi);

		return 0;

	case LWS_CALLBACK_HTTP_WRITEABLE:

		if (!pss || pss->times > pss->budget)
			break;

		/*
		 * We send a large reply in pieces of around 2KB each.
		 *
		 * For http/1, it's possible to send a large buffer at once,
		 * but lws will malloc() up a temp buffer to hold any data
		 * that the kernel didn't accept in one go.  This is expensive
		 * in memory and cpu, so it's better to stage the creation of
		 * the data to be sent each time.
		 *
		 * For http/2, large data frames would block the whole
		 * connection, not just the stream and are not allowed.  Lws
		 * will call back on writable when the stream both has transmit
		 * credit and the round-robin fair access for sibling streams
		 * allows it.
		 *
		 * For http/2, we must send the last part with
		 * LWS_WRITE_HTTP_FINAL to close the stream representing
		 * this transaction.
		 */
		n = LWS_WRITE_HTTP;
		if (pss->times == pss->budget)
			n = LWS_WRITE_HTTP_FINAL;

		if (!pss->times) {
			/*
			 * the first time, we print some html title
			 */
			t = time(NULL);
			/*
			 * to work with http/2, we must take care about LWS_PRE
			 * valid behind the buffer we will send.
			 */
			p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "<html>"
				"<head><meta charset=utf-8 "
				"http-equiv=\"Content-Language\" "
				"content=\"en\"/></head><body>"
				"<img src=\"/libwebsockets.org-logo.svg\">"
				"<br>Dynamic content for '%s' from mountpoint."
				"<br>Time: %s<br><br>"
				"</body></html>", pss->path,
#if defined(LWS_HAVE_CTIME_R)
				ctime_r(&t, date));
#else
				ctime(&t));
#endif
		} else {
			/*
			 * after the first time, we create bulk content.
			 *
			 * Again we take care about LWS_PRE valid behind the
			 * buffer we will send.
			 */

			while (lws_ptr_diff(end, p) > 80)
				p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
					"%d.%d: this is some content... ",
					pss->times, pss->content_lines++);

			p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "<br><br>");
		}

		pss->times++;
		if (lws_write(wsi, (uint8_t *)start, lws_ptr_diff_size_t(p, start), (enum lws_write_protocol)n) !=
				lws_ptr_diff(p, start))
			return 1;

		/*
		 * HTTP/1.0 no keepalive: close network connection
		 * HTTP/1.1 or HTTP1.0 + KA: wait / process next transaction
		 * HTTP/2: stream ended, parent connection remains up
		 */
		if (n == LWS_WRITE_HTTP_FINAL) {
		    if (lws_http_transaction_completed(wsi))
			return -1;
		} else
			lws_callback_on_writable(wsi);

		return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols defprot =
	{ "defprot", lws_callback_http_dummy, 0, 0, 0, NULL, 0 }, protocol =
	{ "http", callback_dynamic_http, sizeof(struct pss), 0, 0, NULL, 0 };

static const struct lws_protocols *pprotocols[] = { &defprot, &protocol, NULL };

/* override the default mount for /dyn in the URL space */

static const struct lws_http_mount mount_dyn = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/dyn",		/* mountpoint URL */
	/* .origin */			NULL,	/* protocol */
	/* .def */			NULL,
	/* .protocol */			"http",
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
	/* .mountpoint_len */		4,		/* char count */
	/* .basic_auth_login_file */	NULL,
};

/* default mount serves the URL space from ./mount-origin */

static const struct lws_http_mount mount = {
	/* .mount_next */	&mount_dyn,		/* linked-list "next" */
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */		"./mount-origin",	/* serve from dir */
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
	lwsl_user("LWS minimal http server dynamic | visit http://localhost:7681\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
		       LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	/* for testing ah queue, not useful in real world */
	if (lws_cmdline_option(argc, argv, "--ah1"))
		info.max_http_header_pool = 1;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	/* http on 7681 */

	info.port = 7681;
	info.pprotocols = pprotocols;
	info.mounts = &mount;
	info.vhost_name = "http";

	if (!lws_create_vhost(context, &info)) {
		lwsl_err("Failed to create tls vhost\n");
		goto bail;
	}

	/* https on 7682 */

	info.port = 7682;
	info.error_document_404 = "/404.html";
#if defined(LWS_WITH_TLS)
	info.ssl_cert_filepath = "localhost-100y.cert";
	info.ssl_private_key_filepath = "localhost-100y.key";
#endif
	info.vhost_name = "localhost";

	if (!lws_create_vhost(context, &info)) {
		lwsl_err("Failed to create tls vhost\n");
		goto bail;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

bail:
	lws_context_destroy(context);

	return 0;
}
