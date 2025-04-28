/*
 * lws-api-test-lws_cache
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>

static struct lws_context *cx;
static int tests, fail;

static int
test_just_l1(void)
{
	struct lws_cache_creation_info ci;
	struct lws_cache_ttl_lru *l1;
	int ret = 1;
	size_t size;
	char *po;

	lwsl_user("%s\n", __func__);

	tests++;

	/* just create a heap cache "L1" */

	memset(&ci, 0, sizeof(ci));
	ci.cx = cx;
	ci.ops = &lws_cache_ops_heap;
	ci.name = "L1";

	l1 = lws_cache_create(&ci);
	if (!l1)
		goto cdone;

	/* add two items, a has 1s expiry and b has 2s */

	if (lws_cache_write_through(l1, "a", (const uint8_t *)"is_a", 5,
				    lws_now_usecs() + LWS_US_PER_SEC, NULL))
		goto cdone;

	if (lws_cache_write_through(l1, "b", (const uint8_t *)"is_b", 5,
				    lws_now_usecs() + LWS_US_PER_SEC * 2, NULL))
		goto cdone;

	/* check they exist as intended */

	if (lws_cache_item_get(l1, "a", (const void **)&po, &size) ||
	    size != 5 || strcmp(po, "is_a"))
		goto cdone;

	if (lws_cache_item_get(l1, "b", (const void **)&po, &size) ||
	    size != 5 || strcmp(po, "is_b"))
		goto cdone;

	/* wait for 1.2s to pass, working the event loop by hand */

	lws_cancel_service(cx);
	if (lws_service(cx, 0) < 0)
		goto cdone;
#if defined(WIN32)
	Sleep(1200);
#else
	/* netbsd cares about < 1M */
	usleep(999999);
	usleep(200001);
#endif
	lws_cancel_service(cx);
	if (lws_service(cx, 0) < 0)
		goto cdone;

	lws_cancel_service(cx);
	if (lws_service(cx, 0) < 0)
		goto cdone;

	/* a only had 1s lifetime, he should be gone */

	if (!lws_cache_item_get(l1, "a", (const void **)&po, &size)) {
		lwsl_err("%s: cache: a still exists after expiry\n", __func__);
		fail++;
		goto cdone;
	}

	/* that's ok then */

	ret = 0;

cdone:
	lws_cache_destroy(&l1);

	if (ret)
		lwsl_warn("%s: fail\n", __func__);

	return ret;
}

static int
test_just_l1_limits(void)
{
	struct lws_cache_creation_info ci;
	struct lws_cache_ttl_lru *l1;
	int ret = 1;
	size_t size;
	char *po;

	lwsl_user("%s\n", __func__);
	tests++;

	/* just create a heap cache "L1" */

	memset(&ci, 0, sizeof(ci));
	ci.cx = cx;
	ci.ops = &lws_cache_ops_heap;
	ci.name = "L1_lim";
	ci.max_items = 1; /* ie, adding a second item destroys the first */

	l1 = lws_cache_create(&ci);
	if (!l1)
		goto cdone;

	/* add two items, a has 1s expiry and b has 2s */

	if (lws_cache_write_through(l1, "a", (const uint8_t *)"is_a", 5,
				    lws_now_usecs() + LWS_US_PER_SEC, NULL))
		goto cdone;

	if (lws_cache_write_through(l1, "b", (const uint8_t *)"is_b", 5,
				    lws_now_usecs() + LWS_US_PER_SEC * 2, NULL))
		goto cdone;

	/* only b should exit, since we limit to cache to just one entry */

	if (!lws_cache_item_get(l1, "a", (const void **)&po, &size))
		goto cdone;

	if (lws_cache_item_get(l1, "b", (const void **)&po, &size) ||
	    size != 5 || strcmp(po, "is_b"))
		goto cdone;

	/* that's ok then */

	ret = 0;

cdone:
	lws_cache_destroy(&l1);

	if (ret)
		lwsl_warn("%s: fail\n", __func__);

	return ret;
}

