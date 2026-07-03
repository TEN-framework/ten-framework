/*
 * lws-minimal-ws-client-binance
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *                         Kutoga <kutoga@user.github.invalid>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a ws client that connects to binance ws server efficiently
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>

typedef struct range {
	uint64_t		sum;
	uint64_t		lowest;
	uint64_t		highest;

	unsigned int		samples;
} range_t;

/*
 * This represents your object that "contains" the client connection and has
 * the client connection bound to it
 */

static struct my_conn {
	lws_sorted_usec_list_t	sul;	     /* schedule connection retry */
	lws_sorted_usec_list_t	sul_hz;	     /* 1hz summary */

	range_t			e_lat_range;
	range_t			price_range;

	struct lws		*wsi;	     /* related wsi if any */
	uint16_t		retry_count; /* count of consequetive retries */
} mco;

static struct lws_context *context;
static int interrupted;

#if defined(LWS_WITH_MBEDTLS) || defined(USE_WOLFSSL)
/*
 * OpenSSL uses the system trust store.  mbedTLS / WolfSSL have to be told which
 * CA to trust explicitly.
 */
static const char * const ca_pem_digicert_global_root =
	"-----BEGIN CERTIFICATE-----\n"
	"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n"
	"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
	"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
	"QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n"
	"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
	"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n"
	"9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n"
	"CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n"
	"nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n"
	"43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n"
	"T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n"
	"gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n"
	"BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n"
	"TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n"
	"DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n"
	"hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n"
	"06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n"
	"PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n"
	"YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n"
	"CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n"
	"-----END CERTIFICATE-----\n";
#endif

/*
 * The retry and backoff policy we want to use for our client connections
 */

static const uint32_t backoff_ms[] = { 1000, 2000, 3000, 4000, 5000 };

static const lws_retry_bo_t retry = {
	.retry_ms_table			= backoff_ms,
	.retry_ms_table_count		= LWS_ARRAY_SIZE(backoff_ms),
	.conceal_count			= LWS_ARRAY_SIZE(backoff_ms),

	.secs_since_valid_ping		= 400,  /* force PINGs after secs idle */
	.secs_since_valid_hangup	= 400, /* hangup after secs idle */

	.jitter_percent			= 0,
};

/*
 * If we don't enable permessage-deflate ws extension, during times when there
 * are many ws messages per second the server coalesces them inside a smaller
 * number of larger ssl records, for >100 mps typically >2048 records.
 *
 * This is a problem, because the coalesced record cannot be send nor decrypted
 * until the last part of the record is received, meaning additional latency
 * for the earlier members of the coalesced record that have just been sitting
 * there waiting for the last one to go out and be decrypted.
 *
 * permessage-deflate reduces the data size before the tls layer, for >100mps
 * reducing the colesced records to ~1.2KB.
 */

static const struct lws_extension extensions[] = {
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate"
		 "; client_no_context_takeover"
		 "; client_max_window_bits"
	},
	{ NULL, NULL, NULL /* terminator */ }
};
/*
 * Scheduled sul callback that starts the connection attempt
 */

static void
connect_client(lws_sorted_usec_list_t *sul)
{
	struct my_conn *mco = lws_container_of(sul, struct my_conn, sul);
	struct lws_client_connect_info i;

	memset(&i, 0, sizeof(i));

	i.context = context;
	i.port = 443;
	i.address = "fstream.binance.com";
	i.path = "/stream?"
		 "streams=btcusdt@depth@0ms/btcusdt@bookTicker/btcusdt@aggTrade";
	i.host = i.address;
	i.origin = i.address;
	i.ssl_connection = LCCSCF_USE_SSL | LCCSCF_PRIORITIZE_READS;
	i.protocol = NULL;
	i.local_protocol_name = "lws-minimal-client";
	i.pwsi = &mco->wsi;
	i.retry_and_idle_policy = &retry;
	i.userdata = mco;

	if (!lws_client_connect_via_info(&i))
		/*
		 * Failed... schedule a retry... we can't use the _retry_wsi()
		 * convenience wrapper api here because no valid wsi at this
		 * point.
		 */
		if (lws_retry_sul_schedule(context, 0, sul, &retry,
					   connect_client, &mco->retry_count)) {
			lwsl_err("%s: connection attempts exhausted\n", __func__);
			interrupted = 1;
		}
}

static void
range_reset(range_t *r)
{
	r->sum = r->highest = 0;
	r->lowest = 999999999999ull;
	r->samples = 0;
}

static uint64_t
get_us_timeofday(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return (uint64_t)((lws_usec_t)tv.tv_sec * LWS_US_PER_SEC) + (uint64_t)tv.tv_usec;
}

