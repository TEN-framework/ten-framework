/*
 * lws-minimal-secure-streams
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 *
 * This demonstrates a minimal http client using secure streams api.
 *
 * It visits https://warmcat.com/ and receives the html page there.
 *
 * This example is built two different ways from the same source... one includes
 * the policy everything needed to fulfil the stream directly.  The other -client
 * variant has no policy itself and some other minor init changes, and connects
 * to the -proxy example to actually get the connection done.
 *
 * In the -client build case, the example does not even init the tls libraries
 * since the proxy part will take care of all that.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

/*
 * uncomment to force network traffic through 127.0.0.1:1080
 *
 * On your local machine, you can run a SOCKS5 proxy like this
 *
 * $ ssh -N -D 0.0.0.0:1080 localhost -v
 *
 * If enabled, this also fetches a remote policy that also
 * specifies that all traffic should go through the remote
 * proxy.
 */
// #define VIA_LOCALHOST_SOCKS

static int interrupted, bad = 1, force_cpd_fail_portal,
	   force_cpd_fail_no_internet, test_respmap;
static const char *streamtype = "mintest";
static unsigned int timeout_ms = 3000;
static lws_state_notify_link_t nl;

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
	  "\"release\":"			"\"01234567\","
	  "\"product\":"			"\"myproduct\","
	  "\"schema-version\":"			"1,"
#if defined(VIA_LOCALHOST_SOCKS)
	  "\"via-socks5\":"                     "\"127.0.0.1:1080\","
#endif

	  "\"retry\": ["	/* named backoff / retry strategies */
		"{\"default\": {"
			"\"backoff\": ["	 "1000,"
						 "2000,"
						 "3000,"
						 "5000,"
						"10000"
				"],"
			"\"conceal\":"		"5,"
			"\"jitterpc\":"		"20,"
			"\"svalidping\":"	"30,"
			"\"svalidhup\":"	"35"
		"}}"
	  "],"
	  "\"certs\": [" /* named individual certificates in BASE64 DER */
		/*
		 * Let's Encrypt certs for warmcat.com / libwebsockets.org
		 *
		 * We fetch the real policy from there using SS and switch to
		 * using that.
		 */
		"{\"isrg_root_x1\": \""
	"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw"
	"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh"
	"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4"
	"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu"
	"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY"
	"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc"
	"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+"
	"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U"
	"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW"
	"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH"
	"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC"
	"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv"
	"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn"
	"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn"
	"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw"
	"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI"
	"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV"
	"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq"
	"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL"
	"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ"
	"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK"
	"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5"
	"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur"
	"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC"
	"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc"
	"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq"
	"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA"
	"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d"
	"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc="
	  "\"}"
	  "],"
	  "\"trust_stores\": [" /* named cert chains */
		"{"
			"\"name\": \"le_via_isrg\","
			"\"stack\": ["
				"\"isrg_root_x1\""
			"]"
		"}"
	  "],"
	  "\"s\": ["
	  	/*
		 * "fetch_policy" decides from where the real policy
		 * will be fetched, if present.  Otherwise the initial
		 * policy is treated as the whole, hardcoded, policy.
		 */
		"{\"fetch_policy\": {"
			"\"endpoint\":"		"\"warmcat.com\","
			"\"port\":"		"443,"
			"\"protocol\":"		"\"h1\","
			"\"http_method\":"	"\"GET\","
#if defined(VIA_LOCALHOST_SOCKS)
			"\"http_url\":"		"\"policy/minimal-proxy-socks.json\","
#else
			"\"http_url\":"		"\"policy/minimal-proxy-v4.2-v2.json\","
#endif
			"\"tls\":"		"true,"
			"\"opportunistic\":"	"true,"
			"\"retry\":"		"\"default\","
			"\"tls_trust_store\":"	"\"le_via_isrg\""
		"}},{"
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
                "}}"
	"]}"
;

#endif

typedef struct myss {
	struct lws_ss_handle 		*ss;
	void				*opaque_data;
	/* ... application specific state ... */
	lws_sorted_usec_list_t		sul;
} myss_t;

