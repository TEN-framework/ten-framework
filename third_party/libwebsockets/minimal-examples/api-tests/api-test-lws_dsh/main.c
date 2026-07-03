/*
 * lws-api-test-lws_dsh
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>

int
test1(void)
{
	struct lws_dsh *dsh;
	size_t size;
	void *a1;

	/*
	 * test 1: single dsh, alloc 2 kinds and free everything back to a
	 *         single free obj
	 */

	dsh = lws_dsh_create(NULL, 16384, 2);
	if (!dsh) {
		lwsl_err("%s: Failed to create dsh\n", __func__);

		return 1;
	}

	if (lws_dsh_alloc_tail(dsh, 0, "hello", 5, NULL, 0)) {
		lwsl_err("%s: Failed to alloc 1\n", __func__);

		goto bail;
	}

	if (lws_dsh_alloc_tail(dsh, 1, "some other string", 17, NULL, 0)) {
		lwsl_err("%s: Failed to alloc 2\n", __func__);

		goto bail;
	}

	if (lws_dsh_alloc_tail(dsh, 0, "hello again", 11, NULL, 0)) {
		lwsl_err("%s: Failed to alloc 3\n", __func__);

		goto bail;
	}

	if (lws_dsh_get_head(dsh, 1, &a1, &size)) {
		lwsl_err("%s: no head 1\n", __func__);

		goto bail;
	}
	if (size != 17 || memcmp(a1, "some other string", 17)) {
		lwsl_err("%s: test 1 mismatch\n", __func__);

		goto bail;
	}
	lws_dsh_free(&a1);

	if (lws_dsh_get_head(dsh, 0, &a1, &size)) {
		lwsl_err("%s: no head 2\n", __func__);

		goto bail;
	}
	if (size != 5 || memcmp(a1, "hello", 5)) {
		lwsl_err("%s: test 2 mismatch\n", __func__);

		goto bail;
	}
	lws_dsh_free(&a1);

	if (lws_dsh_get_head(dsh, 0, &a1, &size)) {
		lwsl_err("%s: no head 3\n", __func__);

		goto bail;
	}
	if (size != 11 || memcmp(a1, "hello again", 11)) {
		lwsl_err("%s: test 3 mismatch\n", __func__);

		goto bail;
	}
	lws_dsh_free(&a1);

	lws_dsh_destroy(&dsh);

	return 0;
bail:
	lws_dsh_destroy(&dsh);

	return 1;
}

int
test3(void)
{
	struct lws_dsh *dsh, *dsh2;
	lws_dll2_owner_t owner;
	uint8_t blob[4096];

	memset(blob, 0, sizeof(blob));

	/*
	 * test 3: multiple dsh, umeetable allocation request
	 */

	lws_dll2_owner_clear(&owner);

	dsh = lws_dsh_create(&owner, 4096, 2);
	if (!dsh) {
		lwsl_err("%s: Failed to create dsh1\n", __func__);

		return 1;
	}

	dsh2 = lws_dsh_create(&owner, 4096, 2);
	if (!dsh2) {
		lwsl_err("%s: Failed to create dsh2\n", __func__);

		goto bail;
	}

	if (lws_dsh_alloc_tail(dsh, 0, blob, 4000, NULL, 0)) {
		lwsl_err("%s: Failed to alloc 1\n", __func__);

		goto bail2;
	}

	if (lws_dsh_alloc_tail(dsh2, 0, "hello", 5, NULL, 0)) {
		lwsl_err("%s: Failed to alloc 2\n", __func__);

		goto bail2;
	}

	/*
	 * There's just no room for this, we expect it to fail
	 */

	if (!lws_dsh_alloc_tail(dsh, 0, blob, 5000, NULL, 0)) {
		lwsl_err("%s: Didn't fail to alloc as expected\n", __func__);

		goto bail2;
	}

	if (lws_dsh_alloc_tail(dsh2, 0, "hello again", 11, NULL, 0)) {
		lwsl_err("%s: Failed to alloc 4\n", __func__);

		goto bail2;
	}

	lws_dsh_destroy(&dsh2);
	lws_dsh_destroy(&dsh);

	return 0;

bail2:
	lws_dsh_destroy(&dsh2);

bail:
	lws_dsh_destroy(&dsh);

	return 1;
}

