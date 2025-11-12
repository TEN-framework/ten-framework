/*
 * lws-minimal-ws-client-ping
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates keeping a ws connection validated by the lws validity
 * timer stuff without having to do anything in the code.  Use debug logging
 * -d1039 to see lws doing the pings / pongs in the background.
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

static struct lws_context *context;
static struct lws *client_wsi;
static int interrupted, port = 443, ssl_connection = LCCSCF_USE_SSL;
static const char *server_address = "libwebsockets.org", *pro = "lws-mirror-protocol";
static lws_sorted_usec_list_t sul;

static const lws_retry_bo_t retry = {
	.secs_since_valid_ping = 3,
	.secs_since_valid_hangup = 10,
};

static void
connect_cb(lws_sorted_usec_list_t *_sul)
{
	struct lws_client_connect_info i;

	lwsl_notice("%s: connecting\n", __func__);

	memset(&i, 0, sizeof(i));

	i.context = context;
	i.port = port;
	i.address = server_address;
	i.path = "/";
	i.host = i.address;
	i.origin = i.address;
	i.ssl_connection = ssl_connection;
	i.protocol = pro;
	i.alpn = "h2;http/1.1";
	i.local_protocol_name = "lws-ping-test";
	i.pwsi = &client_wsi;
	i.retry_and_idle_policy = &retry;

	if (!lws_client_connect_via_info(&i))
		lws_sul_schedule(context, 0, _sul, connect_cb, 5 * LWS_USEC_PER_SEC);
}

static int
callback_minimal_pingtest(struct lws *wsi, enum lws_callback_reasons reason,
			 void *user, void *in, size_t len)
{

	switch (reason) {

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		lws_sul_schedule(context, 0, &sul, connect_cb, 5 * LWS_USEC_PER_SEC);
		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lwsl_user("%s: established\n", __func__);
		break;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
	{
		"lws-ping-test",
		callback_minimal_pingtest,
		0, 0, 0, NULL, 0
	},
	LWS_PROTOCOL_LIST_TERM
};

static void
sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
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
	lwsl_user("LWS minimal ws client PING\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = protocols;
#if defined(LWS_WITH_MBEDTLS) || defined(USE_WOLFSSL)
	/*
	 * OpenSSL uses the system trust store.  mbedTLS has to be told which
	 * CA to trust explicitly.
	 */
	info.client_ssl_ca_filepath = "./libwebsockets.org.cer";
#endif

	if ((p = lws_cmdline_option(argc, argv, "--protocol")))
		pro = p;

	if ((p = lws_cmdline_option(argc, argv, "--server"))) {
		server_address = p;
		pro = "lws-minimal";
		ssl_connection |= LCCSCF_ALLOW_SELFSIGNED;
	}

	if ((p = lws_cmdline_option(argc, argv, "--port")))
		port = atoi(p);

	info.fd_limit_per_thread = 1 + 1 + 1;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	lws_sul_schedule(context, 0, &sul, connect_cb, 100);

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);
	lwsl_user("Completed\n");

	return 0;
}
