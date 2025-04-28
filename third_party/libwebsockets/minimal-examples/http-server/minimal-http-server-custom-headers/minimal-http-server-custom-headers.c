/*
 * lws-minimal-http-server-custom-headers
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

static int interrupted;

struct pss {
	char result[128 + LWS_PRE];
	int len;
};

/*
 * This just lets us override LWS_CALLBACK_HTTP handling before passing it
 * and everything else to the default handler.
 */

static int
callback_http(struct lws *wsi, enum lws_callback_reasons reason,
	      void *user, void *in, size_t len)
{
	uint8_t buf[LWS_PRE + 2048], *start = &buf[LWS_PRE], *p = start,
		*end = &buf[sizeof(buf) - 1];
	struct pss *pss = (struct pss *)user;
	char value[32], *pr = &pss->result[LWS_PRE];
	size_t e = sizeof(pss->result) - LWS_PRE;
	int n;

	switch (reason) {
	case LWS_CALLBACK_HTTP:
		/*
		 * LWS doesn't have the "DNT" header built-in.  But we can
		 * query it using the "custom" versions of the header apis.
		 *
		 * You can set your modern browser to issue DNT, look in the
		 * privacy settings of your browser.
		 */

		pss->len = 0;
		n = lws_hdr_custom_length(wsi, "dnt:", 4);
		if (n < 0)
			pss->len = lws_snprintf(pr, e,
					"%s: DNT header not found\n", __func__);
		else {

			pss->len = lws_snprintf(pr, e,
					"%s: DNT length %d<br>", __func__, n);
			n = lws_hdr_custom_copy(wsi, value, sizeof(value), "dnt:", 4);
			if (n < 0)
				pss->len += lws_snprintf(pr + pss->len, e - (unsigned int)pss->len,
					"%s: unable to get DNT value\n", __func__);
			else

				pss->len += lws_snprintf(pr + pss->len , e - (unsigned int)pss->len,
					"%s: DNT value '%s'\n", __func__, value);
		}

		lwsl_user("%s\n", pr);

		if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
				"text/html", (lws_filepos_t)pss->len, &p, end))
			return 1;

		if (lws_finalize_write_http_header(wsi, start, &p, end))
			return 1;


		/* write the body separately */
		lws_callback_on_writable(wsi);
		return 0;

	case LWS_CALLBACK_HTTP_WRITEABLE:

		strcpy((char *)start, "hello");

		if (lws_write(wsi, (uint8_t *)pr, (unsigned int)pss->len, LWS_WRITE_HTTP_FINAL) != pss->len)
			return 1;

		if (lws_http_transaction_completed(wsi))
			return -1;
		return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static struct lws_protocols protocols[] = {
	{ "http", callback_http, sizeof(struct pss), 0, 0, NULL, 0 },
	LWS_PROTOCOL_LIST_TERM
};

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
	/* .mount_next */		&mount_dyn,		/* linked-list "next" */
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
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal http server custom headers | visit http://localhost:7681\n");

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
	info.protocols = protocols;
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
	info.vhost_name = "https";

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