int
test4(void)
{
	uint8_t blob[4096];
	struct lws_dsh *dsh;
	size_t size;
	void *a1;

	memset(blob, 0, sizeof(blob));

	/*
	 * test 4: use up whole free list, then recover and alloc something
	 *	   else
	 */

	dsh = lws_dsh_create(NULL, 4096, 2);
	if (!dsh) {
		lwsl_err("%s: Failed to create dsh\n", __func__);

		return 1;
	}

	if (lws_dsh_alloc_tail(dsh, 0, blob, 4000, NULL, 0)) {
		lwsl_err("%s: Failed to alloc 1\n", __func__);

		goto bail;
	}

	if (lws_dsh_get_head(dsh, 0, &a1, &size)) {
		lwsl_err("%s: no head 1\n", __func__);

		goto bail;
	}
	if (size != 4000) {
		lwsl_err("%s: test 1 mismatch\n", __func__);

		goto bail;
	}
	lws_dsh_free(&a1);

	if (lws_dsh_alloc_tail(dsh, 0, "some other string", 17, NULL, 0)) {
		lwsl_err("%s: Failed to alloc 2\n", __func__);

		goto bail;
	}

	if (lws_dsh_alloc_tail(dsh, 0, "hello again", 11, NULL, 0)) {
		lwsl_err("%s: Failed to alloc 3\n", __func__);

		goto bail;
	}

	if (lws_dsh_get_head(dsh, 0, &a1, &size)) {
		lwsl_err("%s: no head 1\n", __func__);

		goto bail;
	}
	if (size != 17 || memcmp(a1, "some other string", 17)) {
		lwsl_err("%s: test 1 mismatch\n", __func__);

		goto bail;
	}
	lws_dsh_free(&a1);

	if (lws_dsh_get_head(dsh, 0, &a1, &size)) {
		lwsl_err("%s: no head 2\n", __func__);

		goto bail;
	}
	if (size != 11 || memcmp(a1, "hello again", 11)) {
		lwsl_err("%s: test 2 mismatch (%zu)\n", __func__, size);

		goto bail;
	}

	lws_dsh_free(&a1);

	lws_dsh_destroy(&dsh);

	return 0;
bail:
	lws_dsh_destroy(&dsh);

	return 1;
}

int
test5(void)
{
	struct lws_dsh *dsh;
	unsigned int budget;
	uint8_t blob[4096];
	lws_xos_t xos;
	size_t size;
	void *a1;

	memset(blob, 0, sizeof(blob));
	lws_xos_init(&xos, 0x123456789abcdef0ull);

	budget = (unsigned int)(lws_xos(&xos) % 4000) + 4000;

	lwsl_notice("%s: budget %u\n", __func__, budget);


	/*
	 * test 5: PRNG-based spamming and erratic bidi draining
	 */

	dsh = lws_dsh_create(NULL, 409600, 2);
	if (!dsh) {
		lwsl_err("%s: Failed to create dsh\n", __func__);

		return 1;
	}

	do {

		if (lws_xos_percent(&xos, 60)) {
			/* kind 0 is going to try to write */

			size = (size_t)((lws_xos(&xos) & 127) + 1);

			if (!lws_dsh_alloc_tail(dsh, 0, blob, size, NULL, 0))
				lwsl_notice("%s: kind 0 alloc %d\n", __func__, (int)size);
		}

		if (lws_xos_percent(&xos, 80)) {
			/* kind 1 is going to try to write */

			size = (size_t)((lws_xos(&xos) & 127) + 1);

			if (!lws_dsh_alloc_tail(dsh, 1, blob, size, NULL, 0))
				lwsl_notice("%s: kind 1 alloc %d\n", __func__, (int)size);
		}

		if (lws_xos_percent(&xos, 40)) {
			/* kind 0 is going to try to read */

			while (!lws_dsh_get_head(dsh, 0, &a1, &size)) {
				lwsl_notice("%s: kind 0 read %d\n", __func__, (int)size);
				lws_dsh_free(&a1);
			}
		}

		if (lws_xos_percent(&xos, 30)) {
			/* kind 1 is going to try to read */

			while (!lws_dsh_get_head(dsh, 1, &a1, &size)) {
				lwsl_notice("%s: kind 1 read %d\n", __func__, (int)size);
				lws_dsh_free(&a1);
			}
		}

	} while (budget--);

	while (!lws_dsh_get_head(dsh, 0, &a1, &size)) {
		lwsl_notice("%s: kind 0 read %d\n", __func__, (int)size);
		lws_dsh_free(&a1);
	}

	while (!lws_dsh_get_head(dsh, 1, &a1, &size)) {
		lwsl_notice("%s: kind 1 read %d\n", __func__, (int)size);
		lws_dsh_free(&a1);
	}

#if defined(_DEBUG)
	lws_dsh_describe(dsh, "test dsh end state");
#endif

	lws_dsh_destroy(&dsh);

	return 0;
}

int main(int argc, const char **argv)
{
	int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	int ret = 0, n;
	const char *p;

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS API selftest: lws_dsh\n");

	n = test1();
	lwsl_user("%s: test1: %d\n", __func__, n);
	ret |= n;

	n = test3();
	lwsl_user("%s: test3: %d\n", __func__, n);
	ret |= n;

	n = test4();
	lwsl_user("%s: test4: %d\n", __func__, n);
	ret |= n;

	n = test5();
	lwsl_user("%s: test5: %d\n", __func__, n);
	ret |= n;

	lwsl_user("Completed: %s\n", ret ? "FAIL" : "PASS");

	return ret;
}
