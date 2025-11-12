/*
 * lws-api-test-secure-streams
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * Let's exercise some basic SS / h1 functionality against httpbin.org
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

static int interrupted, bad = 1;
static lws_state_notify_link_t nl;
static struct lws_context *context;

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

		"{\"amz_root_ca1\": \""
	"MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF"
	"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6"
	"b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL"
	"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv"
	"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj"
	"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM"
	"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw"
	"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6"
	"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L"
	"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm"
	"jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC"
	"AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA"
	"A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI"
	"U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs"
	"N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv"
	"o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU"
	"5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy"
	"rqXRfboQnoZsG4q5WTP468SQvvG5"
		"\"}"
	  "],"
	  "\"trust_stores\": [" /* named cert chains */
		"{"
			"\"name\": \"amz\","
			"\"stack\": ["
				"\"amz_root_ca1\""
			"]"
		"}"
	  "],"
	  "\"s\": ["
	  	/*
		 * "fetch_policy" decides from where the real policy
		 * will be fetched, if present.  Otherwise the initial
		 * policy is treated as the whole, hardcoded, policy.
		 */
		"{\"httpbin_get\": {"
			"\"endpoint\":"		"\"httpbin.org\","
			"\"port\":"		"443,"
			"\"protocol\":"		"\"h1\","
			"\"http_method\":"	"\"GET\","
			"\"http_url\":"		"\"/get\","
			"\"tls\":"		"true,"
			"\"opportunistic\":"	"true,"
			"\"retry\":"		"\"default\","
			"\"tls_trust_store\":"	"\"amz\""
		"}},"
		"{\"httpbin_get404\": {"
			"\"endpoint\":"		"\"httpbin.org\","
			"\"port\":"		"443,"
			"\"protocol\":"		"\"h1\","
			"\"http_method\":"	"\"GET\","
			"\"http_url\":"		"\"/status/404\","
			"\"tls\":"		"true,"
			"\"opportunistic\":"	"true,"
			"\"retry\":"		"\"default\","
			"\"tls_trust_store\":"	"\"amz\""
		"}},"
		"{\"httpbin_post\": {"
			"\"endpoint\":"		"\"httpbin.org\","
			"\"port\":"		"443,"
			"\"protocol\":"		"\"h1\","
			"\"http_method\":"	"\"POST\","
			"\"http_url\":"		"\"/post\","
			"\"tls\":"		"true,"
			"\"opportunistic\":"	"true,"
			"\"retry\":"		"\"default\","
			"\"tls_trust_store\":"	"\"amz\""
			"}}"
                "}"
	"]}"
;

typedef struct atss {
	const lws_ss_info_t		*ssi;
	size_t				send;
	char				expect_nack;
} atss_t;

static const atss_t *next_test;

typedef struct myss {
	struct lws_ss_handle 		*ss;
	void				*opaque_data;
	/* ... application specific state ... */
	lws_sorted_usec_list_t		sul;
	size_t				payload;
	size_t				sent;
	char				seen_eom;
	char				ended_well;
} myss_t;

/* secure streams payload interface */

static lws_ss_state_return_t
myss_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	myss_t *m = (myss_t *)userobj;

	lwsl_hexdump_info(buf, len);

	m->payload += len;

	if (!(flags & LWSSS_FLAG_EOM))
		m->seen_eom = 1;

	return 0;
}

static lws_ss_state_return_t
myss_tx_get(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	    int *flags)
{
	return 1; /* nothing to send */
}

static lws_ss_state_return_t
myss_tx_post(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	     int *flags)
{
	myss_t *m = (myss_t *)userobj;
	size_t budget = (next_test->send - m->sent);

	if (!budget)
		return 1;

	if (*len < budget)
		budget = *len;

	if (!m->sent)
		*flags |= LWSSS_FLAG_SOM;

	memset(buf, 0x55, budget);
	*len = budget;
	m->sent += budget;
	if (m->sent != next_test->send)
		return lws_ss_request_tx(m->ss);

	*flags |= LWSSS_FLAG_EOM;

	return LWSSSSRET_OK;
}

