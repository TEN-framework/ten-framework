/*
 * lws-minimal-secure-streams-hugeurl
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 *
 * This checks huge url operations via httpbin.org
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

static unsigned int timeout_ms = 3000;
static int interrupted, bad = 1, h1;
static lws_state_notify_link_t nl;
static size_t hugeurl_size = 4000;
static char *hugeurl, *check;

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
		"{\"amazon_root_ca_1\": \""
		  "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0"
		  "BAQsFADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQ"
		  "QDExBBbWF6b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExN"
		  "zAwMDAwMFowOTELMAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcG"
		  "A1UEAxMQQW1hem9uIFJvb3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggE"
		  "PADCCAQoCggEBALJ4gHHKeNXjca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrA"
		  "IthtOgQ3pOsqTQNroBvo3bSMgHFzZM9O6II8c+6zf1tRn4SWiw3te5djgdY"
		  "Z6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qwIFAGbHrQgLKm+a/sRxmPUDgH"
		  "3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6VOujw5H5SNz/0egwLX0"
		  "tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L93FcXmn/6pUCyz"
		  "iKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQmjgSubJrIq"
		  "g0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYw"
		  "HQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwU"
		  "AA4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9r"
		  "bxenDIU5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/m"
		  "sv0tadQ1wUsN+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96L"
		  "XFvKWlJbYK8U90vvo/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bld"
		  "ZwgJcJmApzyMZFo6IQ6XU5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8o"
		  "b2xJNDd2ZhwLnoQdeXeGADbkpyrqXRfboQnoZsG4q5WTP468SQvvG5"
		"\"}"
	  "],"
	  "\"trust_stores\": [" /* named cert chains */
		"{"
			"\"name\": \"arca1\","
			"\"stack\": ["
				"\"amazon_root_ca_1\""
			"]"
		"}"
	  "],"
	  "\"s\": [{"

		"\"httpbin_anything_h1\": {"
			"\"endpoint\":"			"\"httpbin.org\","
			"\"port\":"			"443,"
			"\"protocol\":"			"\"h1\","
			"\"http_method\":"		"\"GET\","
			"\"http_url\":"			"\"anything?x=${hugearg}\","
			"\"nghttp2_quirk_end_stream\":" "true,"
			"\"h2q_oflow_txcr\":"		"true,"
			"\"metadata\": [{"
				"\"hugearg\":"		"\"\""
			"}],"
			"\"tls\":"			"true,"
			"\"opportunistic\":"		"true,"
			"\"retry\":"			"\"default\","
			"\"tls_trust_store\":"		"\"arca1\""
		"}},{"
			"\"httpbin_anything_h2\": {"
			"\"endpoint\":"			"\"httpbin.org\","
			"\"port\":"			"443,"
			"\"protocol\":"			"\"h2\","
			"\"http_method\":"		"\"GET\","
			"\"http_url\":"			"\"anything?x=${hugearg}\","
			"\"nghttp2_quirk_end_stream\":" "true,"
			"\"h2q_oflow_txcr\":"		"true,"
			"\"metadata\": [{"
				"\"hugearg\":"		"\"\""
			"}],"
			"\"tls\":"			"true,"
			"\"opportunistic\":"		"true,"
			"\"retry\":"			"\"default\","
			"\"tls_trust_store\":"		"\"arca1\""
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
	struct lejp_ctx			ctx;
	size_t				comp;

	char				started;
} myss_t;


static const char * const lejp_tokens[] = {
	"url"
};

/*
 * Parse the "url" member of the JSON, and collect the part after the first '='
 * into the prepared buffer "check".
 */

static signed char
lws_httpbin_json_cb(struct lejp_ctx *ctx, char reason)
{
	myss_t *m = (myss_t *)ctx->user;
	const char *p = ctx->buf;
	size_t l = ctx->npos;

	if (!(reason & LEJP_FLAG_CB_IS_VALUE))
		return 0;

	if (ctx->path_match - 1)
		return 0;

	if (!m->started)
		while (l--)
			if (*p++ == '=') {
				m->started = 1;
				break;
			}

	if (!m->started)
		return 0;

	if (m->comp + l > hugeurl_size) {
		lwsl_err("%s: returned url string too large %u, %u\n",
			 __func__, (unsigned int)m->comp, (unsigned int)l);

		return -1;
	}

	memcpy(check + m->comp, p, l);
	m->comp += l;

	return 0;
}

/* secure streams payload interface */