#if !defined(LWS_SS_USE_SSPC)

static const char *canned_root_token_payload =
	"grant_type=refresh_token"
	"&refresh_token=Atzr|IwEBIJedGXjDqsU_vMxykqOMg"
	"SHfYe3CPcedueWEMWSDMaDnEmiW8RlR1Kns7Cb4B-TOSnqp7ifVsY4BMY2B8tpHfO39XP"
	"zfu9HapGjTR458IyHX44FE71pWJkGZ79uVBpljP4sazJuk8XS3Oe_yLnm_DIO6fU1nU3Y"
	"0flYmsOiOAQE_gRk_pdlmEtHnpMA-9rLw3mkY5L89Ty9kUygBsiFaYatouROhbsTn8-jW"
	"k1zZLUDpT6ICtBXSnrCIg0pUbZevPFhTwdXd6eX-u4rq0W-XaDvPWFO7au-iPb4Zk5eZE"
	"iX6sissYrtNmuEXc2uHu7MnQO1hHCaTdIO2CANVumf-PHSD8xseamyh04sLV5JgFzY45S"
	"KvKMajiUZuLkMokOx86rjC2Hdkx5DO7G-dbG1ufBDG-N79pFMSs7Ck5pc283IdLoJkCQc"
	"AGvTX8o8I29QqkcGou-9TKhOJmpX8As94T61ok0UqqEKPJ7RhfQHHYdCtsdwxgvfVr9qI"
	"xL_hDCcTho8opCVX-6QhJHl6SQFlTw13"
	"&client_id="
		"amzn1.application-oa2-client.4823334c434b4190a2b5a42c07938a2d";

#endif

/* secure streams payload interface */

static lws_ss_state_return_t
myss_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	myss_t *m = (myss_t *)userobj;
	const char *md_srv = "not set", *md_test = "not set";
	size_t md_srv_len = 7, md_test_len = 7;

	if (flags & LWSSS_FLAG_PERF_JSON) {
		lwsl_user("%.*s\n", (int)len, (const char *)buf);

		return LWSSSSRET_OK;
	}

	lws_ss_get_metadata(m->ss, "srv", (const void **)&md_srv, &md_srv_len);
	lws_ss_get_metadata(m->ss, "test", (const void **)&md_test, &md_test_len);

	lwsl_user("%s: len %d, flags: %d, srv: %.*s, test: %.*s\n", __func__,
		  (int)len, flags, (int)md_srv_len, md_srv,
		  (int)md_test_len, md_test);
	lwsl_hexdump_info(buf, len);

	/*
	 * If we received the whole message, for our example it means
	 * we are done.
	 */
	if (flags & LWSSS_FLAG_EOM) {
		bad = 0;
		interrupted = 1;
	}

	return LWSSSSRET_OK;
}

static lws_ss_state_return_t
myss_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	int *flags)
{
	//myss_t *m = (myss_t *)userobj;

	/* in this example, we don't send stuff */

	return LWSSSSRET_TX_DONT_SEND;
}

static lws_ss_state_return_t
myss_state(void *userobj, void *sh, lws_ss_constate_t state,
	   lws_ss_tx_ordinal_t ack)
{
	myss_t *m = (myss_t *)userobj;

	lwsl_user("%s: %s (%d), ord 0x%x\n", __func__,
		  lws_ss_state_name((int)state), state, (unsigned int)ack);

	switch (state) {
	case LWSSSCS_CREATING:
		return lws_ss_client_connect(m->ss);

	case LWSSSCS_CONNECTING:
		lws_ss_start_timeout(m->ss, timeout_ms);
		if (lws_ss_set_metadata(m->ss, "uptag", "myuptag123", 10))
			/* can fail, eg due to OOM, retry later if so */
			return LWSSSSRET_DISCONNECT_ME;

		if (lws_ss_set_metadata(m->ss, "ctype", "myctype", 7))
			/* can fail, eg due to OOM, retry later if so */
			return LWSSSSRET_DISCONNECT_ME;
		break;

	case LWSSSCS_ALL_RETRIES_FAILED:
		/* if we're out of retries, we want to close the app and FAIL */
		interrupted = 1;
		bad = 2;
		break;

	case LWSSSCS_QOS_ACK_REMOTE:
		lwsl_notice("%s: LWSSSCS_QOS_ACK_REMOTE\n", __func__);
		break;

	case LWSSSCS_TIMEOUT:
		lwsl_notice("%s: LWSSSCS_TIMEOUT\n", __func__);
		/* if we're out of time */
		interrupted = 1;
		bad = 3;
		break;

	case LWSSSCS_USER_BASE:
		lwsl_notice("%s: LWSSSCS_USER_BASE\n", __func__);
		break;

	default:
		break;
	}

	return LWSSSSRET_OK;
}

