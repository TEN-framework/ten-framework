/*
 * lws-minimal-secure-streams-seq
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 *
 * This demonstrates the a minimal http client using secure streams api.
 *
 * It visits https://warmcat.com/ and receives the html page there.
 *
 * This is the "secure streams" api equivalent of minimal-http-client...
 * it shows how to use a sequencer to make it easy to build more complex
 * schemes on top of this example.
 *
 * The layering looks like this
 *
 *                        lifetime
 *
 * ------   app   ------  process
 * ----  sequencer  ----  process
 * --- secure stream ---  process
 * -------  wsi  -------  connection
 *
 * see minimal-secure-streams for a similar example without the sequencer.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

static int interrupted, bad = 1, flag_conn_fail, flag_h1post;
static const char * const default_ss_policy =
	"{"
	  "\"release\":"			"\"01234567\","
	  "\"product\":"			"\"myproduct\","
	  "\"schema-version\":"			"1,"
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
			"\"svalidping\":"	"300,"
			"\"svalidhup\":"	"310"
		"}}"
	  "],"
	  "\"certs\": [" /* named individual certificates in BASE64 DER */
		/*
		 * Need to be in order from root cert... notice sometimes as
		 * with Let's Encrypt there are multiple possible validation
		 * paths, all the pieces for one validation path must be
		 * given, excluding the server cert itself.  Let's Encrypt
		 * intermediate is signed by their ISRG Root CA but also is
		 * cross-signed by an IdenTrust intermediate that's widely
		 * deployed in browsers.  We use the ISRG path because that
		 * way we can skip the extra IdenTrust root cert.
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
	  "\"s\": [" /* the supported stream types */
		"{\"mintest\": {"
			"\"endpoint\":"		"\"warmcat.com\","
			"\"port\":"		"443,"
			"\"protocol\":"		"\"h1\","
			"\"http_method\":"	"\"GET\","
			"\"http_url\":"		"\"index.html\","
			"\"plugins\":"		"[],"
			"\"tls\":"		"true,"
			"\"opportunistic\":"	"true,"
			"\"retry\":"		"\"default\","
			"\"tls_trust_store\":"	"\"le_via_isrg\""
		"}},"
		"{\"mintest-fail\": {"
			"\"endpoint\":"		"\"warmcat.com\","
			"\"port\":"		"22,"
			"\"protocol\":"		"\"h1\","
			"\"http_method\":"	"\"GET\","
			"\"http_url\":"		"\"index.html\","
			"\"plugins\":"		"[],"
			"\"tls\":"		"true,"
			"\"opportunistic\":"	"true,"
			"\"retry\":"		"\"default\","
			"\"tls_trust_store\":"	"\"le_via_isrg\""
		"}},"
		"{\"minpost\": {"
			"\"endpoint\":"		"\"warmcat.com\","
			"\"port\":"		"443,"
			"\"protocol\":"		"\"h1\","
			"\"http_method\":"	"\"POST\","
			"\"http_url\":"		"\"testserver/formtest\","
			"\"plugins\":"		"[],"
			"\"tls\":"		"true,"
			"\"opportunistic\":"	"true,"
			"\"retry\":"		"\"default\","
			"\"tls_trust_store\":"	"\"le_via_isrg\""
		"}}"
	  "]"
	"}"
;

typedef struct myss {
	struct lws_ss_handle 	*ss;
	void			*opaque_data;
	/* ... application specific state ... */
} myss_t;

/* secure streams payload interface */

static lws_ss_state_return_t
myss_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
//	myss_t *m = (myss_t *)userobj;

	lwsl_user("%s: len %d, flags: %d\n", __func__, (int)len, flags);
	lwsl_hexdump_info(buf, len);

	/*
	 * If we received the whole message, we let the sequencer know it
	 * was a success
	 */
	if (flags & LWSSS_FLAG_EOM) {
		bad = 0;
		interrupted = 1;
	}

	return 0;
}

static lws_ss_state_return_t
myss_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	int *flags)
{
	// myss_t *m = (myss_t *)userobj;

	/* in this example, we don't send any payload */

	return 0;
}

static lws_ss_state_return_t
myss_state(void *userobj, void *sh, lws_ss_constate_t state,
		lws_ss_tx_ordinal_t ack)
{
	myss_t *m = (myss_t *)userobj;

	lwsl_user("%s: %s, ord 0x%x\n", __func__, lws_ss_state_name(state),
		  (unsigned int)ack);

	switch (state) {
	case LWSSSCS_CREATING:
		return lws_ss_request_tx(m->ss);

	case LWSSSCS_ALL_RETRIES_FAILED:
		/* if we're out of retries, we want to close the app and FAIL */
		interrupted = 1;
		break;
	default:
		break;
	}

	return 0;
}

typedef enum {
	SEQ_IDLE,
	SEQ_TRY_CONNECT,
	SEQ_RECONNECT_WAIT,
	SEQ_CONNECTED,
} myseq_state_t;

typedef struct myseq {
	struct lws_ss_handle	*ss;

	myseq_state_t		state;
	int			http_resp;

	uint16_t		try;
} myseq_t;

/*
 * This defines the sequence of things the test app does.
 */

