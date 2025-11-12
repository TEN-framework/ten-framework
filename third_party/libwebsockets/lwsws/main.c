/*
 * libwebsockets web server application
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The person who associated a work with this deed has dedicated
 * the work to the public domain by waiving all of his or her rights
 * to the work worldwide under copyright law, including all related
 * and neighboring rights, to the extent allowed by law. You can copy,
 * modify, distribute and perform the work, even for commercial purposes,
 * all without asking permission.
 *
 * The test apps are intended to be adapted for use in your code, which
 * may be proprietary.	So unlike the library itself, they are licensed
 * Public Domain.
 */
#include "lws_config.h"

#include <stdio.h>
#include <stdlib.h>
#if defined(LWS_HAS_GETOPT_LONG) || defined(WIN32)
#include <getopt.h>
#endif
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#ifndef _WIN32
#include <dirent.h>
#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/wait.h>
#else
#include <io.h>
#include "gettimeofday.h"
#include <uv.h>

int fork(void)
{
	fprintf(stderr, "Sorry Windows doesn't support fork().\n");
	return 0;
}
#endif

#include <libwebsockets.h>

#include <uv.h>

#if defined(LWS_HAVE_MALLOC_TRIM)
#include <malloc.h>
#endif

static struct lws_context *context;
static lws_sorted_usec_list_t sul_lwsws;
static char config_dir[128], default_plugin_path = 1;
static int opts = 0, do_reload = 1;
static uv_loop_t loop;
static uv_signal_t signal_outer[2];
static int pids[32];
void lwsl_emit_stderr(int level, const char *line);

#define LWSWS_CONFIG_STRING_SIZE (64 * 1024)

static const struct lws_extension exts[] = {
#if !defined(LWS_WITHOUT_EXTENSIONS)
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate"
	},
#endif
	{ NULL, NULL, NULL /* terminator */ }
};

#if defined(LWS_WITH_PLUGINS)
static const char * const plugin_dirs[] = {
	INSTALL_DATADIR"/libwebsockets-test-server/plugins/",
	NULL
};
#endif

#if defined(LWS_HAS_GETOPT_LONG) || defined(WIN32)
static struct option options[] = {
	{ "help",	no_argument,		NULL, 'h' },
	{ "debug",	required_argument,	NULL, 'd' },
	{ "configdir",  required_argument,	NULL, 'c' },
	{ "no-default-plugins",  no_argument,	NULL, 'n' },
	{ NULL, 0, 0, 0 }
};
#endif

void signal_cb(uv_signal_t *watcher, int signum)
{
	switch (watcher->signum) {
	case SIGTERM:
	case SIGINT:
		break;

	case SIGHUP:
		if (lws_context_is_deprecated(context))
			return;
		lwsl_notice("Dropping listen sockets\n");
		lws_context_deprecate(context, NULL);
		return;

	default:
		signal(SIGABRT, SIG_DFL);
		abort();
		break;
	}
	lwsl_err("Signal %d caught\n", watcher->signum);
	uv_signal_stop(watcher);
	uv_signal_stop(&signal_outer[1]);
	lws_context_destroy(context);
}

static void
lwsws_min(lws_sorted_usec_list_t *sul)
{
	lwsl_debug("%s\n", __func__);

#if defined(LWS_HAVE_MALLOC_TRIM)
	malloc_trim(4 * 1024);
#endif

	lws_sul_schedule(context, 0, &sul_lwsws, lwsws_min, 60 * LWS_US_PER_SEC);
}

static int
context_creation(void)
{
	int cs_len = LWSWS_CONFIG_STRING_SIZE - 1;
	struct lws_context_creation_info info;
	char *cs, *config_strings;
	void *foreign_loops[1];

	cs = config_strings = malloc(LWSWS_CONFIG_STRING_SIZE);
	if (!config_strings) {
		lwsl_err("Unable to allocate config strings heap\n");
		return -1;
	}

	memset(&info, 0, sizeof(info));

	info.external_baggage_free_on_destroy = config_strings;
	info.pt_serv_buf_size = 8192;
	info.options = (uint64_t)((uint64_t)opts | LWS_SERVER_OPTION_VALIDATE_UTF8 |
			      LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
			      LWS_SERVER_OPTION_LIBUV);

#if defined(LWS_WITH_PLUGINS)
	if (default_plugin_path)
		info.plugin_dirs = plugin_dirs;
#endif
	lwsl_notice("Using config dir: \"%s\"\n", config_dir);

	/*
	 *  first go through the config for creating the outer context
	 */
	if (lwsws_get_config_globals(&info, config_dir, &cs, &cs_len))
		goto init_failed;

	foreign_loops[0] = &loop;
	info.foreign_loops = foreign_loops;
	info.pcontext = &context;

	context = lws_create_context(&info);
	if (context == NULL) {
		lwsl_err("libwebsocket init failed\n");
		goto init_failed;
	}

	/*
	 * then create the vhosts... protocols are entirely coming from
	 * plugins, so we leave it NULL
	 */

	info.extensions = exts;

	if (lwsws_get_config_vhosts(context, &info, config_dir, &cs, &cs_len))
		return 1;

	lws_sul_schedule(context, 0, &sul_lwsws, lwsws_min, 60 * LWS_US_PER_SEC);

	return 0;

init_failed:
	free(config_strings);

	return 1;
}


