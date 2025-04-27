/*
 * lws-api-test-lws_map
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * unit tests for lws_map
 */

#include <libwebsockets.h>

typedef struct lws_map_item lws_map_item_t;

/* custom key and comparator for test 3 */

typedef struct mykey {
	int			key;
} mykey_t;

static int
compare_mykey_t(const lws_map_key_t key1, size_t kl1,
		const lws_map_value_t key2, size_t kl2)
{
	const mykey_t *m1 = (mykey_t *)key1, *m2 = (mykey_t *)key2;

	return m1->key != m2->key;
}

int main(int argc, const char **argv)
{
	int e = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE,
			expected = 4, pass = 0;
	mykey_t k1 = { .key = 123 }, k2 = { .key = 234 }, k3 = { .key = 999 };
	struct lwsac *ac = NULL;
	lws_map_item_t *item;
	lws_map_info_t info;
	lws_map_t *map;
	const char *p;

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS API selftest: lws_map\n");

	/* Test 1: string keys */

	lwsl_user("%s: test1\n", __func__);
	memset(&info, 0, sizeof(info));
	map = lws_map_create(&info);
	if (!map) {
		e++;
		goto end_t1;
	}
	if (!lws_map_item_create_ks(map, "abc", (lws_map_value_t)"def", 3)) {
		e++;
		goto end_t1;
	}
	if (!lws_map_item_create_ks(map, "123", (lws_map_value_t)"4567", 4)) {
		e++;
		goto end_t1;
	}
	item = lws_map_item_lookup_ks(map, "abc");
	if (!item) {
		e++;
		goto end_t1;
	}

	if (lws_map_item_value_len(item) != 3 ||
	    memcmp(lws_map_item_value(item), "def", 3)) {
		e++;
		goto end_t1;
	}

	item = lws_map_item_lookup_ks(map, "123");
	if (!item) {
		e++;
		goto end_t1;
	}

	if (lws_map_item_value_len(item) != 4 ||
	    memcmp(lws_map_item_value(item), "4567", 4)) {
		e++;
		goto end_t1;
	}

	item = lws_map_item_lookup_ks(map, "nope");
	if (item) {
		e++;
		goto end_t1;
	}

	pass++;

end_t1:
	lws_map_destroy(&map);

	/* Test 2: Use lwsac item allocators */

	lwsl_user("%s: test2\n", __func__);
	memset(&info, 0, sizeof(info));
	info._alloc = lws_map_alloc_lwsac;
	info._free = lws_map_free_lwsac;
	info.opaque = (void *)&ac;

	map = lws_map_create(&info);
	if (!map) {
		e++;
		goto end_t2;
	}
	if (!lws_map_item_create_ks(map, "abc", "def", 3)) {
		e++;
		goto end_t2;
	}
	if (!lws_map_item_create_ks(map, "123", "4567", 4)) {
		e++;
		goto end_t2;
	}
	item = lws_map_item_lookup_ks(map, "abc");
	if (!item) {
		e++;
		goto end_t2;
	}

	if (lws_map_item_value_len(item) != 3 ||
	    memcmp(lws_map_item_value(item), "def", 3)) {
		e++;
		goto end_t2;
	}

	item = lws_map_item_lookup_ks(map, "123");
	if (!item) {
		e++;
		goto end_t2;
	}

	if (lws_map_item_value_len(item) != 4 ||
	    memcmp(lws_map_item_value(item), "4567", 4)) {
		e++;
		goto end_t2;
	}

	item = lws_map_item_lookup_ks(map, "nope");
	if (item) {
		e++;
		goto end_t2;
	}

	pass++;

end_t2:
	lws_map_destroy(&map);
	lwsac_free(&ac);

	/* Test 3: custom key object and comparator */

	lwsl_user("%s: test3\n", __func__);
	memset(&info, 0, sizeof(info));
	info._compare = compare_mykey_t;

	map = lws_map_create(&info);
	if (!map) {
		e++;
		goto end_t3;
	}
	if (!lws_map_item_create(map, (lws_map_key_t)&k1, sizeof(k1),
				      (lws_map_value_t)"def", 3)) {
		lwsl_err("%s: t3; a\n", __func__);
		e++;
		goto end_t3;
	}
	if (!lws_map_item_create(map, (lws_map_key_t)&k2, sizeof(k2),
				      (lws_map_value_t)"4567", 4)) {
		lwsl_err("%s: t3; b\n", __func__);
		e++;
		goto end_t3;
	}
	item = lws_map_item_lookup(map, (lws_map_key_t)&k1, sizeof(k1));
	if (!item) {
		lwsl_err("%s: t3; c\n", __func__);
		e++;
		goto end_t3;
	}

	if (lws_map_item_value_len(item) != 3 ||
	    memcmp(lws_map_item_value(item), "def", 3)) {
		lwsl_err("%s: t3; d\n", __func__);
		e++;
		goto end_t3;
	}

	item = lws_map_item_lookup(map, (lws_map_key_t)&k2, sizeof(k2));
	if (!item) {
		lwsl_err("%s: t3; e\n", __func__);
		e++;
		goto end_t3;
	}

	if (lws_map_item_value_len(item) != 4 ||
	    memcmp(lws_map_item_value(item), "4567", 4)) {
		lwsl_err("%s: t3; f\n", __func__);
		e++;
		goto end_t3;
	}

	item = lws_map_item_lookup(map, (lws_map_key_t)&k3, sizeof(k3));
	if (item) {
		lwsl_err("%s: t3; g\n", __func__);
		e++;
		goto end_t3;
	}

	pass++;

end_t3:
	lws_map_destroy(&map);

	/* Test 4: same key items */

	lwsl_user("%s: test4\n", __func__);
	memset(&info, 0, sizeof(info));
	map = lws_map_create(&info);
	if (!map) {
		e++;
		goto end_t4;
	}
	if (!lws_map_item_create_ks(map, "abc", (lws_map_value_t)"def", 3)) {
		e++;
		goto end_t4;
	}
	if (!lws_map_item_create_ks(map, "abc", (lws_map_value_t)"4567", 4)) {
		e++;
		goto end_t4;
	}
	item = lws_map_item_lookup_ks(map, "abc");
	if (!item) {
		e++;
		goto end_t4;
	}

	if (lws_map_item_value_len(item) != 4 ||
	    memcmp(lws_map_item_value(item), "4567", 4)) {
		e++;
		goto end_t4;
	}

	pass++;

end_t4:
	lws_map_destroy(&map);

	if (e)
		goto bail;

	lwsl_user("Completed: PASS %d / %d\n", pass, expected);

	return 0;

bail:
	lwsl_user("Completed: FAIL, passed %d / %d (e %d)\n", pass,
				expected, e);

	return 1;
}