static lws_ss_state_return_t
myss_state(void *userobj, void *sh, lws_ss_constate_t state,
	   lws_ss_tx_ordinal_t ack)
{
	myss_t *m = (myss_t *)userobj;
	lws_ss_state_return_t r;

	lwsl_notice("%s: %s, ord 0x%x\n", __func__, lws_ss_state_name((int)state),
		  (unsigned int)ack);

	switch (state) {
	case LWSSSCS_CREATING:
		r = lws_ss_client_connect(m->ss);
		if (r)
			return r;
		if (next_test->send)
			return lws_ss_request_tx_len(m->ss, (unsigned long)next_test->send);
		break;
	case LWSSSCS_ALL_RETRIES_FAILED:
		lwsl_notice("%s: Connection failed\n", __func__);
		interrupted = 1;
		break;
	case LWSSSCS_QOS_NACK_REMOTE:
		if (next_test->expect_nack)
			goto happy;
		lwsl_notice("%s: remote NACK\n", __func__);
		interrupted = 1;
		break;
	case LWSSSCS_QOS_ACK_REMOTE:
		/*
		 * To be satisfied, we want to see the ACK_REMOTE indicating
		 * that the transaction went through; that we had the payload
		 * EOM; and that we saw at least 200 + posted bytes response
		 */

		if (!m->seen_eom || m->payload < 200 + next_test->send) {
			lwsl_warn("%s: ACK_REMOTE but eom %d, payload %d\n",
				  __func__, m->seen_eom, (int)m->payload);
			interrupted = 1;
			return -1;
		}

happy:
		/* when we disconnect, we can go happily */
		m->ended_well = 1;

		if (!(++next_test)->ssi) {
			lwsl_notice("%s: completed all tests\n", __func__);
			bad = 0;
			interrupted = 1;
			break;
		}
		if (lws_ss_create(context, 0, next_test->ssi,
				  NULL, NULL, NULL, NULL)) {
			lwsl_err("%s: failed to create secure stream\n",
				 __func__);
			return -1;
		}
		break;

	case LWSSSCS_DISCONNECTED:
		if (!m->ended_well) {
			lwsl_warn("%s: DISCONNECTED without good end\n",
				  __func__);
			interrupted = 1;
		}
		break;
	default:
		break;
	}

	return LWSSSSRET_OK;
}

static const lws_ss_info_t ssi_get = {
	.handle_offset			= offsetof(myss_t, ss),
	.opaque_user_data_offset	= offsetof(myss_t, opaque_data),
	.rx				= myss_rx,
	.tx				= myss_tx_get,
	.state				= myss_state,
	.user_alloc			= sizeof(myss_t),
	.streamtype			= "httpbin_get"
}, ssi_get404 = {
	.handle_offset			= offsetof(myss_t, ss),
	.opaque_user_data_offset	= offsetof(myss_t, opaque_data),
	.rx				= myss_rx,
	.tx				= myss_tx_get,
	.state				= myss_state,
	.user_alloc			= sizeof(myss_t),
	.streamtype			= "httpbin_get404"
}, ssi_post = {
	.handle_offset			= offsetof(myss_t, ss),
	.opaque_user_data_offset	= offsetof(myss_t, opaque_data),
	.rx				= myss_rx,
	.tx				= myss_tx_post,
	.state				= myss_state,
	.user_alloc			= sizeof(myss_t),
	.streamtype			= "httpbin_post"
};

static const atss_t test_list[] = {
		{ .ssi = &ssi_get },
		{ .ssi = &ssi_get404, .expect_nack = 1 },
		{ .ssi = &ssi_post, .send = 4096 },
		{ .ssi = NULL }
};


static int
app_system_state_nf(lws_state_manager_t *mgr, lws_state_notify_link_t *link,
		    int current, int target)
{
	struct lws_context *context = lws_system_context_from_system_mgr(mgr);

	/*
	 * For the things we care about, let's notice if we are trying to get
	 * past them when we haven't solved them yet, and make the system
	 * state wait while we trigger the dependent action.
	 */
	switch (target) {

	case LWS_SYSTATE_OPERATIONAL:
		if (current == LWS_SYSTATE_OPERATIONAL) {

			next_test = &test_list[0];

			if (lws_ss_create(context, 0, next_test->ssi,
					  NULL, NULL, NULL, NULL)) {
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

	memset(&info, 0, sizeof info);
	lws_cmdline_option_handle_builtin(argc, argv, &info);

	lwsl_user("LWS secure streams test client [-d<verb>]\n");

	/* these options are mutually exclusive if given */

	info.fd_limit_per_thread = 1 + 6 + 1;
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.pss_policies_json = default_ss_policy;
	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
		       LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
		       LWS_SERVER_OPTION_H2_JUST_FIX_WINDOW_UPDATE_OVERFLOW;

	/* integrate us with lws system state management when context created */

	nl.name = "app";
	nl.notify_cb = app_system_state_nf;
	info.register_notifier_list = app_notifier_list;

	/* create the context */

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	/* the event loop */

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);

	lwsl_user("Completed: %s\n", bad ? "failed" : "OK");

	return bad;
}
