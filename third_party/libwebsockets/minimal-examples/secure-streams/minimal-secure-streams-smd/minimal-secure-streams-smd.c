/*
 * lws-minimal-secure-streams-smd
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 *
 * This demonstrates a minimal http client using secure streams to access the
 * SMD api.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

static int interrupted, bad = 1, count_p1, count_p2, count_tx, expected = 0;
static unsigned int how_many_msg = 100, usec_interval = 1000;
static lws_sorted_usec_list_t sul_timeout;

/*
 * If the -proxy app is fulfilling our connection, then we don't need to have
 * the policy in the client.
 *
 * When we build with LWS_SS_USE_SSPC, the apis hook up to a proxy process over
 * a Unix Domain Socket.  To test that, you need to separately run the
 * ./lws-minimal-secure-streams-proxy test app on the same machine.
 */

#if !defined(LWS_SS_USE_SSPC)
static const char * const default_ss_policy =
	"{"
		"\"schema-version\":1,"
		"\"s\": ["
			"{"
				/*
				 * "captive_portal_detect" describes
				 * what to do in order to check if the path to
				 * the Internet is being interrupted by a
				 * captive portal.  If there's a larger policy
				 * fetched from elsewhere, it should also include
				 * this since it needs to be done at least after
				 * every DHCP acquisition
				 */
				"\"captive_portal_detect\": {"
					"\"endpoint\": \"connectivitycheck.android.com\","
					"\"http_url\": \"generate_204\","
					"\"port\": 80,"
					"\"protocol\": \"h1\","
					"\"http_method\": \"GET\","
					"\"opportunistic\": true,"
					"\"http_expect\": 204,"
					"\"http_fail_redirect\": true"
				"}"
			"}"
		"]"
	"}"
;

#endif

typedef struct myss {
	struct lws_ss_handle 		*ss;
	void				*opaque_data;
	/* ... application specific state ... */
	lws_sorted_usec_list_t		sul;
	char				alternate;
} myss_t;


/* secure streams payload interface */

static lws_ss_state_return_t
myss_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	/*
	 * Call the helper to translate into a real smd message and forward to
	 * this context / process smd participants... except us, since we
	 * definitely already received it
	 */

	if (lws_smd_ss_rx_forward(userobj, buf, len))
		lwsl_warn("%s: forward failed\n", __func__);

	count_p1++;

	return LWSSSSRET_OK;
}

static void
sul_tx_periodic_cb(lws_sorted_usec_list_t *sul)
{
	myss_t *m = lws_container_of(sul, myss_t, sul);

	lwsl_info("%s: requesting TX\n", __func__);
	if (lws_ss_request_tx(m->ss))
		lwsl_info("%s: req failed\n", __func__);
}

static lws_ss_state_return_t
myss_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	int *flags)
{
	myss_t *m = (myss_t *)userobj;

	lwsl_info("%s: sending SS smd\n", __func__);

	/*
	 * The SS RX isn't going to see INTERACTION messages, because its class
	 * filter doesn't accept INTERACTION class messages.  The direct
	 * participant we also set up for the test will see them though.
	 *
	 * Let's alternate between sending NETWORK class smd messages and
	 * INTERACTION so we can test both rx paths
	 */

	m->alternate++;

	if (m->alternate == 4) {
		/*
		 * after a few, let's request a CPD check
		 */

		if (lws_smd_ss_msg_printf(lws_ss_tag(m->ss), buf, len, LWSSMDCL_NETWORK,
					  "{\"trigger\": \"cpdcheck\", "
					   "\"src\":\"SS-test\"}"))
			return LWSSSSRET_TX_DONT_SEND;
	} else
		if (lws_smd_ss_msg_printf(lws_ss_tag(m->ss), buf, len,
					  (m->alternate & 1) ? LWSSMDCL_NETWORK :
							       LWSSMDCL_INTERACTION,
					  (m->alternate & 1) ?
					       "{\"class\":\"NETWORK\",\"x\":%d}" :
					       "{\"class\":\"INTERACTION\",\"x\":%d}",
					       count_tx))
			return LWSSSSRET_TX_DONT_SEND;

	*flags = LWSSS_FLAG_SOM | LWSSS_FLAG_EOM;

	count_tx++;

	lws_sul_schedule(lws_ss_get_context(m->ss), 0, &m->sul,
			 sul_tx_periodic_cb, usec_interval);

	return LWSSSSRET_OK;
}

