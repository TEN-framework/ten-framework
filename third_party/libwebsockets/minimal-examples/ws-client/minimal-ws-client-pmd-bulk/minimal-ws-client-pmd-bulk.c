/*
 * lws-minimal-ws-client-pmd-bulk
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a ws client that sends bulk data in multiple
 * ws fragments, in a way compatible with per-message deflate.
 *
 * It shows how to send huge messages without needing a lot of memory.
 * 
 * Build and start the minimal-examples/ws-server/minmal-ws-server-pmd-bulk
 * example first.  Running this sends a large message to the server and
 * exits.
 *
 * If you give both sides the -n commandline option, it disables permessage-
 * deflate compression extension.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

#define LWS_PLUGIN_STATIC
#include "protocol_lws_minimal_pmd_bulk.c"

static struct lws_protocols protocols[] = {
	{ "http", lws_callback_http_dummy, 0, 0, 0, NULL, 0 },
	LWS_PLUGIN_PROTOCOL_MINIMAL_PMD_BULK,
	LWS_PROTOCOL_LIST_TERM
};

static int interrupted, options;

/* pass pointers to shared vars to the protocol */

static const struct lws_protocol_vhost_options pvo_options = {
	NULL,
	NULL,
	"options",		/* pvo name */
	(void *)&options	/* pvo value */
};

static const struct lws_protocol_vhost_options pvo_interrupted = {
	&pvo_options,
	NULL,
	"interrupted",		/* pvo name */
	(void *)&interrupted	/* pvo value */
};

static const struct lws_protocol_vhost_options pvo = {
	NULL,		/* "next" pvo linked-list */
	&pvo_interrupted,	/* "child" pvo linked-list */
	"lws-minimal-pmd-bulk",	/* protocol name we belong to on this vhost */
	""		/* ignored */
};
static const struct lws_extension extensions[] = {
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate"
		 "; client_no_context_takeover"
		 "; client_max_window_bits"
	},
	{ NULL, NULL, NULL /* terminator */ }
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
	lwsl_user("LWS minimal ws client + permessage-deflate + multifragment bulk message\n");
	lwsl_user("   needs minimal-ws-server-pmd-bulk running to communicate with\n");
	lwsl_user("   %s [-n (no exts)] [-c (compressible)]\n", argv[0]);

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = protocols;
	info.pvo = &pvo;
	if (!lws_cmdline_option(argc, argv, "-n"))
		info.extensions = extensions;
	info.pt_serv_buf_size = 32 * 1024;

	if (lws_cmdline_option(argc, argv, "-c"))
		options |= 1;

	/*
	 * since we know this lws context is only ever going to be used with
	 * one client wsis / fds / sockets at a time, let lws know it doesn't
	 * have to use the default allocations for fd tables up to ulimit -n.
	 * It will just allocate for 1 internal and 1 (+ 1 http2 nwsi) that we
	 * will use.
	 */
	info.fd_limit_per_thread = 1 + 1 + 1;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);

	lwsl_user("Completed %s\n", interrupted == 2 ? "OK" : "failed");

	return interrupted != 2;
}