#if defined(LWS_WITH_CACHE_NSCOOKIEJAR)

static const char
	*cookie1 = "host.com\tFALSE\t/\tTRUE\t4000000000\tmycookie\tmycookievalue",
	*tag_cookie1 = "host.com|/|mycookie",
	*cookie2 = "host.com\tFALSE\t/xxx\tTRUE\t4000000000\tmycookie\tmyxxxcookievalue",
	*tag_cookie2 = "host.com|/xxx|mycookie",
	*cookie3 = "host.com\tFALSE\t/\tTRUE\t4000000000\textra\tcookie3value",
	*tag_cookie3 = "host.com|/|extra",
	*cookie4 = "host.com\tFALSE\t/yyy\tTRUE\t4000000000\tnewcookie\tnewcookievalue",
	*tag_cookie4 = "host.com|/yyy|newcookie"
;

static int
test_nsc1(void)
{
	struct lws_cache_creation_info ci;
	struct lws_cache_ttl_lru *l1 = NULL, *nsc;
	lws_cache_results_t cr;
	int n, ret = 1;
	size_t size;
	char *po;

	lwsl_user("%s\n", __func__);
	tests++;

	/* First create a netscape cookie cache object */

	memset(&ci, 0, sizeof(ci));
	ci.cx = cx;
	ci.ops = &lws_cache_ops_nscookiejar;
	ci.name = "NSC";
	ci.u.nscookiejar.filepath = "./cookies.txt";

	nsc = lws_cache_create(&ci);
	if (!nsc)
		goto cdone;

	/* Then a heap cache "L1" as a child of nsc */

	ci.ops = &lws_cache_ops_heap;
	ci.name = "L1";
	ci.parent = nsc;

	l1 = lws_cache_create(&ci);
	if (!l1)
		goto cdone;

	lws_cache_debug_dump(nsc);
	lws_cache_debug_dump(l1);

	lwsl_user("%s: add cookies to L1\n", __func__);

	/* add three cookies */

	if (lws_cache_write_through(l1, tag_cookie1,
				    (const uint8_t *)cookie1, strlen(cookie1),
				    lws_now_usecs() + LWS_US_PER_SEC, NULL)) {
		lwsl_err("%s: write1 failed\n", __func__);
		goto cdone;
	}

	lws_cache_debug_dump(nsc);
	lws_cache_debug_dump(l1);

	if (lws_cache_write_through(l1, tag_cookie2,
				    (const uint8_t *)cookie2, strlen(cookie2),
				    lws_now_usecs() + LWS_US_PER_SEC * 2, NULL)) {
		lwsl_err("%s: write2 failed\n", __func__);
		goto cdone;
	}

	lws_cache_debug_dump(nsc);
	lws_cache_debug_dump(l1);

	if (lws_cache_write_through(l1, tag_cookie3,
				    (const uint8_t *)cookie3, strlen(cookie3),
				    lws_now_usecs() + LWS_US_PER_SEC * 2, NULL)) {
		lwsl_err("%s: write3 failed\n", __func__);
		goto cdone;
	}

	lws_cache_debug_dump(nsc);
	lws_cache_debug_dump(l1);

	lwsl_user("%s: check cookies in L1\n", __func__);

	/* confirm that the cookies are individually in L1 */

	if (lws_cache_item_get(l1, tag_cookie1, (const void **)&po, &size) ||
	    size != strlen(cookie1) || memcmp(po, cookie1, size)) {
		lwsl_err("%s: L1 '%s' missing, size %llu, po %s\n", __func__,
			 tag_cookie1, (unsigned long long)size, po);
		goto cdone;
	}

	if (lws_cache_item_get(l1, tag_cookie2, (const void **)&po, &size) ||
	    size != strlen(cookie2) || memcmp(po, cookie2, size)) {
		lwsl_err("%s: L1 '%s' missing\n", __func__, tag_cookie2);
		goto cdone;
	}

	if (lws_cache_item_get(l1, tag_cookie3, (const void **)&po, &size) ||
	    size != strlen(cookie3) || memcmp(po, cookie3, size)) {
		lwsl_err("%s: L1 '%s' missing\n", __func__, tag_cookie3);
		goto cdone;
	}

	/* confirm that the cookies are individually in L2 / NSC... normally
	 * we don't do this but check via L1 so we can get it from there if
	 * present.  But as a unit test, we want to make sure it's in L2 / NSC
	 */

	lwsl_user("%s: check cookies written thru to NSC\n", __func__);

	if (lws_cache_item_get(nsc, tag_cookie1, (const void **)&po, &size) ||
	    size != strlen(cookie1) || memcmp(po, cookie1, size)) {
		lwsl_err("%s: NSC '%s' missing, size %llu, po %s\n", __func__,
			 tag_cookie1, (unsigned long long)size, po);
		goto cdone;
	}

	if (lws_cache_item_get(nsc, tag_cookie2, (const void **)&po, &size) ||
	    size != strlen(cookie2) || memcmp(po, cookie2, size)) {
		lwsl_err("%s: NSC '%s' missing\n", __func__, tag_cookie2);
		goto cdone;
	}

	if (lws_cache_item_get(nsc, tag_cookie3, (const void **)&po, &size) ||
	    size != strlen(cookie3) || memcmp(po, cookie3, size)) {
		lwsl_err("%s: NSC '%s' missing\n", __func__, tag_cookie3);
		goto cdone;
	}

	/* let's do a lookup with no results */

	lwsl_user("%s: nonexistant get must not pass\n", __func__);

	if (!lws_cache_item_get(l1, "x.com|y|z", (const void **)&po, &size)) {
		lwsl_err("%s: nonexistant found size %llu, po %s\n", __func__,
			 (unsigned long long)size, po);
		goto cdone;
	}

	/*
	 * let's try some url paths and check we get the right results set...
	 * for / and any cookie, we expect only c1 and c3 to be listed
	 */

	lwsl_user("%s: wildcard lookup 1\n", __func__);

	n = lws_cache_lookup(l1, "host.com|/|*",
			     (const void **)&cr.ptr, &cr.size);
	if (n) {
		lwsl_err("%s: lookup failed %d\n", __func__, n);
		goto cdone;
	}
	lwsl_hexdump_notice(cr.ptr, size);

	if (cr.size != 53)
		goto cdone;

	while (!lws_cache_results_walk(&cr))
		lwsl_notice("  %s (%d)\n", (const char *)cr.tag,
					   (int)cr.payload_len);

	/*
	 * for /xxx and any cookie, we expect all 3 listed
	 */

	lwsl_user("%s: wildcard lookup 2\n", __func__);

	n = lws_cache_lookup(l1, "host.com|/xxx|*",
			     (const void **)&cr.ptr, &cr.size);
	if (n) {
		lwsl_err("%s: lookup failed %d\n", __func__, n);
		goto cdone;
	}

	if (cr.size != 84)
		goto cdone;

	while (!lws_cache_results_walk(&cr))
		lwsl_notice("  %s (%d)\n", (const char *)cr.tag,
					   (int)cr.payload_len);

	/*
	 * for /yyyy and any cookie, we expect only c1 and c3
	 */

	lwsl_user("%s: wildcard lookup 3\n", __func__);

	n = lws_cache_lookup(l1, "host.com|/yyyy|*",
			     (const void **)&cr.ptr, &cr.size);
	if (n) {
		lwsl_err("%s: lookup failed %d\n", __func__, n);
		goto cdone;
	}

	if (cr.size != 53)
		goto cdone;

	while (!lws_cache_results_walk(&cr))
		lwsl_notice("  %s (%d)\n", (const char *)cr.tag,
					   (int)cr.payload_len);

	/*
	 * repeat the above test, results should come from cache
	 */

	lwsl_user("%s: wildcard lookup 4\n", __func__);

	n = lws_cache_lookup(l1, "host.com|/yyyy|*",
			     (const void **)&cr.ptr, &cr.size);
	if (n) {
		lwsl_err("%s: lookup failed %d\n", __func__, n);
		goto cdone;
	}

	if (cr.size != 53)
		goto cdone;

	while (!lws_cache_results_walk(&cr))
		lwsl_notice("  %s (%d)\n", (const char *)cr.tag,
					   (int)cr.payload_len);

	/* now let's try deleting cookie 1 */

	if (lws_cache_item_remove(l1, tag_cookie1))
		goto cdone;

	lws_cache_debug_dump(nsc);
	lws_cache_debug_dump(l1);

	/* with c1 gone, we should only get c3 */

	lwsl_user("%s: wildcard lookup 5\n", __func__);

	n = lws_cache_lookup(l1, "host.com|/|*",
			     (const void **)&cr.ptr, &cr.size);
	if (n) {
		lwsl_err("%s: lookup failed %d\n", __func__, n);
		goto cdone;
	}

	if (cr.size != 25)
		goto cdone;

	while (!lws_cache_results_walk(&cr))
		lwsl_notice("  %s (%d)\n", (const char *)cr.tag,
					   (int)cr.payload_len);

	/*
	 * let's add a fourth cookie (third in cache now we deleted one)
	 */

	if (lws_cache_write_through(l1, tag_cookie4,
				    (const uint8_t *)cookie4, strlen(cookie4),
				    lws_now_usecs() + LWS_US_PER_SEC * 2, NULL)) {
		lwsl_err("%s: write4 failed\n", __func__);
		goto cdone;
	}

	/*
	 * for /yy and any cookie, we expect only c3
	 */

	lwsl_user("%s: wildcard lookup 6\n", __func__);

	n = lws_cache_lookup(l1, "host.com|/yy|*",
			     (const void **)&cr.ptr, &cr.size);
	if (n) {
		lwsl_err("%s: lookup failed %d\n", __func__, n);
		goto cdone;
	}

	if (cr.size != 25)
		goto cdone;

	while (!lws_cache_results_walk(&cr))
		lwsl_notice("  %s (%d)\n", (const char *)cr.tag,
					   (int)cr.payload_len);

	/*
	 * for /yyy and any cookie, we expect  c3 and c4
	 */

	lwsl_user("%s: wildcard lookup 7\n", __func__);

	n = lws_cache_lookup(l1, "host.com|/yyy|*",
			     (const void **)&cr.ptr, &cr.size);
	if (n) {
		lwsl_err("%s: lookup failed %d\n", __func__, n);
		goto cdone;
	}

	if (cr.size != 57)
		goto cdone;

	while (!lws_cache_results_walk(&cr))
		lwsl_notice("  %s (%d)\n", (const char *)cr.tag,
					   (int)cr.payload_len);

	/* that's ok then */

	lwsl_user("%s: done\n", __func__);

	ret = 0;

cdone:
	lws_cache_destroy(&nsc);
	lws_cache_destroy(&l1);

	if (ret)
		lwsl_warn("%s: fail\n", __func__);

	return ret;
}
#endif


int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;

	memset(&info, 0, sizeof info);
	lws_cmdline_option_handle_builtin(argc, argv, &info);
	info.fd_limit_per_thread = 1 + 6 + 1;
	info.port = CONTEXT_PORT_NO_LISTEN;

	lwsl_user("LWS API selftest: lws_cache\n");

	cx = lws_create_context(&info);
	if (!cx) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	if (test_just_l1())
		fail++;
	if (test_just_l1_limits())
		fail++;

#if defined(LWS_WITH_CACHE_NSCOOKIEJAR)
	if (test_nsc1())
		fail++;
#endif

	lws_context_destroy(cx);

	if (tests && !fail)
		lwsl_user("Completed: PASS\n");
	else
		lwsl_err("Completed: FAIL %d / %d\n", fail, tests);

	return 0;
}
