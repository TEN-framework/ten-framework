/*
 * lws-minimal-http-client-h2-rxflow
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
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

static int interrupted, bad = 1, status, each = 1024;
static struct lws *client_wsi;

static const lws_retry_bo_t retry = {
	.secs_since_valid_ping = 3,
	.secs_since_valid_hangup = 10,
};

struct pss {
	lws_sorted_usec_list_t sul;
	struct lws *wsi;
};

/*
 * Once we're established, we ask the server for another 1KB every 250ms
 * until we have it all.
 */

static void
drain_cb(lws_sorted_usec_list_t *sul)
{
	struct pss *pss = lws_container_of(sul, struct pss, sul);

	lws_wsi_tx_credit(pss->wsi, LWSTXCR_PEER_TO_US, each);

	lws_sul_schedule(lws_get_context(pss->wsi), 0, &pss->sul, drain_cb,
			 250 * LWS_US_PER_MS);
}


static int
callback_http(struct lws *wsi, enum lws_callback_reasons reason,
	      void *user, void *in, size_t len)
{
	struct pss *pss = (struct pss *)user;

	switch (reason) {

	/* because we are protocols[0] ... */
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		interrupted = 1;
		break;

	case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
		{
			char buf[128];

			lws_get_peer_simple(wsi, buf, sizeof(buf));
			status = (int)lws_http_client_http_response(wsi);

			lwsl_user("Connected to %s, http response: %d\n",
					buf, status);
		}
		pss->wsi = wsi;
		lws_sul_schedule(lws_get_context(wsi), 0, &pss->sul, drain_cb,
				 250 * LWS_US_PER_MS);
		break;

	/* chunks of chunked content, with header removed */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
		lwsl_user("RECEIVE_CLIENT_HTTP_READ: read %d\n", (int)len);

#if 0  /* enable to dump the html */
		{
			const char *p = in;

			while (len--)
				if (*p < 0x7f)
					putchar(*p++);
				else
					putchar('.');
		}
#endif
		return 0; /* don't passthru */

	/* uninterpreted http content */
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
		{
			char buffer[1024 + LWS_PRE];
			char *px = buffer + LWS_PRE;
			int lenx = sizeof(buffer) - LWS_PRE;

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
		lws_sul_cancel(&pss->sul);
		lws_cancel_service(lws_get_context(wsi)); /* abort poll wait */
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
		sizeof(struct pss),
		0, 0, NULL, 0
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
	if (!lws_cmdline_option(a->argc, a->argv, "-n"))
		i.ssl_connection = LCCSCF_USE_SSL;

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
			    LCCSCF_H2_QUIRK_NGHTTP2_END_STREAM;

	i.alpn = "h2";
	if (lws_cmdline_option(a->argc, a->argv, "--h1"))
		i.alpn = "http/1.1";

	if ((p = lws_cmdline_option(a->argc, a->argv, "-p")))
		i.port = atoi(p);

	if (lws_cmdline_option(a->argc, a->argv, "-j"))
		i.ssl_connection |= LCCSCF_ALLOW_SELFSIGNED;

	if (lws_cmdline_option(a->argc, a->argv, "-k"))
		i.ssl_connection |= LCCSCF_ALLOW_INSECURE;

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

	if ((p = lws_cmdline_option(a->argc, a->argv, "--each")))
		each = atoi(p);

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

	return !lws_client_connect_via_info(&i);
}

int main(int argc, const char **argv)
{
	lws_state_notify_link_t notifier = { { NULL, NULL, NULL },
						system_notify_cb, "app" };
	lws_state_notify_link_t *na[] = { &notifier, NULL };
	struct lws_context_creation_info info;
	struct lws_context *context;
	struct args args;
	int n = 0;
	// uint8_t memcert[4096];

	args.argc = argc;
	args.argv = argv;

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	lws_cmdline_option_handle_builtin(argc, argv, &info);

	lwsl_user("LWS minimal http client [-d<verbosity>] [-l] [--h1]\n");

	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = protocols;
	info.user = &args;
	info.register_notifier_list = na;
	info.timeout_secs = 10;
	info.connect_timeout_secs = 30;

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
		return 1;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);
	lwsl_user("Completed: %s\n", bad ? "failed" : "OK");

	return bad;
}
