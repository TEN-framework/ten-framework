/*
 * lws-minimal-http-server-multivhost
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
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

static int interrupted;

static const struct lws_http_mount mount_localhost1 = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */			"./mount-origin-localhost1",
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
}, mount_localhost2 = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */			"./mount-origin-localhost2",
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
}, mount_localhost3 = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */			"./mount-origin-localhost3",
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

void vh_destruction_notification(struct lws_vhost *vh, void *arg)
{
	lwsl_user("%s: called, arg: %p\n", __func__, arg);
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	struct lws_vhost *new_vhost;
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
	lwsl_user("LWS minimal http server-multivhost | visit http://localhost:7681 / 7682\n");

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	/*
	 * Because of LWS_SERVER_OPTION_EXPLICIT_VHOSTS, this only creates
	 * the context and no longer creates a default vhost
	 */
	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	/* it's our job now to create the vhosts we want:
	 *
	 *   - "localhost1" listen on 7681 and serve ./mount-origin-localhost1/
	 *   - "localhost2" listen on 7682 and serve ./mount-origin-localhost2/
	 *   - "localhost3" share 7682 and serve ./mount-origin-localhost3/
	 *
	 * Note lws supports dynamic vhost creation and destruction at runtime.
	 * When using multi-vhost with your own protocols, you must provide a
	 * pvo for each vhost naming each protocol you want enabled on it.
	 * minimal-ws-server-threads demonstrates how to provide pvos.
	 */

	info.port = 7681;
	info.mounts = &mount_localhost1;
	info.error_document_404 = "/404.html";
	info.vhost_name = "localhost1";

	if (!lws_create_vhost(context, &info)) {
		lwsl_err("Failed to create first vhost\n");
		goto bail;
	}

	info.port = 7682;
	info.mounts = &mount_localhost2;
	info.error_document_404 = "/404.html";
	info.vhost_name = "localhost2";

	if (!lws_cmdline_option(argc, argv, "--kill-7682")) {

		if (!lws_create_vhost(context, &info)) {
			lwsl_err("Failed to create second vhost\n");
			goto bail;
		}
	}

	/* a second vhost listens on port 7682 */
	info.mounts = &mount_localhost3;
	info.error_document_404 = "/404.html";
	info.vhost_name = "localhost3";
	info.finalize = vh_destruction_notification;
	info.finalize_arg = NULL;

	new_vhost = lws_create_vhost(context, &info);
	if (!new_vhost) {
		lwsl_err("Failed to create third vhost\n");
		goto bail;
	}

	if (lws_cmdline_option(argc, argv, "--kill-7682"))
		lws_vhost_destroy(new_vhost);

	if (lws_cmdline_option(argc, argv, "--die-after-vhost")) {
		lwsl_warn("bailing after creating vhosts\n");
		goto bail;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

bail:
	lws_context_destroy(context);

	return 0;
}
