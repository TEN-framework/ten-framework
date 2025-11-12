/*
 * lws-minimal-http-server-tls-80
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates the most minimal http server you can make with lws,
 * with three extra lines giving it tls (ssl) capabilities, which in
 * turn allow operation with HTTP/2 if lws was configured for it.
 *
 * In addition, it runs a vhost on port 80 with the job of redirecting
 * and upgrading http clients that came in on port 80 to https on port 443.
 *
 * To keep it simple, it serves stuff from the subdirectory 
 * "./mount-origin" of the directory it was started in.
 *
 * You can change that by changing mount.origin below.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

static int interrupted;

static const struct lws_http_mount mount80 = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */			"localhost/",
	/* .def */			"/",	/* default filename */
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
	/* .origin_protocol */		LWSMPRO_REDIR_HTTPS, /* https redir */
	/* .mountpoint_len */		1,		/* char count */
	/* .basic_auth_login_file */	NULL,
};

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

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal http server TLS + 80 | visit https://localhost\n");
	lwsl_user(" Run as ROOT so can listen on 443\n");

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */

	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
		       LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}


	info.port = 80;
	info.mounts = &mount80;
	info.vhost_name = "localhost80";

	if (!lws_create_vhost(context, &info)) {
		lwsl_err("Failed to create tls vhost\n");
		goto bail;
	}

	info.port = 443;
	info.mounts = &mount;
	info.error_document_404 = "/404.html";
	info.ssl_cert_filepath = "localhost-100y.cert";
	info.ssl_private_key_filepath = "localhost-100y.key";
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