/*
 * root-level sighup handler
 */

static void
reload_handler(int signum)
{
#ifndef _WIN32
	int m;

	switch (signum) {

	case SIGHUP: /* reload */
		fprintf(stderr, "root process receives reload\n");
		if (!do_reload) {
			fprintf(stderr, "passing HUP to child processes\n");
			for (m = 0; m < (int)LWS_ARRAY_SIZE(pids); m++)
				if (pids[m])
					kill(pids[m], SIGHUP);
			sleep(1);
		}
		do_reload = 1;
		break;
	case SIGINT:
	case SIGTERM:
	case SIGKILL:
		fprintf(stderr, "parent process waiting 2s...\n");
		sleep(2); /* give children a chance to deal with the signal */
		fprintf(stderr, "killing service processes\n");
		for (m = 0; m < (int)LWS_ARRAY_SIZE(pids); m++)
			if (pids[m])
				kill(pids[m], SIGTERM);
		exit(0);
	}
#else
	// kill() implementation needed for WIN32
#endif
}

int main(int argc, char **argv)
{
	int n = 0, budget = 100, debug_level = 1024 + 7;
#ifndef _WIN32
	int m;
	int status;//, syslog_options = LOG_PID | LOG_PERROR;
#endif

	strcpy(config_dir, "/etc/lwsws");
	while (n >= 0) {
#if defined(LWS_HAS_GETOPT_LONG) || defined(WIN32)
		n = getopt_long(argc, argv, "hd:c:n", options, NULL);
#else
		n = getopt(argc, argv, "hd:c:n");
#endif
		if (n < 0)
			continue;
		switch (n) {
		case 'd':
			debug_level = atoi(optarg);
			break;
		case 'n':
			default_plugin_path = 0;
			break;
		case 'c':
			lws_strncpy(config_dir, optarg, sizeof(config_dir));
			break;
		case 'h':
			fprintf(stderr, "Usage: lwsws [-c <config dir>] "
					"[-d <log bitfield>] [--help] "
					"[-n]\n");
			exit(1);
		}
	}
#ifndef _WIN32
	/*
	 * We leave our original process up permanently, because that
	 * suits systemd.
	 *
	 * Otherwise we get into problems when reload spawns new processes and
	 * the original one dies randomly.
	 */

	signal(SIGHUP, reload_handler);
	signal(SIGINT, reload_handler);

	fprintf(stderr, "Root process is %u\n", (unsigned int)getpid());

	while (1) {
		if (do_reload) {
			do_reload = 0;
			n = fork();
			if (n == 0) /* new */
				break;
			/* old */
			if (n > 0)
				for (m = 0; m < (int)LWS_ARRAY_SIZE(pids); m++)
					if (!pids[m]) {
						pids[m] = n;
						break;
					}
		}
#ifndef _WIN32
		sleep(2);

		n = waitpid(-1, &status, WNOHANG);
		if (n > 0)
			for (m = 0; m < (int)LWS_ARRAY_SIZE(pids); m++)
				if (pids[m] == n) {
					pids[m] = 0;
					break;
				}
#else
// !!! implemenation needed
#endif
	}
#endif
	/* child process */

	lws_set_log_level(debug_level, lwsl_emit_stderr_notimestamp);

	lwsl_notice("lwsws libwebsockets web server - license CC0 + MIT\n");
	lwsl_notice("(C) Copyright 2010-2020 Andy Green <andy@warmcat.com>\n");

#if (UV_VERSION_MAJOR > 0) // Travis...
	uv_loop_init(&loop);
#else
	fprintf(stderr, "Your libuv is too old!\n");
	return 0;
#endif
	uv_signal_init(&loop, &signal_outer[0]);
	uv_signal_start(&signal_outer[0], signal_cb, SIGINT);
	uv_signal_init(&loop, &signal_outer[1]);
	uv_signal_start(&signal_outer[1], signal_cb, SIGHUP);

	if (context_creation()) {
		lwsl_err("Context creation failed\n");
		return 1;
	}

	lws_service(context, 0);

	lwsl_err("%s: closing\n", __func__);

	for (n = 0; n < 2; n++) {
		uv_signal_stop(&signal_outer[n]);
		uv_close((uv_handle_t *)&signal_outer[n], NULL);
	}

	/* cancel the per-minute sul */
	lws_sul_cancel(&sul_lwsws);

	lws_context_destroy(context);
	(void)budget;
#if (UV_VERSION_MAJOR > 0) // Travis...
	while ((n = uv_loop_close(&loop)) && --budget)
		uv_run(&loop, UV_RUN_ONCE);
#endif

	fprintf(stderr, "lwsws exited cleanly: %d\n", n);

#ifndef _WIN32
	closelog();
#endif

	context = NULL;

	return 0;
}
