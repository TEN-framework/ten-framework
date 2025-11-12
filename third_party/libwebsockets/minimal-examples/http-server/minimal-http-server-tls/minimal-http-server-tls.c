/*
 * lws-minimal-http-server-tls
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
 * To keep it simple, it serves stuff from the subdirectory 
 * "./mount-origin" of the directory it was started in.
 *
 * You can change that by changing mount.origin below.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

static int interrupted;

#if defined(LWS_WITH_PLUGINS)
static const char * const plugin_dirs[] = {
	LWS_INSTALL_DATADIR"/libwebsockets-test-server/plugins/",
	NULL
};
#endif

static const struct lws_http_mount
#if defined(LWS_WITH_SYS_METRICS)
	mount_metrics = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/metrics",		/* mountpoint URL */
	/* .origin */			"lws-openmetrics", /* serve from dir */
	/* .def */			"x",	/* default filename */
	/* .protocol */			"lws-openmetrics",
	/* .cgienv */			NULL,
	/* .extra_mimetypes */		NULL,
	/* .interpret */		NULL,
	/* .cgi_timeout */		0,
	/* .cache_max_age */		0,
	/* .auth_mask */		0,
	/* .cache_reusable */		0,
	/* .cache_revalidate */		0,
	/* .cache_intermediaries */	0,
	/* .origin_protocol */		LWSMPRO_CALLBACK, /* bind to callback */
	/* .mountpoint_len */		8,		/* char count */
	/* .basic_auth_login_file */	NULL,
	},
#endif
	mount = {
#if defined(LWS_WITH_SYS_METRICS)
	/* .mount_next */		&mount_metrics,		/* linked-list "next" */
#else
	/* .mount_next */		NULL,		/* linked-list "next" */
#endif
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */			"./mount-origin", /* serve from dir */
	/* .def */			"index.html",	/* default filename */
	/* .protocol */			"http-only",
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

#if !defined(WIN32)
void sigint_handler(int sig, siginfo_t *siginfo, void *context)
{
	pid_t sender_pid = siginfo->si_pid;
	lwsl_err("%s: sig %d from pid %lu\n", __func__, sig, (unsigned long)sender_pid);
	interrupted = 1;
}
#else
void sigint_handler(int sig)
{
	interrupted = 1;
}
#endif

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
#if !defined(WIN32)
	struct sigaction siga;
#endif
	const char *p;
	int n = 0;

#if !defined(WIN32)
	memset(&siga, 0, sizeof(siga));
	siga.sa_sigaction = sigint_handler;
	siga.sa_flags |= SA_SIGINFO; // get detail info

	    // change signal action,
	if (sigaction(SIGINT, &siga, NULL) != 0) {
	        printf("error sigaction()");
	        return errno;
	    }
#else
	signal(SIGINT, sigint_handler);
#endif
	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	lws_cmdline_option_handle_builtin(argc, argv, &info);
	lwsl_user("LWS minimal http server TLS | visit https://localhost:7681\n");

	info.port = 7681;
	if ((p = lws_cmdline_option(argc, argv, "--port")))
		info.port = atoi(p);
	info.mounts = &mount;
	info.error_document_404 = "/404.html";
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
	info.ssl_cert_filepath = "localhost-100y.cert";
	info.ssl_private_key_filepath = "localhost-100y.key";
	info.fo_listen_queue = 32;

#if defined(LWS_WITH_PLUGINS)
	info.plugin_dirs = plugin_dirs;
#endif

	if (lws_cmdline_option(argc, argv, "-h"))
		info.options |= LWS_SERVER_OPTION_VHOST_UPG_STRICT_HOST_CHECK;

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
