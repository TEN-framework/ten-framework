/*
 * lws-api-test-async-dns
 *
 * Written in 2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This api test confirms various kinds of async dns apis
 */

#include <libwebsockets.h>
#include <signal.h>

static int interrupted, dtest, ok, fail, _exp = 26;
struct lws_context *context;

/*
 * These are used to test the apis to parse and print ipv4 / ipv6 literal
 * address strings for various cases.
 *
 * Expected error cases are not used to test the ip data -> string api.
 */

static const struct ipparser_tests {
	const char	*test;
	int		rlen;
	const char	*emit_test;
	int		emit_len;
	uint8_t		b[16];
} ipt[] = {
	{ "2001:db8:85a3:0:0:8a2e:370:7334", 16,
	  "2001:db8:85a3::8a2e:370:7334", 28,
		{ 0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3, 0x00, 0x00,
		  0x00, 0x00, 0x8a, 0x2e, 0x03, 0x70, 0x73, 0x34 } },

	{ "2001:db8:85a3::8a2e:370:7334", 16,
	  "2001:db8:85a3::8a2e:370:7334", 28,
		{ 0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3, 0x00, 0x00,
		  0x00, 0x00, 0x8a, 0x2e, 0x03, 0x70, 0x73, 0x34 } },

	{ "::1", 16, "::1", 3,
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },

	{ "::",  16, "::", 2,
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },

	{ "::ffff:192.0.2.128", 16,  "::ffff:192.0.2.128", 18,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0xff, 0xff, 0xc0, 0x00, 0x02, 0x80 } },

	{ "cats", -1, "", 0,
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },

	{ "onevalid.bogus.warmcat.com", -1, "", 0,
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },

	{ "1.cat.dog.com", -1, "", 0,
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },

	{ ":::1", -8, "", 0,
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },

	{ "0:0::0:1", 16, "::1", 3,
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },

	{ "1.2.3.4", 4, "1.2.3.4", 7, { 1, 2, 3, 4 } },
};

static const struct async_dns_tests {
	const char *dns_name;
	int recordtype;
	int addrlen;
	uint8_t ads[16];
} adt[] = {
	{ "warmcat.com", LWS_ADNS_RECORD_A, 4,
		{ 46, 105, 127, 147, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, } },
	{ "libwebsockets.org", LWS_ADNS_RECORD_A, 4,
		{ 46, 105, 127, 147, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, } },
	{ "doesntexist", LWS_ADNS_RECORD_A, 0,
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, } },
	{ "localhost", LWS_ADNS_RECORD_A, 4,
		{ 127, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, } },
	{ "ipv4only.warmcat.com", LWS_ADNS_RECORD_A, 4,
		{ 46, 105, 127, 147, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, } },
	{ "onevalid.bogus.warmcat.com", LWS_ADNS_RECORD_A, 4,
		{ 46, 105, 127, 147, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, } },
#if defined(LWS_WITH_IPV6)
	{ "warmcat.com", LWS_ADNS_RECORD_AAAA, 16, /* check ipv6 */
		{ 0x20, 0x01, 0x41, 0xd0, 0x00, 0x02, 0xee, 0x93,
				0, 0, 0, 0, 0, 0, 0, 1, } },
	{ "ipv6only.warmcat.com", LWS_ADNS_RECORD_AAAA, 16, /* check ipv6 */
		{ 0x20, 0x01, 0x41, 0xd0, 0x00, 0x02, 0xee, 0x93,
				0, 0, 0, 0, 0, 0, 0, 1, } },
#endif
};

static lws_sorted_usec_list_t sul;

struct lws *
cb1(struct lws *wsi_unused, const char *ads, const struct addrinfo *a, int n,
    void *opaque);

static void
next_test_cb(lws_sorted_usec_list_t *sul)
{
	int m;

	lwsl_notice("%s: querying %s\n", __func__, adt[dtest].dns_name);

	m = lws_async_dns_query(context, 0,
				adt[dtest].dns_name,
				(adns_query_type_t)adt[dtest].recordtype, cb1, NULL,
				context);
	if (m != LADNS_RET_CONTINUING && m != LADNS_RET_FOUND && m != LADNS_RET_FAILED_WSI_CLOSED) {
		lwsl_err("%s: adns 1: %s failed: %d\n", __func__, adt[dtest].dns_name, m);
		interrupted = 1;
	}
}

struct lws *
cb1(struct lws *wsi_unused, const char *ads, const struct addrinfo *a, int n,
    void *opaque)
{
	const struct addrinfo *ac = a;
	int ctr = 0, alen;
	uint8_t *addr;
	char buf[64];

	dtest++;

	if (!ac)
		lwsl_warn("%s: no results\n", __func__);

	/* dump the results */

	while (ac) {
		if (ac->ai_family == AF_INET) {
			addr = (uint8_t *)&(((struct sockaddr_in *)
					ac->ai_addr)->sin_addr.s_addr);
			alen = 4;
		} else {
			addr = (uint8_t *)&(((struct sockaddr_in6 *)
					ac->ai_addr)->sin6_addr.s6_addr);
			alen = 16;
		}
		strcpy(buf, "unknown");
		lws_write_numeric_address(addr, alen, buf, sizeof(buf));

		lwsl_warn("%s: %d: %s %d %s\n", __func__, ctr++, ads, alen, buf);

		ac = ac->ai_next;
	}

	ac = a;
	while (ac) {
		if (ac->ai_family == AF_INET) {
			addr = (uint8_t *)&(((struct sockaddr_in *)
					ac->ai_addr)->sin_addr.s_addr);
			alen = 4;
		} else {
#if defined(LWS_WITH_IPV6)
			addr = (uint8_t *)&(((struct sockaddr_in6 *)
					ac->ai_addr)->sin6_addr.s6_addr);
			alen = 16;
#else
			goto again;
#endif
		}
		if (alen == adt[dtest - 1].addrlen &&
		    !memcmp(adt[dtest - 1].ads, addr, (unsigned int)alen)) {
			ok++;
			goto next;
		}
#if !defined(LWS_WITH_IPV6)
again:
#endif
		ac = ac->ai_next;
	}

	/* testing for NXDOMAIN? */

	if (!a && !adt[dtest - 1].addrlen) {
		ok++;
		goto next;
	}

	lwsl_err("%s: dns test %d: no match\n", __func__, dtest);
	fail++;

next:
	lws_async_dns_freeaddrinfo(&a);
	if (dtest == (int)LWS_ARRAY_SIZE(adt))
		interrupted = 1;
	else
		lws_sul_schedule(context, 0, &sul, next_test_cb, 1);

	return NULL;
}

