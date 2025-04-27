#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#if defined(WIN32)
#define HAVE_STRUCT_TIMESPEC
#if defined(pid_t)
#undef pid_t
#endif
#endif

static int nclients = 11;
unsigned char msg[LWS_PRE+128];
static int message_delay = 500000; // microseconds
static int connection_delay = 100000; // microseconds
static struct lws_context *context;
static const char *server_address = "localhost", *pro = "lws-minimal";
static int interrupted = 0, port = 7681, ssl_connection = 0;

static int connect_client()
{
	struct lws_client_connect_info i;

	memset(&i, 0, sizeof(i));

	i.context = context;
	i.port = port;
	i.address = server_address;
	i.path = "/";
	i.host = i.address;
	i.origin = i.address;
	i.ssl_connection = ssl_connection;
	i.protocol = pro;
	i.local_protocol_name = pro;

	//usleep(connection_delay);
	lwsl_notice("%s: connection %s:%d\n", __func__, i.address, i.port);
	if (!lws_client_connect_via_info(&i)) return 1;

	return 0;
}

static int
callback(struct lws *wsi, enum lws_callback_reasons reason,
		void *user, void *in, size_t len)
{
	int m= 0, n = 0;
	short r;
#if defined(_DEBUG) && !defined(LWS_WITH_NO_LOGS)
	size_t remain;
	int first = 0, final = 0;
#endif

	//lwsl_notice("callback called with reason %d\n", reason);
	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT:
		for (n = 0; n < nclients; n++)
			connect_client();
		break;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char *)in :
				"(null)");
		if(--nclients == 0) interrupted = 1;
		break;

		/* --- client callbacks --- */

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lws_callback_on_writable(wsi);
		lwsl_user("%s: established connection, wsi = %p\n",
				__func__, wsi);
		break;

	case LWS_CALLBACK_CLIENT_CLOSED:
		lwsl_user("%s: CLOSED\n", __func__);
		if(--nclients == 0) interrupted = 1;
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:

		m = lws_write(wsi, msg + LWS_PRE, 128, LWS_WRITE_TEXT);
		if (m < 128) {
			lwsl_err("sending message failed: %d < %d\n", m, n);
			return -1;
		}

		/*
		 * Schedule the timer after minimum message delay plus the
		 * random number of centiseconds.
		 */
		if (lws_get_random(lws_get_context(wsi), &r, 2) == 2) {
			n = message_delay + 10000*(r % 100);
			lwsl_debug("set timer on %d usecs\n", n);
			lws_set_timer_usecs(wsi, n);
		}
		break;

	case LWS_CALLBACK_TIMER:
		// Let the main loop know we want to send another message to the
		// server
		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
#if defined(_DEBUG) && !defined(LWS_WITH_NO_LOGS)
		first = lws_is_first_fragment(wsi);
		final = lws_is_final_fragment(wsi);
		remain = lws_remaining_packet_payload(wsi);
		lwsl_debug("LWS_CALLBACK_RECEIVE: len = %lu, first = %d, "
			   "final = %d, remains = %lu\n",
			   (unsigned long)len, first, final,
			   (unsigned long)remain);
#endif
		break;

	case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
		lwsl_notice("server initiated connection close: len = %lu, "
			    "in = %s\n", (unsigned long)len, (char*)in);
		return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
		{ "spam-rx-tx", callback, 4096, 4096, 0, NULL, 0 },
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
	int n = 0, logs =
			LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
#ifndef WIN32
	srandom((unsigned int)time(0));
#endif

	memset(msg, 'x', sizeof(msg));

	signal(SIGINT, sigint_handler);

	if (lws_cmdline_option(argc, argv, "-d"))
		logs |= LLL_INFO | LLL_DEBUG;

	lws_set_log_level(logs, NULL);

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

	if ((p = lws_cmdline_option(argc, argv, "-h"))) {
		server_address = p;
	}

	if ((p = lws_cmdline_option(argc, argv, "-s"))) {
		ssl_connection |=
				LCCSCF_USE_SSL |
				LCCSCF_ALLOW_SELFSIGNED |
				LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
	}

	if ((p = lws_cmdline_option(argc, argv, "-p"))) {
		port = atoi(p);
		if (port > 65535 || port < 0)
			return 1;
	}

	if ((p = lws_cmdline_option(argc, argv, "-n"))) {
		n = atoi(p);
		if (n < 1)
			n = 1;
		if (n > LWS_MAX_SMP)
			n = LWS_MAX_SMP;
		if (n < nclients)
			nclients = n;
		lwsl_notice("Start test clients: %d\n", nclients);
	}

	if ((p = lws_cmdline_option(argc, argv, "-c"))) {
		connection_delay = atoi(p);
		lwsl_notice("Connection delay: %d\n", connection_delay);
	}

	if ((p = lws_cmdline_option(argc, argv, "-m"))) {
		message_delay = atoi(p);
		lwsl_notice("Message delay: %d\n", connection_delay);
	}

	info.fd_limit_per_thread = (unsigned int)(1 + nclients + 1);

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lwsl_notice("%s: exiting service loop. n = %d, interrupted = %d\n",
			__func__, n, interrupted);

	lws_context_destroy(context);

	return 0;
}