static lws_ss_state_return_t
myss_state(void *userobj, void *h_src, lws_ss_constate_t state,
	   lws_ss_tx_ordinal_t ack)
{
	myss_t *m = (myss_t *)userobj;

	lwsl_notice("%s: %s: %s (%d), ord 0x%x\n", __func__, lws_ss_tag(m->ss),
		    lws_ss_state_name((int)state), state, (unsigned int)ack);

	if (state == LWSSSCS_DESTROYING) {
		lws_sul_cancel(&m->sul);
		return LWSSSSRET_OK;
	}

	if (state == LWSSSCS_CONNECTED) {
		lwsl_notice("%s: CONNECTED\n", __func__);
		lws_sul_schedule(lws_ss_get_context(m->ss), 0, &m->sul,
				 sul_tx_periodic_cb, 1);
		return LWSSSSRET_OK;
	}

	return LWSSSSRET_OK;
}

static const lws_ss_info_t ssi_lws_smd = {
	.handle_offset		  = offsetof(myss_t, ss),
	.opaque_user_data_offset  = offsetof(myss_t, opaque_data),
	.rx			  = myss_rx,
	.tx			  = myss_tx,
	.state			  = myss_state,
	.user_alloc		  = sizeof(myss_t),
	.streamtype		  = LWS_SMD_STREAMTYPENAME,
	.manual_initial_tx_credit = LWSSMDCL_SYSTEM_STATE |
				    LWSSMDCL_METRICS |
				    LWSSMDCL_NETWORK,
};

/* for comparison, this is a non-SS lws_smd participant */

static int
direct_smd_cb(void *opaque, lws_smd_class_t _class, lws_usec_t timestamp,
	      void *buf, size_t len)
{
	struct lws_context **pctx = (struct lws_context **)opaque;

//	lwsl_notice("%s: class: 0x%x, ts: %llu\n", __func__, _class,
//		  (unsigned long long)timestamp);
//	lwsl_hexdump_notice(buf, len);

	count_p2++;

	if (_class != LWSSMDCL_SYSTEM_STATE)
		return 0;

	if (!lws_json_simple_strcmp(buf, len, "\"state\":", "OPERATIONAL")) {

#if !defined(LWS_SS_USE_SSPC)
		/*
		 * Let's trigger a CPD check, just as a test.  SS can't see it
		 * anyway since it doesn't listen for NETWORK but the direct /
		 * local participant will see it and the result
		 *
		 * This process doesn't run the smd / captive portal action
		 * when it's a client of the SS proxy.  SMD has to be passed
		 * via the SS _lws_smd proxied connection in that case.
		 */
		(void)lws_smd_msg_printf(*pctx, LWSSMDCL_NETWORK,
				   "{\"trigger\": \"cpdcheck\", \"src\":\"direct-test\"}");
#endif

		/*
		 * Create the SS link to lws_smd... notice in ssi_lws_smd
		 * above, we tell this link to use a class filter that excludes
		 * NETWORK messages.
		 */

		if (lws_ss_create(*pctx, 0, &ssi_lws_smd, NULL, NULL, NULL, NULL)) {
			lwsl_err("%s: failed to create secure stream\n",
				 __func__);
			interrupted = 1;
			lws_cancel_service(*pctx);
			return -1;
		}
	}

	return 0;
}


static void
sul_timeout_cb(lws_sorted_usec_list_t *sul)
{
	lwsl_notice("%s: test finishing\n", __func__);
	interrupted = 1;
}