static lws_seq_cb_return_t
min_sec_str_sequencer_cb(struct lws_sequencer *seq, void *user, int event,
			 void *v, void *a)
{
	struct myseq *s = (struct myseq *)user;
	lws_ss_info_t ssi;

	switch ((int)event) {

	/* these messages are created just by virtue of being a sequencer */

	case LWSSEQ_CREATED: /* our sequencer just got started */
		s->state = SEQ_IDLE;
		lwsl_notice("%s: LWSSEQ_CREATED\n", __func__);

		/* We're making an outgoing secure stream ourselves */

		memset(&ssi, 0, sizeof(ssi));
		ssi.handle_offset = offsetof(myss_t, ss);
		ssi.opaque_user_data_offset = offsetof(myss_t, opaque_data);
		ssi.rx = myss_rx;
		ssi.tx = myss_tx;
		ssi.state = myss_state;
		ssi.user_alloc = sizeof(myss_t);

		/* requested to fail (to check backoff)? */
		if (flag_conn_fail)
			ssi.streamtype = "mintest-fail";
		else
			/* request to check h1 POST */
			if (flag_h1post)
				ssi.streamtype = "minpost";
			else
				/* default to h1 GET */
				ssi.streamtype = "mintest";

		if (lws_ss_create(lws_seq_get_context(seq), 0, &ssi, NULL,
				  &s->ss, seq, NULL)) {
			lwsl_err("%s: failed to create secure stream\n",
				 __func__);

			return LWSSEQ_RET_DESTROY;
		}
		break;

	case LWSSEQ_DESTROYED:
		lwsl_notice("%s: LWSSEQ_DESTROYED\n", __func__);
		break;

	case LWSSEQ_TIMED_OUT: /* current step timed out */
		if (s->state == SEQ_RECONNECT_WAIT)
			return lws_ss_request_tx(s->ss);
		break;

	/*
	 * These messages are created because we have a secure stream that was
	 * bound to this sequencer at creation time.  It copies its state
	 * events to us as its sequencer parent.  v is the lws_ss_handle_t *
	 */

	case LWSSEQ_SS_STATE_BASE + LWSSSCS_CREATING:
		lwsl_info("%s: seq LWSSSCS_CREATING\n", __func__);
		return lws_ss_request_tx(s->ss);

	case LWSSEQ_SS_STATE_BASE + LWSSSCS_DISCONNECTED:
		lwsl_info("%s: seq LWSSSCS_DISCONNECTED\n", __func__);
		break;
	case LWSSEQ_SS_STATE_BASE + LWSSSCS_UNREACHABLE:
		lwsl_info("%s: seq LWSSSCS_UNREACHABLE\n", __func__);
		break;
	case LWSSEQ_SS_STATE_BASE + LWSSSCS_AUTH_FAILED:
		lwsl_info("%s: seq LWSSSCS_AUTH_FAILED\n", __func__);
		break;
	case LWSSEQ_SS_STATE_BASE + LWSSSCS_CONNECTED:
		lwsl_info("%s: seq LWSSSCS_CONNECTED\n", __func__);
		break;
	case LWSSEQ_SS_STATE_BASE + LWSSSCS_CONNECTING:
		lwsl_info("%s: seq LWSSSCS_CONNECTING\n", __func__);
		break;
	case LWSSEQ_SS_STATE_BASE + LWSSSCS_DESTROYING:
		lwsl_info("%s: seq LWSSSCS_DESTROYING\n", __func__);
		break;
	case LWSSEQ_SS_STATE_BASE + LWSSSCS_POLL:
		/* somebody called lws_ss_poll() on the stream */
		lwsl_info("%s: seq LWSSSCS_POLL\n", __func__);
		break;
	case LWSSEQ_SS_STATE_BASE + LWSSSCS_ALL_RETRIES_FAILED:
		lwsl_info("%s: seq LWSSSCS_ALL_RETRIES_FAILED\n", __func__);
		interrupted = 1;
		break;
	case LWSSEQ_SS_STATE_BASE + LWSSSCS_QOS_ACK_REMOTE:
		lwsl_info("%s: seq LWSSSCS_QOS_ACK_REMOTE\n", __func__);
		break;
	case LWSSEQ_SS_STATE_BASE + LWSSSCS_QOS_ACK_LOCAL:
		lwsl_info("%s: seq LWSSSCS_QOS_ACK_LOCAL\n", __func__);
		break;

	/*
	 * This is the message we send from the ss handler to inform the
	 * sequencer we had the payload properly
	 */

	case LWSSEQ_USER_BASE:
		bad = 0;
		interrupted = 1;
		break;

	default:
		break;
	}

	return LWSSEQ_RET_CONTINUE;
}

static void
sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	struct lws_context_creation_info info;
	struct lws_context *context;
	lws_seq_info_t i;
	const char *p;
	myseq_t *ms;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal secure streams [-d<verbosity>][-f][--h1post]\n");

	flag_conn_fail = !!lws_cmdline_option(argc, argv, "-f");
	flag_h1post = !!lws_cmdline_option(argc, argv, "--h1post");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */

	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
		       LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.fd_limit_per_thread = 1 + 1 + 1;
	info.pss_policies_json = default_ss_policy;
	info.port = CONTEXT_PORT_NO_LISTEN;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	/*
	 * Create the sequencer that performs the steps of the test action
	 * from inside the event loop.
	 */

	memset(&i, 0, sizeof(i));
	i.context	= context;
	i.user_size	= sizeof(myseq_t);
	i.puser		= (void **)&ms;
	i.cb		= min_sec_str_sequencer_cb;
	i.name		= "min-sec-stream-seq";

	if (!lws_seq_create(&i)) {
		lwsl_err("%s: failed to create sequencer\n", __func__);
		goto bail;
	}

	/* the event loop */

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

bail:
	lws_context_destroy(context);
	lwsl_user("Completed: %s\n", bad ? "failed" : "OK");

	return bad;
}
