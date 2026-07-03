/*
 * lws-minimal-http-server-form-get
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a minimal http server that performs a form GET with a couple
 * of parameters.  It dumps the parameters to the console log and redirects
 * to another page.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

static int interrupted;

static const char * param_names[] = {
	"text1",
	"send"
};

static int
callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	      void *in, size_t len)
{
	uint8_t buf[LWS_PRE + LWS_RECOMMENDED_MIN_HEADER_SPACE],
		*start = &buf[LWS_PRE], *p = start,
		*end = &buf[sizeof(buf) - 1];
	int n;

	switch (reason) {
	case LWS_CALLBACK_HTTP:

		if (!lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI))
			/* not a GET */
			break;
		lwsl_err("%s: %s\n", __func__, (const char *)in);
		if (strcmp((const char *)in, "/form1"))
			/* not our form URL */
			break;

		/* we just dump the decoded things to the log */

		for (n = 0; n < (int)LWS_ARRAY_SIZE(param_names); n++) {
			int rv = lws_get_urlarg_by_name_safe(wsi, param_names[n],
					(char *)buf, sizeof(buf));
			if (rv < 0)
				lwsl_user("%s: undefined\n", param_names[n]);
			else
				lwsl_user("%s: (len %d) '%s'\n", param_names[n],
					  (int)rv, buf);
		}

		/*
		 * Our response is to redirect to a static page.  We could
		 * have generated a dynamic html page here instead.
		 */

		if (lws_http_redirect(wsi, HTTP_STATUS_MOVED_PERMANENTLY,
				      (unsigned char *)"after-form1.html",
				      16, &p, end) < 0)
			return -1;
		break;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static struct lws_protocols protocols[] = {
	{ "http", callback_http, 0, 0, 0, NULL, 0 },
	LWS_PROTOCOL_LIST_TERM
};

/* default mount serves the URL space from ./mount-origin */

static const struct lws_http_mount mount = {
	/* .mount_next */	       NULL,		/* linked-list "next" */
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
	lwsl_user("LWS minimal http server GET | visit http://localhost:7681\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = 7681;
	info.protocols = protocols;
	info.mounts = &mount;
	info.options =
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

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
