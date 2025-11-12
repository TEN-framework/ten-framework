/*
 * lws-minimal-raw-proxy
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a vhost that acts as a raw tcp proxy.  Incoming connections
 * cause an outgoing connection to be initiated, and if successfully established
 * then traffic coming in one side is placed on a ringbuffer and sent out the
 * opposite side as soon as possible.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>

#define LWS_PLUGIN_STATIC
#include "../plugins/raw-proxy/protocol_lws_raw_proxy.c"

static struct lws_protocols protocols[] = {
	LWS_PLUGIN_PROTOCOL_RAW_PROXY,
	LWS_PROTOCOL_LIST_TERM
};

static int interrupted;

void sigint_handler(int sig)
{
	interrupted = 1;
}

static struct lws_protocol_vhost_options pvo1 = {
        NULL,
        NULL,
        "onward",          /* pvo name */
        "ipv4:127.0.0.1:22"    /* pvo value */
};

static const struct lws_protocol_vhost_options pvo = {
        NULL,           /* "next" pvo linked-list */
        &pvo1,       /* "child" pvo linked-list */
        "raw-proxy",      /* protocol name we belong to on this vhost */
        ""              /* ignored */
};


int main(int argc, const char **argv)
{
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	struct lws_context_creation_info info;
	struct lws_context *context;
	char outward[256];
	const char *p;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal raw proxy\n");

	if ((p = lws_cmdline_option(argc, argv, "-r"))) {
		lws_strncpy(outward, p, sizeof(outward));
		pvo1.value = outward;
	}

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = 7681;
	info.protocols = protocols;
	info.pvo = &pvo;
	info.options = LWS_SERVER_OPTION_ADOPT_APPLY_LISTEN_ACCEPT_CONFIG;
	info.listen_accept_role = "raw-proxy";
	info.listen_accept_protocol = "raw-proxy";

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