static lws_sorted_usec_list_t sul_l;

struct lws *
cb_loop(struct lws *wsi_unused, const char *ads, const struct addrinfo *a, int n,
		void *opaque)
{
	if (!a) {
		lwsl_err("%s: no results\n", __func__);
		return NULL;
	}

	lwsl_notice("%s: addrinfo %p\n", __func__, a);\
	lws_async_dns_freeaddrinfo(&a);

	return NULL;
}


static void
sul_retry_l(struct lws_sorted_usec_list *sul)
{
	int m;

	lwsl_user("%s: starting new query\n", __func__);

	m = lws_async_dns_query(context, 0, "warmcat.com",
				    (adns_query_type_t)LWS_ADNS_RECORD_A,
				    cb_loop, NULL, context);
	switch (m) {
	case LADNS_RET_FAILED_WSI_CLOSED:
		lwsl_warn("%s: LADNS_RET_FAILED_WSI_CLOSED "
			  "(== from cache / success in this test)\n", __func__);
		break;
	case LADNS_RET_NXDOMAIN:
		lwsl_warn("%s: LADNS_RET_NXDOMAIN\n", __func__);
		break;
	case LADNS_RET_TIMEDOUT:
		lwsl_warn("%s: LADNS_RET_TIMEDOUT\n", __func__);
		break;
	case LADNS_RET_FAILED:
		lwsl_warn("%s: LADNS_RET_FAILED\n", __func__);
		break;
	case LADNS_RET_FOUND:
		lwsl_warn("%s: LADNS_RET_FOUND\n", __func__);
		break;
	case LADNS_RET_CONTINUING:
		lwsl_warn("%s: LADNS_RET_CONTINUING\n", __func__);
		break;
	}

	lws_sul_schedule(context, 0, &sul_l, sul_retry_l, 5 * LWS_US_PER_SEC);
}

void sigint_handler(int sig)
{
	interrupted = 1;
}

int
main(int argc, const char **argv)
{
	int n = 1, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	struct lws_context_creation_info info;
	const char *p;

	/* the normal lws init */

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS API selftest: Async DNS\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	if (lws_cmdline_option(argc, argv, "-l")) {
		lws_sul_schedule(context, 0, &sul_l, sul_retry_l, LWS_US_PER_SEC);
		goto evloop;
	}


	/* ip address parser tests */

	for (n = 0; n < (int)LWS_ARRAY_SIZE(ipt); n++) {
		uint8_t u[16];
		int m = lws_parse_numeric_address(ipt[n].test, u, sizeof(u));

		if (m != ipt[n].rlen) {
			lwsl_err("%s: fail %s ret %d\n",
					__func__, ipt[n].test, m);
			fail++;
			continue;
		}

		if (m > 0) {
			if (memcmp(ipt[n].b, u, (unsigned int)m)) {
				lwsl_err("%s: fail %s compare\n", __func__,
						ipt[n].test);
				lwsl_hexdump_notice(u, (unsigned int)m);
				fail++;
				continue;
			}
		}
		ok++;
	}

	/* ip address formatter tests */

	for (n = 0; n < (int)LWS_ARRAY_SIZE(ipt); n++) {
		char buf[64];
		int m;

		/* don't attempt to reverse the ones that are meant to fail */
		if (ipt[n].rlen < 0)
			continue;

		m = lws_write_numeric_address(ipt[n].b, ipt[n].rlen, buf,
						sizeof(buf));
		if (m != ipt[n].emit_len) {
			lwsl_err("%s: fail %s ret %d\n",
					__func__, ipt[n].emit_test, m);
			fail++;
			continue;
		}

		if (m > 0) {
			if (strcmp(ipt[n].emit_test, buf)) {
				lwsl_err("%s: fail %s compare\n", __func__,
						ipt[n].test);
				lwsl_hexdump_notice(buf, (unsigned int)m);
				fail++;
				continue;
			}
		}
		ok++;
	}

#if !defined(LWS_WITH_IPV6)
	_exp -= 2;
#endif

	/* kick off the async dns tests */

	lws_sul_schedule(context, 0, &sul, next_test_cb, 1);

evloop:
	/* the usual lws event loop */

	n = 1;
	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);

	if (fail || ok != _exp)
		lwsl_user("Completed: PASS: %d / %d, FAIL: %d\n", ok, _exp,
				fail);
	else
		lwsl_user("Completed: ALL PASS: %d / %d\n", ok, _exp);

	return !(ok == _exp && !fail);
}