static int
app_system_state_nf(lws_state_manager_t *mgr, lws_state_notify_link_t *link,
		    int current, int target)
{
	struct lws_context *context = lws_system_context_from_system_mgr(mgr);
#if !defined(LWS_SS_USE_SSPC)

	lws_system_blob_t *ab = lws_system_get_blob(context,
				LWS_SYSBLOB_TYPE_AUTH, 1 /* AUTH_IDX_ROOT */);
	size_t size;
#endif

	/*
	 * For the things we care about, let's notice if we are trying to get
	 * past them when we haven't solved them yet, and make the system
	 * state wait while we trigger the dependent action.
	 */
	switch (target) {

#if !defined(LWS_SS_USE_SSPC)

	/*
	 * The proxy takes responsibility for this stuff if we get things
	 * done through that
	 */

	case LWS_SYSTATE_INITIALIZED: /* overlay on the hardcoded policy */
	case LWS_SYSTATE_POLICY_VALID: /* overlay on the loaded policy */

		if (target != current)
			break;

		if (force_cpd_fail_portal)

			/* this makes it look like we're behind a captive portal
			 * because the overriden address does a redirect */

			lws_ss_policy_overlay(context,
				      "{\"s\": [{\"captive_portal_detect\": {"
				         "\"endpoint\": \"google.com\","
					 "\"http_url\": \"/\","
					 "\"port\": 80"
				      "}}]}");

		if (force_cpd_fail_no_internet)

			/* this looks like no internet, because the overridden
			 * port doesn't have anything that will connect to us */

			lws_ss_policy_overlay(context,
				      "{\"s\": [{\"captive_portal_detect\": {"
					 "\"endpoint\": \"warmcat.com\","
					 "\"http_url\": \"/\","
					 "\"port\": 999"
				      "}}]}");
		break;

	case LWS_SYSTATE_REGISTERED:
		size = lws_system_blob_get_size(ab);
		if (size)
			break;

		/* let's register our canned root token so auth can use it */
		lws_system_blob_direct_set(ab,
				(const uint8_t *)canned_root_token_payload,
				strlen(canned_root_token_payload));
		break;

#endif

	case LWS_SYSTATE_OPERATIONAL:
		if (current == LWS_SYSTATE_OPERATIONAL) {
			lws_ss_info_t ssi;

			/* We're making an outgoing secure stream ourselves */

			memset(&ssi, 0, sizeof(ssi));
			ssi.handle_offset = offsetof(myss_t, ss);
			ssi.opaque_user_data_offset = offsetof(myss_t,
							       opaque_data);
			ssi.rx = myss_rx;
			ssi.tx = myss_tx;
			ssi.state = myss_state;
			ssi.user_alloc = sizeof(myss_t);
			ssi.streamtype = test_respmap ? "respmap" : streamtype;

			if (lws_ss_create(context, 0, &ssi, NULL, NULL,
					  NULL, NULL)) {
				lwsl_err("%s: failed to create secure stream\n",
					 __func__);
				return -1;
			}
		}
		break;
	}

	return 0;
}

static lws_state_notify_link_t * const app_notifier_list[] = {
	&nl, NULL
};

