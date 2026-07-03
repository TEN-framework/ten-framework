/*
 * lws-api-test-lejp
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * sanity tests for lejp
 */

#include <libwebsockets.h>

/*
 * in this example, the JSON is for one "builder" object, which may specify
 * a child list "targets" of zero or more "target" objects.
 */

static const char * const json_tests[] = {
	"{" /* test 1 */
		"\"schema\":\"com-warmcat-sai-builder\","

		"\"hostname\":\"learn\","
		"\"nspawn_timeout\":1800,"
		"\"targets\":["
			"{"
				"\"name\":\"target1\","
				"\"someflag\":true"
			"},"
			"{"
				"\"name\":\"target2\","
				"\"someflag\":false"
			"}"
		"]"
	"}",
	"{" /* test 2 */
		"\"schema\":\"com-warmcat-sai-builder\","

		"\"hostname\":\"learn\","
		"\"targets\":["
			"{"
				"\"name\":\"target1\""
			"},"
			"{"
				"\"name\":\"target2\""
			"},"
			"{"
				"\"name\":\"target3\""
			"}"
		"]"
	"}", "{" /* test 3 */
		"\"schema\":\"com-warmcat-sai-builder\","

		"\"hostname\":\"learn\","
		"\"nspawn_timeout\":1800,"
		"\"targets\":["
			"{"
				"\"name\":\"target1\","
				"\"unrecognized\":\"xyz\","
				"\"child\": {"
					"\"somename\": \"abc\","
					"\"junk\": { \"x\": \"y\" }"
				"}"
			"},"
			"{"
				"\"name\":\"target2\""
			"}"
		"]"
	"}",
	"{" /* test 4 */
		"\"schema\":\"com-warmcat-sai-builder\","

		"\"hostname\":\"learn\","
		"\"nspawn_timeout\":1800"
	"}",
	"{" /* test 5 */
		"\"schema\":\"com-warmcat-sai-builder\""
	"}",
	"{" /* test 6 ... check huge strings into smaller fixed char array */
		"\"schema\":\"com-warmcat-sai-builder\","
		"\"hostname\":\""
		"PYvtan6kqppjnS0KpYTCaiOLsJkc7XecAr1kcE0aCIciewYB+JcLG82mO1Vb1mJtjDwUjBxy2I6A"
		"zefzoWUWmqZbsv4MXR55j9bKlyz1liiSX63iO0x6JAwACMtE2MkgcLwR86TSWAD9D1QKIWqg5RJ/"
		"CRuVsW0DKAUMD52ql4JmPFuJpJgTq28z6PhYNzN3yI3bmQt6bzhA+A/xAsFzSBnb3MHYWzGMprr5"
		"3FAP1ISo5Ec9i+2ehV40sG6Q470sH3PGQZ0YRPO7Sh/SyrSQ/scONmxRc3AcXl7X/CSs417ii+CV"
		"8sq3ZgcxKNB7tNfN7idNx3upZ00G2BZy9jSy03cLKKLNaNUt0TQsxXbH55uDHzSEeZWvxJgT6zB1"
		"NoMhdC02w+oXim94M6z6COCnqT3rgkGk8PHMry9Bkh4yVpRmzIRfMmln/lEhdZgxky2+g5hhlSIG"
		"JYDCrdynD9kCfvfy6KGOpNIi1X+mhbbWn4lnL9ZKihL/RrfOV+oV4R26IDq+KqUiJBENeo8/GXkG"
		"LUH/87iPyzXKEMavr6fkrK0vTGto8yEYxmOyaVz8phG5rwf4jJgmYNoMbGo8gWvhqO7UAGy2g7MW"
		"v+B/t1eZZ+1euLsNrWAsFJiFbQKgdFfQT3RjB14iU8knlQ8usoy+pXssY2ddGJGVcGC21oZvstK9"
		"eu1eRZftda/wP+N5unT1Hw7kCoVzqxHieiYt47EGIOaaQ7XjZDK6qPN6O/grHnvJZm2vBkxuXgsY"
		"VkRQ7AuTWIecphqFsq7Wbc1YNbMW47SVU5zMD0WaCqbaaI0t4uIzRvPlD8cpiiTzFTrEHlIBTf8/"
		"uZjjEGGLhJR1jPqA9D1Ej3ChV+ye6F9JTUMlozRMsGuF8U4btDzH5xdnmvRS4Ar6LKEtAXGkj2yu"
		"yJln+v4RIWj2xOGPJovOqiXwi0FyM61f8U8gj0OiNA2/QlvrqQVDF7sMXgjvaE7iQt5vMETteZlx"
		"+z3f+jTFM/aon511W4+ZkRD+6AHwucvM9BEC\""
	"}",
	"{" /* test 7 ... check huge strings into char * */
		"\"schema\":\"com-warmcat-sai-builder\","
		"\"targets\":["
			"{"
				"\"name\":\""
		"PYvtan6kqppjnS0KpYTCaiOLsJkc7XecAr1kcE0aCIciewYB+JcLG82mO1Vb1mJtjDwUjBxy2I6A"
		"zefzoWUWmqZbsv4MXR55j9bKlyz1liiSX63iO0x6JAwACMtE2MkgcLwR86TSWAD9D1QKIWqg5RJ/"
		"CRuVsW0DKAUMD52ql4JmPFuJpJgTq28z6PhYNzN3yI3bmQt6bzhA+A/xAsFzSBnb3MHYWzGMprr5"
		"3FAP1ISo5Ec9i+2ehV40sG6Q470sH3PGQZ0YRPO7Sh/SyrSQ/scONmxRc3AcXl7X/CSs417ii+CV"
		"8sq3ZgcxKNB7tNfN7idNx3upZ00G2BZy9jSy03cLKKLNaNUt0TQsxXbH55uDHzSEeZWvxJgT6zB1"
		"NoMhdC02w+oXim94M6z6COCnqT3rgkGk8PHMry9Bkh4yVpRmzIRfMmln/lEhdZgxky2+g5hhlSIG"
		"JYDCrdynD9kCfvfy6KGOpNIi1X+mhbbWn4lnL9ZKihL/RrfOV+oV4R26IDq+KqUiJBENeo8/GXkG"
		"LUH/87iPyzXKEMavr6fkrK0vTGto8yEYxmOyaVz8phG5rwf4jJgmYNoMbGo8gWvhqO7UAGy2g7MW"
		"v+B/t1eZZ+1euLsNrWAsFJiFbQKgdFfQT3RjB14iU8knlQ8usoy+pXssY2ddGJGVcGC21oZvstK9"
		"eu1eRZftda/wP+N5unT1Hw7kCoVzqxHieiYt47EGIOaaQ7XjZDK6qPN6O/grHnvJZm2vBkxuXgsY"
		"VkRQ7AuTWIecphqFsq7Wbc1YNbMW47SVU5zMD0WaCqbaaI0t4uIzRvPlD8cpiiTzFTrEHlIBTf8/"
		"uZjjEGGLhJR1jPqA9D1Ej3ChV+ye6F9JTUMlozRMsGuF8U4btDzH5xdnmvRS4Ar6LKEtAXGkj2yu"
		"yJln+v4RIWj2xOGPJovOqiXwi0FyM61f8U8gj0OiNA2/QlvrqQVDF7sMXgjvaE7iQt5vMETteZlx"
		"+z3f+jTFM/aon511W4+ZkRD+6AHwucvM9BEC\"}]}"
	"}",
	"{" /* test 8 the "other" schema */
		"\"schema\":\"com-warmcat-sai-logs\","
		"\"task_uuid\":\"97fc90052506af8b3eb43b87aaa6fb76feab32bc128ede479a8a6b961e801f06\","
		"\"timestamp\": 170366786103,\"channel\":3, \"len\":20, "
		"\"log\": \"PnNhaWI+IE5TU1RBVEVfSU5JVAo=\"}\x0a"
		"ntu-xenial-amd64\"},{\"name\":\"linux-ubuntu-bionic-amd64\"},{\"name\":\"linux-fedora-32-x86_64\"}]}\",",

	"{" /* test 9, empty object */
		"\"a\":123,\"b\":{}"
	"}",

	"{" /* SHOULD_FAIL: test 10, missing open */
		"\"a\":123,\"b\":}"
	"}"
};

