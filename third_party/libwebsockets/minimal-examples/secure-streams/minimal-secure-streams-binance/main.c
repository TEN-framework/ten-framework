/*
 * lws-minimal-secure-streams-binance
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *                         Kutoga <kutoga@user.github.invalid>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a Secure Streams implementation of a client that connects
 * to binance ws server efficiently.
 *
 * Build lws with -DLWS_WITH_SECURE_STREAMS=1 -DLWS_WITHOUT_EXTENSIONS=0
 *
 * "policy.json" contains all the information about endpoints, protocols and
 * connection validation, tagged by streamtype name.
 *
 * The example tries to load it from the cwd, it lives
 * in ./minimal-examples/secure-streams/minimal-secure-streams-binance dir, so
 * either run it from there, or copy the policy.json to your cwd.  It's also
 * possible to put the policy json in the code as a string and pass that at
 * context creation time.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>

static int interrupted;

typedef struct range {
	uint64_t		sum;
	uint64_t		lowest;
	uint64_t		highest;

	unsigned int		samples;
} range_t;

typedef struct binance {
	struct lws_ss_handle 	*ss;
	void			*opaque_data;

	lws_sorted_usec_list_t	sul_hz;	     /* 1hz summary dump */

	range_t			e_lat_range;
	range_t			price_range;
} binance_t;

/****** Part 1 / 3: application data processing */

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

	return (uint64_t)((lws_usec_t)tv.tv_sec * LWS_US_PER_SEC) +
			  (uint64_t)tv.tv_usec;
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

static void
sul_hz_cb(lws_sorted_usec_list_t *sul)
{
	binance_t *bin = lws_container_of(sul, binance_t, sul_hz);

	/*
	 * We are called once a second to dump statistics on the connection
	 */

	lws_sul_schedule(lws_ss_get_context(bin->ss), 0, &bin->sul_hz,
			 sul_hz_cb, LWS_US_PER_SEC);

	if (bin->price_range.samples)
		lwsl_notice("%s: price: min: %llu¢, max: %llu¢, avg: %llu¢, "
			    "(%d prices/s)\n", __func__,
			    (unsigned long long)bin->price_range.lowest,
			    (unsigned long long)bin->price_range.highest,
			    (unsigned long long)(bin->price_range.sum /
						    bin->price_range.samples),
			    bin->price_range.samples);
	if (bin->e_lat_range.samples)
		lwsl_notice("%s: elatency: min: %llums, max: %llums, "
			    "avg: %llums, (%d msg/s)\n", __func__,
			    (unsigned long long)bin->e_lat_range.lowest / 1000,
			    (unsigned long long)bin->e_lat_range.highest / 1000,
			    (unsigned long long)(bin->e_lat_range.sum /
					   bin->e_lat_range.samples) / 1000,
			    bin->e_lat_range.samples);

	range_reset(&bin->e_lat_range);
	range_reset(&bin->price_range);
}

/****** Part 2 / 3: communication */

static lws_ss_state_return_t
binance_rx(void *userobj, const uint8_t *in, size_t len, int flags)
{
	binance_t *bin = (binance_t *)userobj;
	uint64_t latency_us, now_us;
	char numbuf[16];
	uint64_t price;
	const char *p;
	size_t alen;

	now_us = (uint64_t)get_us_timeofday();

	p = lws_json_simple_find((const char *)in, len, "\"depthUpdate\"",
				 &alen);
	if (!p)
		return LWSSSSRET_OK;

	p = lws_json_simple_find((const char *)in, len, "\"E\":", &alen);
	if (!p) {
		lwsl_err("%s: no E JSON\n", __func__);
		return LWSSSSRET_OK;
	}

	lws_strnncpy(numbuf, p, alen, sizeof(numbuf));
	latency_us = now_us - ((uint64_t)atoll(numbuf) * LWS_US_PER_MS);

	if (latency_us < bin->e_lat_range.lowest)
		bin->e_lat_range.lowest = latency_us;
	if (latency_us > bin->e_lat_range.highest)
		bin->e_lat_range.highest = latency_us;

	bin->e_lat_range.sum += latency_us;
	bin->e_lat_range.samples++;

	p = lws_json_simple_find((const char *)in, len, "\"a\":[[\"", &alen);
	if (!p)
		return LWSSSSRET_OK;

	lws_strnncpy(numbuf, p, alen, sizeof(numbuf));
	price = pennies(numbuf);

	if (price < bin->price_range.lowest)
		bin->price_range.lowest = price;
	if (price > bin->price_range.highest)
		bin->price_range.highest = price;

	bin->price_range.sum += price;
	bin->price_range.samples++;

	return LWSSSSRET_OK;
}

static lws_ss_state_return_t
binance_state(void *userobj, void *h_src, lws_ss_constate_t state,
	      lws_ss_tx_ordinal_t ack)
{
	binance_t *bin = (binance_t *)userobj;

	lwsl_ss_info(bin->ss, "%s (%d), ord 0x%x",
		     lws_ss_state_name((int)state), state, (unsigned int)ack);

	switch (state) {

	case LWSSSCS_CONNECTED:
		lws_sul_schedule(lws_ss_get_context(bin->ss), 0, &bin->sul_hz,
				 sul_hz_cb, LWS_US_PER_SEC);
		range_reset(&bin->e_lat_range);
		range_reset(&bin->price_range);

		return LWSSSSRET_OK;

	case LWSSSCS_DISCONNECTED:
		lws_sul_cancel(&bin->sul_hz);
		break;

	default:
		break;
	}

	return LWSSSSRET_OK;
}

static const lws_ss_info_t ssi_binance = {
	.handle_offset		  = offsetof(binance_t, ss),
	.opaque_user_data_offset  = offsetof(binance_t, opaque_data),
	.rx			  = binance_rx,
	.state			  = binance_state,
	.user_alloc		  = sizeof(binance_t),
	.streamtype		  = "binance", /* bind to corresponding policy */
};

/****** Part 3 / 3: init and event loop */

static const struct lws_extension extensions[] = {
	{
		"permessage-deflate", lws_extension_callback_pm_deflate,
		"permessage-deflate" "; client_no_context_takeover"
		 "; client_max_window_bits"
	},
	{ NULL, NULL, NULL /* terminator */ }
};

static void
sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *cx;
	int n = 0;

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info);
	lws_cmdline_option_handle_builtin(argc, argv, &info);

	lwsl_user("LWS minimal Secure Streams binance client\n");

	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
		       LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.fd_limit_per_thread = 1 + 1 + 1;
	info.extensions = extensions;
	info.pss_policies_json = "policy.json"; /* literal JSON, or path */

	cx = lws_create_context(&info);
	if (!cx) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	if (lws_ss_create(cx, 0, &ssi_binance, NULL, NULL, NULL, NULL)) {
		lwsl_cx_err(cx, "failed to create secure stream");
		interrupted = 1;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(cx, 0);

	lws_context_destroy(cx);

	lwsl_user("Completed\n");

	return 0;
}