#if defined(LWS_WITH_SYS_METRICS)

static int
my_metric_report(lws_metric_pub_t *mp)
{
	lws_metric_bucket_t *sub = mp->u.hist.head;
	char buf[192];

	do {
		if (lws_metrics_format(mp, &sub, buf, sizeof(buf)))
			lwsl_user("%s: %s\n", __func__, buf);
	} while ((mp->flags & LWSMTFL_REPORT_HIST) && sub);

	/* 0 = leave metric to accumulate, 1 = reset the metric */

	return 1;
}

static const lws_system_ops_t system_ops = {
	.metric_report = my_metric_report,
};

#endif

static void
sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	int n = 0, expected = 0;
	const char *p;

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info);
	lws_cmdline_option_handle_builtin(argc, argv, &info);

	lwsl_user("LWS secure streams test client PERF [-d<verb>]\n");

	/* these options are mutually exclusive if given */

	if (lws_cmdline_option(argc, argv, "--force-portal"))
		force_cpd_fail_portal = 1;

	if (lws_cmdline_option(argc, argv, "--force-no-internet"))
		force_cpd_fail_no_internet = 1;

	if (lws_cmdline_option(argc, argv, "--respmap"))
		test_respmap = 1;

	if (lws_cmdline_option(argc, argv, "--test404"))
		streamtype = "mintest404";

	if (lws_cmdline_option(argc, argv, "--test404red"))
		streamtype = "mintest404red";

	if (lws_cmdline_option(argc, argv, "--test404redref"))
		streamtype = "mintest404redref";

	if ((p = lws_cmdline_option(argc, argv, "--timeout_ms")))
		timeout_ms = (unsigned int)atoi(p);

	info.fd_limit_per_thread = 1 + 6 + 1;
	info.port = CONTEXT_PORT_NO_LISTEN;
#if defined(LWS_SS_USE_SSPC)
	info.protocols = lws_sspc_protocols;
	{
		const char *p;

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
#else
	info.pss_policies_json = default_ss_policy;
	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
		       LWS_SERVER_OPTION_H2_JUST_FIX_WINDOW_UPDATE_OVERFLOW |
		       LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
#endif

	/* integrate us with lws system state management when context created */

	nl.name = "app";
	nl.notify_cb = app_system_state_nf;
	info.register_notifier_list = app_notifier_list;


#if defined(LWS_WITH_SYS_METRICS)
	info.system_ops = &system_ops;
	info.metrics_prefix = "ssmex";
#endif

	/* create the context */

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		goto bail;
	}

#if !defined(LWS_SS_USE_SSPC)
	/*
	 * If we're being a proxied client, the proxy does all this
	 */

	/*
	 * Set the related lws_system blobs
	 *
	 * ...direct_set() sets a pointer, so the thing pointed to has to have
	 * a suitable lifetime, eg, something that already exists on the heap or
	 * a const string in .rodata like this
	 */

	lws_system_blob_direct_set(lws_system_get_blob(context,
				   LWS_SYSBLOB_TYPE_DEVICE_SERIAL, 0),
				   (const uint8_t *)"SN12345678", 10);
	lws_system_blob_direct_set(lws_system_get_blob(context,
				   LWS_SYSBLOB_TYPE_DEVICE_FW_VERSION, 0),
				   (const uint8_t *)"v0.01", 5);

	/*
	 * ..._heap_append() appends to a buflist kind of arrangement on heap,
	 * just one block is fine, otherwise it will concatenate the fragments
	 * in the order they were appended (and take care of freeing them at
	 * context destroy time). ..._heap_empty() is also available to remove
	 * everything that was already allocated.
	 *
	 * Here we use _heap_append() just so it's tested as well as direct set.
	 */

	lws_system_blob_heap_append(lws_system_get_blob(context,
				    LWS_SYSBLOB_TYPE_DEVICE_TYPE, 0),
				   (const uint8_t *)"spacerocket", 11);
#endif

	/* the event loop */

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