static lws_ss_state_return_t
myss_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	myss_t *m = (myss_t *)userobj;

	if (flags & LWSSS_FLAG_SOM)
		lejp_construct(&m->ctx, lws_httpbin_json_cb, m,
				lejp_tokens, LWS_ARRAY_SIZE(lejp_tokens));

	if (len) {
		int pr = lejp_parse(&m->ctx, buf, (int)len);

		if (pr != LEJP_CONTINUE && pr < 0) {
			lwsl_err("%s: parse failed line %u: %d: %s\n", __func__,
				 (unsigned int)m->ctx.line, pr,
				 lejp_error_to_string(pr));

			return LWSSSSRET_DESTROY_ME;
		}
	}

	if (flags & LWSSS_FLAG_EOM) {

		interrupted = 1;

		/* confirm that what we collected is the expected size */

		if (m->comp != hugeurl_size) {
			lwsl_err("%s: wrong urlarg size recovered %d %d\n",
				 __func__, (int)m->comp, (int)hugeurl_size);
			return LWSSSSRET_OK;
		}

		/* confirm what we sent is the same as what we collected */

		if (memcmp(hugeurl, check, hugeurl_size)) {
			lwsl_err("%s: huge url content mismatch\n", __func__);

			return LWSSSSRET_OK;
		}

		lwsl_user("%s: return hugeurl len %u matches OK\n", __func__,
				(unsigned int)hugeurl_size);

		bad = 0;
	}

	return LWSSSSRET_OK;
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
		lws_ss_start_timeout(m->ss, timeout_ms);

		/* let's make the hugeurl part */

		hugeurl = malloc(hugeurl_size + 1);
		if (!hugeurl) {
			lwsl_err("OOM\n");
			return LWSSSSRET_DESTROY_ME;
		}

		check = malloc(hugeurl_size + 1);
		if (!check) {
			lwsl_err("OOM\n");
			free(hugeurl);
			hugeurl = NULL;
			return LWSSSSRET_DESTROY_ME;
		}

		/* Create the big, random, urlarg */

		lws_hex_random(lws_ss_get_context(m->ss), hugeurl,
			       hugeurl_size + 1);
		if (lws_ss_set_metadata(m->ss, "hugearg", hugeurl, hugeurl_size))
			return LWSSSSRET_DISCONNECT_ME;

		return lws_ss_client_connect(m->ss);

	case LWSSSCS_ALL_RETRIES_FAILED:
		/* if we're out of retries, we want to close the app and FAIL */
		interrupted = 1;
		break;
	case LWSSSCS_QOS_ACK_REMOTE:
		lwsl_notice("%s: LWSSSCS_QOS_ACK_REMOTE\n", __func__);
		break;

	case LWSSSCS_TIMEOUT:
		lwsl_notice("%s: LWSSSCS_TIMEOUT\n", __func__);
		break;

	case LWSSSCS_USER_BASE:
		lwsl_notice("%s: LWSSSCS_USER_BASE\n", __func__);
		break;

	default:
		break;
	}

	return LWSSSSRET_OK;
}

static lws_ss_info_t ssi = {
	.handle_offset			= offsetof(myss_t, ss),
	.opaque_user_data_offset	= offsetof(myss_t, opaque_data),
	.rx				= myss_rx,
	.state				= myss_state,
	.user_alloc			= sizeof(myss_t),
	.streamtype			= "httpbin_anything_h2"
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
	if (target != LWS_SYSTATE_OPERATIONAL)
		return 0;

	if (current != LWS_SYSTATE_OPERATIONAL)
		return 0;

	if (h1)
		ssi.streamtype = "httpbin_anything_h1";

	if (!lws_ss_create(context, 0, &ssi, NULL, NULL, NULL, NULL))
		return 0;

	lwsl_err("%s: failed to create secure stream\n", __func__);

	return -1;
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
	struct lws_context *context;
	const char *p;
	int n = 0;

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info);
	lws_cmdline_option_handle_builtin(argc, argv, &info);

	lwsl_user("LWS secure streams hugeurl test client [-d<verb>][-h <urlarg len>]\n");

	info.fd_limit_per_thread = 1 + 6 + 1;
	info.port = CONTEXT_PORT_NO_LISTEN;
#if defined(LWS_SS_USE_SSPC)
	info.protocols = lws_sspc_protocols;

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
#else
	info.pss_policies_json = default_ss_policy;
	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
		       LWS_SERVER_OPTION_H2_JUST_FIX_WINDOW_UPDATE_OVERFLOW |
		       LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
#endif

	if (lws_cmdline_option(argc, argv, "--h1"))
		h1 = 1;

	if ((p = lws_cmdline_option(argc, argv, "-h")))
		hugeurl_size = (size_t)atol(p);

	if (hugeurl_size < 1 || hugeurl_size > 16384) {
		lwsl_err("%s: -h should be between 1 and 16384\n", __func__);
		return 1;
	}

	lwsl_user("%s: huge argument size: %u bytes\n", __func__,
			(unsigned int)hugeurl_size);

	info.pt_serv_buf_size = (unsigned int)((hugeurl_size * 2) + 2048);
	info.max_http_header_data = (unsigned short)(hugeurl_size + 2048);

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

	if (hugeurl)
		free(hugeurl);
	if (check)
		free(check);

	lwsl_user("Completed: %s\n", bad ? "failed" : "OK");

	return bad;
}