static const char * const tok[] = {
	"something",
};

static signed char
test_cb(struct lejp_ctx *ctx, char reason)
{
	lwsl_info("%s: ctx->path %s, buf %s\n", __func__, ctx->path, ctx->buf);
	return 0;
}

/* authz JSON parsing */


int main(int argc, const char **argv)
{
	int n, m, e = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	struct lejp_ctx ctx;
	const char *p;

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS API selftest: lws_struct JSON\n");

	for (m = 0; m < (int)LWS_ARRAY_SIZE(json_tests); m++) {

		lwsl_info("%s: ++++++++++++++++ test %d\n", __func__, m + 1);

		lejp_construct(&ctx, test_cb, NULL, tok, LWS_ARRAY_SIZE(tok));

		lwsl_hexdump_info(json_tests[m], strlen(json_tests[m]));

		if (m == 7)
			n = lejp_parse(&ctx, (uint8_t *)json_tests[m],
							 0xc8);
		else
			n = lejp_parse(&ctx, (uint8_t *)json_tests[m],
						 (int)strlen(json_tests[m]));

		lwsl_info("n = %d\n", n);
		if (n < 0 && m != 9) {
			lwsl_err("%s: test %d: JSON decode failed '%s'\n",
					__func__, m + 1, lejp_error_to_string(n));
			e++;
		}
		if (n >= 0 && m == 9) {
			lwsl_err("%s: test %d: JSON decode should have failed '%s'\n",
					__func__, m + 1, lejp_error_to_string(n));
			e++;
		}
	}

	{
		const char *cs;
		size_t cslen;
		cs = lws_json_simple_find("{\"blah\":123,\"ext\":{\"authorized\":1}}", 35,
					    "\"ext\":", &cslen);
		if (!cs) {
			lwsl_err("%s: simple_find failed\n", __func__);
			e++;
		} else {
			if (lws_json_simple_strcmp(cs, cslen,
					"\"authorized\":", "1"))
				e++;
		}
		cs = lws_json_simple_find("{\"blah\":123,\"auth_user\":\"andy@warmcat.com\",\"thing\":\"yeah\"}", 57,
					    "\"auth_user\":", &cslen);
		if (cslen != 16) {
			lwsl_err("%s: wrong string len %d isolated\n", __func__, (int)cslen);
			e++;
		}
	}

	if (e)
		goto bail;

	lwsl_user("Completed: PASS\n");

	return 0;

bail:
	lwsl_user("Completed: FAIL\n");

	return 1;
}
