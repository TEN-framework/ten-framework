/*
 * lws-minimal-http-client
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates the a minimal http client using lws.
 *
 * It visits https://warmcat.com/ and receives the html page there.  You
 * can dump the page data by changing the #if 0 below.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

static int interrupted, bad = 1, status, conmon;
#if defined(LWS_WITH_HTTP2)
static int long_poll;
#endif
static struct lws *client_wsi;
static const char *ba_user, *ba_password;

static const lws_retry_bo_t retry = {
	.secs_since_valid_ping = 3,
	.secs_since_valid_hangup = 10,
};

#if defined(LWS_WITH_CONMON)
void
dump_conmon_data(struct lws *wsi)
{
	const struct addrinfo *ai;
	struct lws_conmon cm;
	char ads[48];

	lws_conmon_wsi_take(wsi, &cm);

	lws_sa46_write_numeric_address(&cm.peer46, ads, sizeof(ads));
	lwsl_notice("%s: peer %s, dns: %uus, sockconn: %uus, tls: %uus, txn_resp: %uus\n",
		    __func__, ads,
		    (unsigned int)cm.ciu_dns,
		    (unsigned int)cm.ciu_sockconn,
		    (unsigned int)cm.ciu_tls,
		    (unsigned int)cm.ciu_txn_resp);

	ai = cm.dns_results_copy;
	while (ai) {
		lws_sa46_write_numeric_address((lws_sockaddr46 *)ai->ai_addr, ads, sizeof(ads));
		lwsl_notice("%s: DNS %s\n", __func__, ads);
		ai = ai->ai_next;
	}

	/*
	 * This destroys the DNS list in the lws_conmon that we took
	 * responsibility for when we used lws_conmon_wsi_take()
	 */

	lws_conmon_release(&cm);
}
#endif

static const char *ua = "Mozilla/5.0 (X11; Linux x86_64) "
			"AppleWebKit/537.36 (KHTML, like Gecko) "
			"Chrome/51.0.2704.103 Safari/537.36",
		  *acc = "*/*";

static int
callback_http(struct lws *wsi, enum lws_callback_reasons reason,
	      void *user, void *in, size_t len)
{
	switch (reason) {

	/* because we are protocols[0] ... */
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		interrupted = 1;
		bad = 3; /* connection failed before we could make connection */
		lws_cancel_service(lws_get_context(wsi));

#if defined(LWS_WITH_CONMON)
	if (conmon)
		dump_conmon_data(wsi);
#endif
		break;

	case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
		{
			char buf[128];

			lws_get_peer_simple(wsi, buf, sizeof(buf));
			status = (int)lws_http_client_http_response(wsi);

			lwsl_user("Connected to %s, http response: %d\n",
					buf, status);
		}
#if defined(LWS_WITH_HTTP2)
		if (long_poll) {
			lwsl_user("%s: Client entering long poll mode\n", __func__);
			lws_h2_client_stream_long_poll_rxonly(wsi);
		}
#endif

		if (lws_fi_user_wsi_fi(wsi, "user_reject_at_est"))
			return -1;

		break;

	/* you only need this if you need to do Basic Auth */
	case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
	{
		unsigned char **p = (unsigned char **)in, *end = (*p) + len;

		if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_USER_AGENT,
				(unsigned char *)ua, (int)strlen(ua), p, end))
			return -1;

		if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_ACCEPT,
				(unsigned char *)acc, (int)strlen(acc), p, end))
			return -1;
#if defined(LWS_WITH_HTTP_BASIC_AUTH)
		{
		char b[128];

		if (!ba_user || !ba_password)
			break;

		if (lws_http_basic_auth_gen(ba_user, ba_password, b, sizeof(b)))
			break;
		if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_AUTHORIZATION,
				(unsigned char *)b, (int)strlen(b), p, end))
			return -1;
		}
#endif
		break;
	}

	/* chunks of chunked content, with header removed */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
		lwsl_user("RECEIVE_CLIENT_HTTP_READ: read %d\n", (int)len);
#if defined(LWS_WITH_HTTP2)
		if (long_poll) {
			char dotstar[128];
			lws_strnncpy(dotstar, (const char *)in, len,
				     sizeof(dotstar));
			lwsl_notice("long poll rx: %d '%s'\n", (int)len,
					dotstar);
		}
