/*
 * lws-minimal-secure-streams-server
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

extern const lws_ss_info_t ssi_client, ssi_server;

static struct lws_context *context;
int interrupted, bad = 1;
static const char * const default_ss_policy =
	"{"
	  "\"release\":"			"\"01234567\","
	  "\"product\":"			"\"myproduct\","
	  "\"schema-version\":"			"1,"
	  "\"s\": ["

		/*
		 * This streamtype represents a raw server listening on :7681,
		 * without tls
		 */

		"{\"myrawserver\": {"
			/* if given, "endpoint" is network if to bind to */
			"\"server\":"		"true,"
			"\"port\":"		"7681,"
			"\"protocol\":"		"\"raw\""
		"}}"

	  "]"
	"}"
;

static int
smd_cb(void *opaque, lws_smd_class_t c, lws_usec_t ts, void *buf, size_t len)
{
	if ((c & LWSSMDCL_SYSTEM_STATE) &&
	    !lws_json_simple_strcmp(buf, len, "\"state\":", "OPERATIONAL")) {

		/* create the secure streams */

		lwsl_notice("%s: creating server stream\n", __func__);

		if (lws_ss_create(context, 0, &ssi_server, NULL, NULL,
				  NULL, NULL)) {
			lwsl_err("%s: failed to create secure stream\n",
				 __func__);
			return -1;
		}
	}

	return 0;
}

static void
sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	int n = 0;

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	lws_cmdline_option_handle_builtin(argc, argv, &info);
	lwsl_user("LWS Secure Streams Server Raw\n");

	info.options			= LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
					  LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.fd_limit_per_thread	= 1 + 6 + 1;
	info.pss_policies_json		= default_ss_policy;
	info.port			= CONTEXT_PORT_NO_LISTEN;
	info.early_smd_cb		= smd_cb;
	info.early_smd_class_filter	= LWSSMDCL_SYSTEM_STATE;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	/* the event loop */

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	bad = 0;

	lws_context_destroy(context);
	lwsl_user("Completed: %s\n", bad ? "failed" : "OK");

	return bad;
}