static void
sul_hz_cb(lws_sorted_usec_list_t *sul)
{
	struct my_conn *mco = lws_container_of(sul, struct my_conn, sul_hz);

	/*
	 * We are called once a second to dump statistics on the connection
	 */

	lws_sul_schedule(lws_get_context(mco->wsi), 0, &mco->sul_hz,
			 sul_hz_cb, LWS_US_PER_SEC);

	if (mco->price_range.samples)
		lwsl_notice("%s: price: min: %llu¢, max: %llu¢, avg: %llu¢, "
			    "(%d prices/s)\n",
			    __func__,
			    (unsigned long long)mco->price_range.lowest,
			    (unsigned long long)mco->price_range.highest,
			    (unsigned long long)(mco->price_range.sum / mco->price_range.samples),
			    mco->price_range.samples);
	if (mco->e_lat_range.samples)
		lwsl_notice("%s: elatency: min: %llums, max: %llums, "
			    "avg: %llums, (%d msg/s)\n", __func__,
			    (unsigned long long)mco->e_lat_range.lowest / 1000,
			    (unsigned long long)mco->e_lat_range.highest / 1000,
			    (unsigned long long)(mco->e_lat_range.sum /
					   mco->e_lat_range.samples) / 1000,
			    mco->e_lat_range.samples);

	range_reset(&mco->e_lat_range);
	range_reset(&mco->price_range);
}

static uint64_t
pennies(const char *s)
{
	uint64_t price = (uint64_t)atoll(s) * 100;

	s = strchr(s, '.');

	if (s && isdigit(s[1]) && isdigit(s[2]))
		price = price + (uint64_t)((10 * (s[1] - '0')) + (s[2] - '0'));

	return price;
}

static int
callback_minimal(struct lws *wsi, enum lws_callback_reasons reason,
		 void *user, void *in, size_t len)
{
	struct my_conn *mco = (struct my_conn *)user;
	uint64_t latency_us, now_us;
	uint64_t price;
	char numbuf[16];
	const char *p;
	size_t alen;

	switch (reason) {

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		goto do_retry;
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		/*
		 * The messages are a few 100 bytes of JSON each
		 */

		// lwsl_hexdump_notice(in, len);

		now_us = (uint64_t)get_us_timeofday();

		p = lws_json_simple_find((const char *)in, len,
					 "\"depthUpdate\"", &alen);
		/*
		 * Only the JSON with depthUpdate init has the numbers we care
		 * about as well
		 */
		if (!p)
			break;

		p = lws_json_simple_find((const char *)in, len, "\"E\":", &alen);
		if (!p) {
			lwsl_err("%s: no E JSON\n", __func__);
			break;
		}
		lws_strnncpy(numbuf, p, alen, sizeof(numbuf));
		latency_us = now_us -
				((uint64_t)atoll(numbuf) * LWS_US_PER_MS);

		if (latency_us < mco->e_lat_range.lowest)
			mco->e_lat_range.lowest = latency_us;
		if (latency_us > mco->e_lat_range.highest)
			mco->e_lat_range.highest = latency_us;

		mco->e_lat_range.sum += latency_us;
		mco->e_lat_range.samples++;

		p = lws_json_simple_find((const char *)in, len,
					 "\"a\":[[\"", &alen);
		if (p) {
			lws_strnncpy(numbuf, p, alen, sizeof(numbuf));
			price = pennies(numbuf);

			if (price < mco->price_range.lowest)
				mco->price_range.lowest = price;
			if (price > mco->price_range.highest)
				mco->price_range.highest = price;

			mco->price_range.sum += price;
			mco->price_range.samples++;
		}
		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lwsl_user("%s: established\n", __func__);
		lws_sul_schedule(lws_get_context(wsi), 0, &mco->sul_hz,
				 sul_hz_cb, LWS_US_PER_SEC);
		mco->wsi = wsi;
		range_reset(&mco->e_lat_range);
		range_reset(&mco->price_range);
		break;

	case LWS_CALLBACK_CLIENT_CLOSED:
		lws_sul_cancel(&mco->sul_hz);
		goto do_retry;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);

do_retry:
	/*
	 * retry the connection to keep it nailed up
	 *
	 * For this example, we try to conceal any problem for one set of
	 * backoff retries and then exit the app.
	 *
	 * If you set retry.conceal_count to be larger than the number of
	 * elements in the backoff table, it will never give up and keep
	 * retrying at the last backoff delay plus the random jitter amount.
	 */
	if (lws_retry_sul_schedule_retry_wsi(wsi, &mco->sul, connect_client,
					     &mco->retry_count)) {
		lwsl_err("%s: connection attempts exhausted\n", __func__);
		interrupted = 1;
	}

	return 0;
}

static const struct lws_protocols protocols[] = {
	{ "lws-minimal-client", callback_minimal, 0, 0, 0, NULL, 0 },
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
	int n = 0;

	signal(SIGINT, sigint_handler);
	memset(&info, 0, sizeof info);
	lws_cmdline_option_handle_builtin(argc, argv, &info);

	lwsl_user("LWS minimal binance client\n");

	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = protocols;
	info.fd_limit_per_thread = 1 + 1 + 1;
	info.extensions = extensions;

#if defined(LWS_WITH_MBEDTLS) || defined(USE_WOLFSSL)
	/*
	 * OpenSSL uses the system trust store.  mbedTLS / WolfSSL have to be
	 * told which CA to trust explicitly.
	 */
	info.client_ssl_ca_mem = ca_pem_digicert_global_root;
	info.client_ssl_ca_mem_len = (unsigned int)strlen(ca_pem_digicert_global_root);
#endif

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	/* schedule the first client connection attempt to happen immediately */
	lws_sul_schedule(context, 0, &mco.sul, connect_client, 1);

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);
	lwsl_user("Completed\n");

	return 0;
}