#endif
#if 0
		lwsl_hexdump_notice(in, len);
#endif

		return 0; /* don't passthru */

	/* uninterpreted http content */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
		{
			char buffer[1024 + LWS_PRE];
			char *px = buffer + LWS_PRE;
			int lenx = sizeof(buffer) - LWS_PRE;

			if (lws_fi_user_wsi_fi(wsi, "user_reject_at_rx"))
				return -1;

			if (lws_http_client_read(wsi, &px, &lenx) < 0)
				return -1;
		}
		return 0; /* don't passthru */

	case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
		lwsl_user("LWS_CALLBACK_COMPLETED_CLIENT_HTTP\n");
		interrupted = 1;
		bad = status != 200;
		lws_cancel_service(lws_get_context(wsi)); /* abort poll wait */
		break;

	case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
		interrupted = 1;
		bad = status != 200;
		lws_cancel_service(lws_get_context(wsi)); /* abort poll wait */
#if defined(LWS_WITH_CONMON)
		if (conmon)
			dump_conmon_data(wsi);
#endif
		break;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
	{
		"http",
		callback_http,
		0, 0, 0, NULL, 0
	},
	LWS_PROTOCOL_LIST_TERM
};

static void
sigint_handler(int sig)
{
	interrupted = 1;
}

struct args {
	int argc;
	const char **argv;
};

static int
system_notify_cb(lws_state_manager_t *mgr, lws_state_notify_link_t *link,
		   int current, int target)
{
	struct lws_context *context = mgr->parent;
	struct lws_client_connect_info i;
	struct args *a = lws_context_user(context);
	const char *p;

	if (current != LWS_SYSTATE_OPERATIONAL || target != LWS_SYSTATE_OPERATIONAL)
		return 0;

	lwsl_info("%s: operational\n", __func__);

	memset(&i, 0, sizeof i); /* otherwise uninitialized garbage */
	i.context = context;
	if (!lws_cmdline_option(a->argc, a->argv, "-n")) {
		i.ssl_connection = LCCSCF_USE_SSL;
#if defined(LWS_WITH_HTTP2)
		/* requires h2 */
		if (lws_cmdline_option(a->argc, a->argv, "--long-poll")) {
			lwsl_user("%s: long poll mode\n", __func__);
			long_poll = 1;
		}
#endif
	}

	if (lws_cmdline_option(a->argc, a->argv, "-l")) {
		i.port = 7681;
		i.address = "localhost";
		i.ssl_connection |= LCCSCF_ALLOW_SELFSIGNED;
	} else {
		i.port = 443;
		i.address = "warmcat.com";
	}

	if (lws_cmdline_option(a->argc, a->argv, "--nossl"))
		i.ssl_connection = 0;

	i.ssl_connection |= LCCSCF_H2_QUIRK_OVERFLOWS_TXCR |
			    LCCSCF_ACCEPT_TLS_DOWNGRADE_REDIRECTS |
			    LCCSCF_H2_QUIRK_NGHTTP2_END_STREAM;

	i.alpn = "h2,http/1.1";
	if (lws_cmdline_option(a->argc, a->argv, "--h1"))
		i.alpn = "http/1.1";

	if (lws_cmdline_option(a->argc, a->argv, "--h2-prior-knowledge"))
		i.ssl_connection |= LCCSCF_H2_PRIOR_KNOWLEDGE;

	if ((p = lws_cmdline_option(a->argc, a->argv, "-p")))
		i.port = atoi(p);

	if ((p = lws_cmdline_option(a->argc, a->argv, "--user")))
		ba_user = p;
	if ((p = lws_cmdline_option(a->argc, a->argv, "--password")))
		ba_password = p;

	if (lws_cmdline_option(a->argc, a->argv, "-j"))
		i.ssl_connection |= LCCSCF_ALLOW_SELFSIGNED;

	if (lws_cmdline_option(a->argc, a->argv, "-k"))
		i.ssl_connection |= LCCSCF_ALLOW_INSECURE;

	if (lws_cmdline_option(a->argc, a->argv, "-b"))
		i.ssl_connection |= LCCSCF_CACHE_COOKIES;

	if (lws_cmdline_option(a->argc, a->argv, "-m"))
		i.ssl_connection |= LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;

	if (lws_cmdline_option(a->argc, a->argv, "-e"))
		i.ssl_connection |= LCCSCF_ALLOW_EXPIRED;

	if ((p = lws_cmdline_option(a->argc, a->argv, "-f"))) {
		i.ssl_connection |= LCCSCF_H2_MANUAL_RXFLOW;
		i.manual_initial_tx_credit = atoi(p);
		lwsl_notice("%s: manual peer tx credit %d\n", __func__,
				i.manual_initial_tx_credit);
	}

#if defined(LWS_WITH_CONMON)
	if (lws_cmdline_option(a->argc, a->argv, "--conmon")) {
		i.ssl_connection |= LCCSCF_CONMON;
		conmon = 1;
	}
#endif

	/* the default validity check is 5m / 5m10s... -v = 3s / 10s */

	if (lws_cmdline_option(a->argc, a->argv, "-v"))
		i.retry_and_idle_policy = &retry;

	if ((p = lws_cmdline_option(a->argc, a->argv, "--server")))
		i.address = p;

	if ((p = lws_cmdline_option(a->argc, a->argv, "--path")))
		i.path = p;
	else
		i.path = "/";

	i.host = i.address;
	i.origin = i.address;
	i.method = "GET";

	i.protocol = protocols[0].name;
	i.pwsi = &client_wsi;
	i.fi_wsi_name = "user";

	if (!lws_client_connect_via_info(&i)) {
		lwsl_err("Client creation failed\n");
		interrupted = 1;
		bad = 2; /* could not even start client connection */
		lws_cancel_service(context);

		return 1;
	}

	return 0;
}