static void
sigint_handler(int sig)
{
	interrupted = 1;
}

extern int smd_ss_multi_test(int argc, const char **argv);

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	const char *p;

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info);

#if defined(LWS_SS_USE_SSPC)
	if (lws_cmdline_option(argc, argv, "--multi"))
		return smd_ss_multi_test(argc, argv);
#endif

	lws_cmdline_option_handle_builtin(argc, argv, &info);

	if ((p = lws_cmdline_option(argc, argv, "--count")))
		how_many_msg = (unsigned int)atol(p);

	if ((p = lws_cmdline_option(argc, argv, "--interval")))
		usec_interval = (unsigned int)atol(p);

	lwsl_user("LWS Secure Streams SMD test client [-d<verb>]: "
		  "%u msgs at %uus interval\n", how_many_msg, usec_interval);

	info.fd_limit_per_thread	= 1 + 6 + 1;
	info.port			= CONTEXT_PORT_NO_LISTEN;
#if !defined(LWS_SS_USE_SSPC)
	info.pss_policies_json		= default_ss_policy;
#else
	info.protocols			= lws_sspc_protocols;
	{
		/* connect to ssproxy via UDS by default, else via
		 * tcp connection to this port */
		if ((p = lws_cmdline_option(argc, argv, "-p")))
			info.ss_proxy_port = (uint16_t)atoi(p);

		/* UDS "proxy.ss.lws" in abstract namespace, else this socket
		 * path; when -p given this can specify the network interface
		 * to bind to */
		if ((p = lws_cmdline_option(argc, argv, "-i")))
			info.ss_proxy_bind = p;

		/* if -p given, -a specifies the proxy address to connect to */
		if ((p = lws_cmdline_option(argc, argv, "-a")))
			info.ss_proxy_address = p;
	}
#endif
	info.options			= LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
					  LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

	info.early_smd_cb		= direct_smd_cb;
	info.early_smd_class_filter	= 0xffffffff;
	info.early_smd_opaque		= &context;

	/* create the context */

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

#if defined(LWS_SS_USE_SSPC)
	if (!lws_create_vhost(context, &info)) {
		lwsl_err("%s: failed to create default vhost\n", __func__);
		goto bail;
	}
#endif

	/* set up the test timeout */

	lws_sul_schedule(context, 0, &sul_timeout, sul_timeout_cb,
			 (how_many_msg * (usec_interval + 50000)) + LWS_US_PER_SEC);

	/* the event loop */

	while (lws_service(context, 0) >= 0 && !interrupted)
		;

	/* compare what happened with what we expect */

#if defined(LWS_SS_USE_SSPC)
	/* if SSPC
	 *
	 *  - the SS _lws_smd link does not enable INTERACTION class, so doesn't
	 *    see these messages (count_p1 is half count_tx)
	 *
	 *  - the direct smd participant sees local state, but it doesn't send
	 *    any local CPD request, since as a client it doesn't do CPD
	 *    directly (count_p2 -= 1 compared to non-SSPC)
	 *
	 *  - one CPD trigger is sent on the proxied SS link (countp1 += 1)
	 */
	if (count_p1 >= 6 && count_p2 >= 11 && count_tx >= 12)
#else
	/* if not SSPC, then we can see direct smd activity */
	if (count_p1 >= 2 && count_p2 >= 15 && count_tx >= 5)
#endif
		bad = 0;

	lwsl_notice("%d %d %d\n", count_p1, count_p2, count_tx);

#if defined(LWS_SS_USE_SSPC)
bail:
#endif
	lws_context_destroy(context);

	if ((p = lws_cmdline_option(argc, argv, "--expected-exit")))
		expected = atoi(p);

	if (bad == expected) {
		lwsl_user("Completed: OK (seen expected %d)\n", expected);
		return 0;
	}

	lwsl_err("Completed: failed: exit %d, expected %d\n", bad, expected);

	return 1;
}
