/*
 * lws-minimal-ws-client-spam
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a ws client that makes continuous mass ws connections
 * asynchronously
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

enum {
	CLIENT_IDLE,
	CLIENT_CONNECTING,
	CLIENT_AWAITING_SEND,
};

struct client {
	struct lws *wsi;
	int index;
	int state;
};

static struct lws_context *context;
static struct client clients[200];
static int interrupted, port = 443, ssl_connection = LCCSCF_USE_SSL;
static const char *server_address = "libwebsockets.org",
		  *pro = "lws-mirror-protocol";
static int concurrent = 3, conn, tries, est, errors, closed, sent, limit = 15;

struct pss {
	int conn;
};

static int
connect_client(int idx)
{
	struct lws_client_connect_info i;

	if (tries == limit) {
		lwsl_user("Reached limit... finishing\n");
		return 0;
	}

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
	i.pwsi = &clients[idx].wsi;

	clients[idx].state = CLIENT_CONNECTING;
	tries++;

	lwsl_notice("%s: connection %s:%d\n", __func__, i.address, i.port);
	if (!lws_client_connect_via_info(&i)) {
		clients[idx].wsi = NULL;
		clients[idx].state = CLIENT_IDLE;

		return 1;
	}

	return 0;
}

static int
callback_minimal_spam(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct pss *pss = (struct pss *)user;
	uint8_t ping[LWS_PRE + 125];
	int n, m;

	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT:
		for (n = 0; n < concurrent; n++) {
			clients[n].index = n;
			connect_client(n);
		}
		break;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		errors++;
		lwsl_err("CLIENT_CONNECTION_ERROR: %s (try %d, est %d, closed %d, err %d)\n",
			 in ? (char *)in : "(null)", tries, est, closed, errors);
		for (n = 0; n < concurrent; n++) {
			if (clients[n].wsi == wsi) {
				clients[n].wsi = NULL;
				clients[n].state = CLIENT_IDLE;
				connect_client(n);
				break;
			}
		}
		if (tries == closed + errors) {
			interrupted = 1;
			lws_cancel_service(lws_get_context(wsi));
		}
		break;

	/* --- client callbacks --- */

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lwsl_user("%s: established (try %d, est %d, closed %d, err %d)\n",
				__func__, tries, est, closed, errors);
		est++;
		pss->conn = conn++;
		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_CLIENT_CLOSED:
		closed++;
		if (tries == closed + errors) {
			interrupted = 1;
			lws_cancel_service(lws_get_context(wsi));
		}
		if (tries == limit) {
			lwsl_user("%s: leaving CLOSED (try %d, est %d, sent %d, closed %d, err %d)\n",
					__func__, tries, est, sent, closed, errors);
			break;
		}

		for (n = 0; n < concurrent; n++) {
			if (clients[n].wsi == wsi) {
				connect_client(n);
				lwsl_user("%s: reopening (try %d, est %d, closed %d, err %d)\n",
						__func__, tries, est, closed, errors);
				break;
			}
		}
		if (n == concurrent)
			lwsl_user("CLOSED: can't find client wsi\n");
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
		n = lws_snprintf((char *)ping + LWS_PRE, sizeof(ping) - LWS_PRE,
					  "hello %d", pss->conn);

		m = lws_write(wsi, ping + LWS_PRE, (unsigned int)n, LWS_WRITE_TEXT);
		if (m < n) {
			lwsl_err("sending ping failed: %d\n", m);

			return -1;
		}
		lws_set_timeout(wsi, PENDING_TIMEOUT_USER_OK, LWS_TO_KILL_ASYNC);
		break;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
	{
		"lws-spam-test",
		callback_minimal_spam,
		sizeof(struct pss),
		0, 0, NULL, 0
	},
	LWS_PROTOCOL_LIST_TERM
};

static struct lws_protocol_vhost_options pvo = {
        NULL,                  /* "next" pvo linked-list */
        NULL,                 /* "child" pvo linked-list */
        "lws-spam-test",        /* protocol name we belong to on this vhost */
        "OK"                     /* ignored */
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
	lwsl_user("LWS minimal ws client SPAM\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = protocols;
	info.pvo = &pvo;
#if defined(LWS_WITH_MBEDTLS) || defined(USE_WOLFSSL)
	/*
	 * OpenSSL uses the system trust store.  mbedTLS has to be told which
	 * CA to trust explicitly.
	 */
	info.client_ssl_ca_filepath = "./libwebsockets.org.cer";
#endif

	if ((p = lws_cmdline_option(argc, argv, "--server"))) {
		server_address = p;
		ssl_connection |= LCCSCF_ALLOW_SELFSIGNED;
	}

	if ((p = lws_cmdline_option(argc, argv, "--port")))
		port = atoi(p);

	if ((p = lws_cmdline_option(argc, argv, "-l")))
		limit = atoi(p);

	if ((p = lws_cmdline_option(argc, argv, "-c")))
		concurrent = atoi(p);

	if (lws_cmdline_option(argc, argv, "-n")) {
		ssl_connection = 0;
		info.options = 0;
	}

	if (concurrent < 0 ||
	    concurrent > (int)LWS_ARRAY_SIZE(clients)) {
		lwsl_err("%s: -c %d larger than max concurrency %d\n", __func__,
				concurrent, (int)LWS_ARRAY_SIZE(clients));

		return 1;
	}

	/*
	 * since we know this lws context is only ever going to be used with
	 * one client wsis / fds / sockets at a time, let lws know it doesn't
	 * have to use the default allocations for fd tables up to ulimit -n.
	 * It will just allocate for 1 internal and n (+ 1 http2 nwsi) that we
	 * will use.
	 */
	info.fd_limit_per_thread = (unsigned int)(1 + concurrent + 1);

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lwsl_notice("%s: exiting service loop\n", __func__);

	lws_context_destroy(context);

	if (tries == limit && closed == tries) {
		lwsl_user("Completed\n");
		return 0;
	}

	lwsl_err("Failed\n");

	return 1;
}
