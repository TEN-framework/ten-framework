/*
 * lws-minimal-http-server-smp
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a minimal multithreaded http server you can make with lws.
 *
 * To keep it simple, it serves stuff in the subdirectory "./mount-origin" of
 * the directory it was started in.
 * You can change that by changing mount.origin.
 *
 * Also for simplicity the number of threads is set in the code... note that
 * the real number of threads possible is decided by the LWS_MAX_SMP that lws
 * was configured with, by default that is 1.  Lws will limit the number of
 * requested threads to the number possible.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#if defined(WIN32)
#define HAVE_STRUCT_TIMESPEC
#if defined(pid_t)
#undef pid_t
#endif
#endif
#include <pthread.h>

#define COUNT_THREADS 8

static struct lws_context *context;
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

void *thread_service(void *threadid)
{
	while (lws_service_tsi(context, 10000,
			       (int)(lws_intptr_t)threadid) >= 0 &&
	       !interrupted)
		;

	pthread_exit(NULL);

	return NULL;
}

void sigint_handler(int sig)
{
	interrupted = 1;
	lws_cancel_service(context);
}

int main(int argc, const char **argv)
{
	pthread_t pthread_service[COUNT_THREADS];
	struct lws_context_creation_info info;
	void *retval;
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
	lwsl_user("LWS minimal http server SMP | visit http://127.0.0.1:7681\n");

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = 7681;
	info.mounts = &mount;
	info.options =
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
	if ((p = lws_cmdline_option(argc, argv, "-t"))) {
		info.count_threads = (unsigned int)atoi(p);
		if (info.count_threads < 1 || info.count_threads > LWS_MAX_SMP)
			return 1;
	} else
		info.count_threads = COUNT_THREADS;

#if defined(LWS_WITH_TLS)
	if (lws_cmdline_option(argc, argv, "-s")) {
		info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
		info.ssl_cert_filepath = "localhost-100y.cert";
		info.ssl_private_key_filepath = "localhost-100y.key";
	}
#endif

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	lwsl_notice("  Service threads: %d\n", lws_get_count_threads(context));

	/* start all the service threads */

	for (n = 0; n < lws_get_count_threads(context); n++)
		if (pthread_create(&pthread_service[n], NULL, thread_service,
				   (void *)(lws_intptr_t)n))
			lwsl_err("Failed to start service thread\n");

	/* wait for all the service threads to exit */

	while ((--n) >= 0)
		pthread_join(pthread_service[n], &retval);

	lws_context_destroy(context);

	return 0;
}
