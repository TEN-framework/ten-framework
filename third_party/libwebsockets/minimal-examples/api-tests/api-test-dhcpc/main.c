/*
 * lws-api-test-dhcpc
 *
 * Written in 2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>
#include <signal.h>

static int interrupted, ok, fail, exp = 1;
struct lws_context *context;
const char *nif;

static const char * const sa46_names[] = {
	"LWSDH_SA46_IP",
	"LWSDH_SA46_DNS_SRV_1",
	"LWSDH_SA46_DNS_SRV_2",
	"LWSDH_SA46_DNS_SRV_3",
	"LWSDH_SA46_DNS_SRV_4",
	"LWSDH_SA46_IPV4_ROUTER",
	"LWSDH_SA46_NTP_SERVER",
	"LWSDH_SA46_DHCP_SERVER",
};

static int
lws_dhcpc_cb(void *opaque, lws_dhcpc_ifstate_t *is)
{
	unsigned int n;
	char buf[64];

	lwsl_user("%s: dhcp set OK\n", __func__);

	for (n = 0; n < LWS_ARRAY_SIZE(sa46_names); n++) {
		lws_sa46_write_numeric_address(&is->sa46[n], buf, sizeof(buf));
		lwsl_notice("%s: %s: %s\n", __func__, sa46_names[n], buf);
	}

	ok = 1;
	interrupted = 1;
	return 0;
}

void sigint_handler(int sig)
{
	interrupted = 1;
}

int
main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
#if !defined(__COVERITY__)
	const char *p;
#endif
	int n = 1;

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	lws_cmdline_option_handle_builtin(argc, argv, &info);
	lwsl_user("LWS API selftest: DHCP Client\n");

	info.port = CONTEXT_PORT_NO_LISTEN;
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

#if !defined(__COVERITY__)
	if ((p = lws_cmdline_option(argc, argv, "-i")))
		nif = p;
#endif

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	if (nif) {
		lwsl_user("%s: requesting DHCP for %s\n", __func__, nif);
		lws_dhcpc_request(context, nif, AF_INET, lws_dhcpc_cb, NULL);
	} else {
		lwsl_err("%s: use -i <network-interface> to select if\n", __func__);
		interrupted = 1;
	}

	/* the usual lws event loop */

	n = 1;
	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);

	if (fail || ok != exp)
		lwsl_user("Completed: PASS: %d / %d, FAIL: %d\n", ok, exp,
				fail);
	else
		lwsl_user("Completed: ALL PASS: %d / %d\n", ok, exp);

	return !(ok == exp && !fail);
}