int main(int argc, const char **argv)
{
	lws_state_notify_link_t notifier = { { NULL, NULL, NULL },
					     system_notify_cb, "app" };
	lws_state_notify_link_t *na[] = { &notifier, NULL };
	struct lws_context_creation_info info;
	struct lws_context *context;
	int n = 0, expected = 0;
	struct args args;
	const char *p;
	// uint8_t memcert[4096];

	args.argc = argc;
	args.argv = argv;

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	lws_cmdline_option_handle_builtin(argc, argv, &info);

	lwsl_user("LWS minimal http client [-d<verbosity>] [-l] [--h1]\n");

	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
		       LWS_SERVER_OPTION_H2_JUST_FIX_WINDOW_UPDATE_OVERFLOW;
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = protocols;
	info.user = &args;
	info.register_notifier_list = na;
	info.connect_timeout_secs = 30;

#if defined(LWS_WITH_CACHE_NSCOOKIEJAR)
	info.http_nsc_filepath = "./cookies.txt";
	if ((p = lws_cmdline_option(argc, argv, "-c")))
		info.http_nsc_filepath = p;
#endif

	/*
	 * since we know this lws context is only ever going to be used with
	 * one client wsis / fds / sockets at a time, let lws know it doesn't
	 * have to use the default allocations for fd tables up to ulimit -n.
	 * It will just allocate for 1 internal and 1 (+ 1 http2 nwsi) that we
	 * will use.
	 */
	info.fd_limit_per_thread = 1 + 1 + 1;

#if defined(LWS_WITH_MBEDTLS) || defined(USE_WOLFSSL)
	/*
	 * OpenSSL uses the system trust store.  mbedTLS has to be told which
	 * CA to trust explicitly.
	 */
	if (lws_cmdline_option(argc, argv, "-w"))
		/* option to confirm we are validating against the right cert */
		info.client_ssl_ca_filepath = "./wrong.cer";
	else
		info.client_ssl_ca_filepath = "./warmcat.com.cer";
#endif
#if 0
	n = open("./warmcat.com.cer", O_RDONLY);
	if (n >= 0) {
		info.client_ssl_ca_mem_len = read(n, memcert, sizeof(memcert));
		info.client_ssl_ca_mem = memcert;
		close(n);
		n = 0;
		memcert[info.client_ssl_ca_mem_len++] = '\0';
	}
#endif
	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		bad = 5;
		goto bail;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);

bail:
	if ((p = lws_cmdline_option(argc, argv, "--expected-exit")))
		expected = atoi(p);

	if (bad == expected) {
		lwsl_user("Completed: OK (seen expected %d)\n", expected);
		return 0;
	} else
		lwsl_err("Completed: failed: exit %d, expected %d\n", bad, expected);

	return 1;
}
