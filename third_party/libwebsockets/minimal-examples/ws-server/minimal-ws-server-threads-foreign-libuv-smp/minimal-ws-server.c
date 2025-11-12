/*
 * lws-minimal-ws-server-threads-foreign-smp
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a minimal ws server that can cooperate with
 * other threads cleanly.  Two other threads are started, which fill
 * a ringbuffer with strings at 10Hz.
 *
 * The actual work and thread spawning etc are done in the protocol
 * implementation in protocol_lws_minimal.c.
 *
 * To keep it simple, it serves stuff in the subdirectory "./mount-origin" of
 * the directory it was started in.
 * You can change that by changing mount.origin.
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
#include <uv.h>

#define COUNT_THREADS 5

#define LWS_PLUGIN_STATIC
#include "protocol_lws_minimal.c"

static struct lws_protocols protocols[] = {
	{ "http", lws_callback_http_dummy, 0, 0, 0, NULL, 0 },
	LWS_PLUGIN_PROTOCOL_MINIMAL,
	LWS_PROTOCOL_LIST_TERM
};

static struct lws_context *context;
static int interrupted;
static uv_loop_t loop[COUNT_THREADS];
static uv_signal_t *s, signal_outer[COUNT_THREADS];

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

/*
 * This demonstrates how to pass a pointer into a specific protocol handler
 * running on a specific vhost.  In this case, it's our default vhost and
 * we pass the pvo named "config" with the value a const char * "myconfig".
 *
 * This is the preferred way to pass configuration into a specific vhost +
 * protocol instance.
 */

static const struct lws_protocol_vhost_options pvo_ops = {
	NULL,
	NULL,
	"config",		/* pvo name */
	(void *)"myconfig"	/* pvo value */
};

static const struct lws_protocol_vhost_options pvo = {
	NULL,		/* "next" pvo linked-list */
	&pvo_ops,	/* "child" pvo linked-list */
	"lws-minimal",	/* protocol name we belong to on this vhost */
	""		/* ignored */
};

void *thread_service(void *threadid)
{
	/*
	 * This is a foreign thread context for each event loop... lws doesn't
	 * know about it, except that it's getting called into from the event
	 * lib bound to each of these.
	 *
	 * When closing, at the point we have detached everything related to
	 * lws from the loop and destroyed the context we can as the "foreign
	 * app" take care of stopping the foreign loop and cloing this thread.
	 *
	 * The call to lws_service_tsi just starts the related event loop
	 */
	while (lws_service_tsi(context, 0,
			       (int)(lws_intptr_t)threadid) >= 0 &&
	       !interrupted)
		lwsl_notice("%s\n", __func__);

	lwsl_info("%s: thr %d: exiting\n", __func__, (int)(lws_intptr_t)threadid);

	pthread_exit(NULL);

	return NULL;
}

static void
signal_cb(uv_signal_t *watcher, int signum)
{
	int n;

	n = (int)(watcher - signal_outer);

	lwsl_notice("%s: thr %d: signal %d caught\n", __func__, n,
			watcher->signum);

	uv_signal_stop(watcher);
	uv_close((uv_handle_t *)&signal_outer[n], NULL);
	if (!interrupted) {
		interrupted = 1;
		lws_context_destroy(context);
	}
}

int main(int argc, const char **argv)
{
	int n, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	pthread_t pthread_service[COUNT_THREADS];
	struct lws_context_creation_info info;
	void *foreign_loops[COUNT_THREADS];
	int actual_threads;
	const char *p;
	void *retval;

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal ws server + threads + smp | visit http://localhost:7681\n");

	for (n = 0; n < COUNT_THREADS; n++) {
		uv_loop_init(&loop[n]);

		s = &signal_outer[n];
		uv_signal_init(&loop[n], s);
		uv_signal_start(s, signal_cb, SIGINT);

		foreign_loops[n] = &loop[n];
	}

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = 7681;
	info.mounts = &mount;
	info.pcontext = &context;
	info.protocols = protocols;
	info.pvo = &pvo; /* per-vhost options */
	info.foreign_loops = foreign_loops;
	info.count_threads = COUNT_THREADS;
	info.options = LWS_SERVER_OPTION_LIBUV |
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	actual_threads = lws_get_count_threads(context);
	lwsl_notice("  Service threads: %d\n", actual_threads);

	/* start all the service threads */

	for (n = 0; n < actual_threads; n++)
		if (pthread_create(&pthread_service[n], NULL, thread_service,
				   (void *)(lws_intptr_t)n))
			lwsl_err("Failed to start service thread\n");

	/* wait for all the service threads to exit */

	while ((--n) >= 0)
		pthread_join(pthread_service[n], &retval);

	lws_context_destroy(context);

	for (n = 0; n < COUNT_THREADS; n++) {
		int m;

		m = uv_loop_close(&loop[n]);
		if (m)
			lwsl_notice("%s: uv_close_loop %d: %d\n", __func__, n, m);
	}

	return 0;
}
