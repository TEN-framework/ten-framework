/*
 * lws-api-test-lws_tokenize
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates the most minimal http server you can make with lws.
 *
 * To keep it simple, it serves stuff from the subdirectory 
 * "./mount-origin" of the directory it was started in.
 * You can change that by changing mount.origin below.
 */

#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>

struct expected {
	lws_tokenize_elem e;
	const char *value;
	size_t len;
};

struct tests {
	const char *string;
	struct expected *exp;
	int count;
	int flags;
};

struct expected expected1[] = {
			{ LWS_TOKZE_TOKEN,		"protocol-1", 10 },
		{ LWS_TOKZE_DELIMITER, ",", 1},
			{ LWS_TOKZE_TOKEN,		"protocol_2", 10 },
		{ LWS_TOKZE_DELIMITER, ",", 1},
			{ LWS_TOKZE_TOKEN,		"protocol3", 9 },
		{ LWS_TOKZE_ENDED, NULL, 0 },
	},
	expected2[] = {
		{ LWS_TOKZE_TOKEN_NAME_COLON,		"Accept-Language", 15 },
			{ LWS_TOKZE_TOKEN,		"fr-CH", 5 },
		{ LWS_TOKZE_DELIMITER,			",", 1 },
			{ LWS_TOKZE_TOKEN,		"fr", 2 },
			{ LWS_TOKZE_DELIMITER,		";", 1},
			{ LWS_TOKZE_TOKEN_NAME_EQUALS,	"q", 1 },
			{ LWS_TOKZE_FLOAT,		"0.9", 3 },
		{ LWS_TOKZE_DELIMITER,			",", 1 },
			{ LWS_TOKZE_TOKEN,		"en", 2 },
			{ LWS_TOKZE_DELIMITER,		";", 1},
			{ LWS_TOKZE_TOKEN_NAME_EQUALS,	"q", 1 },
			{ LWS_TOKZE_FLOAT,		"0.8", 3 },
		{ LWS_TOKZE_DELIMITER,			",", 1 },
			{ LWS_TOKZE_TOKEN,		"de", 2 },
			{ LWS_TOKZE_DELIMITER,		";", 1},
			{ LWS_TOKZE_TOKEN_NAME_EQUALS,	"q", 1 },
			{ LWS_TOKZE_FLOAT,		"0.7", 3 },
		{ LWS_TOKZE_DELIMITER, ",", 1 },
			{ LWS_TOKZE_DELIMITER,		"*", 1 },
			{ LWS_TOKZE_DELIMITER,		";", 1 },
			{ LWS_TOKZE_TOKEN_NAME_EQUALS,	"q", 1 },
			{ LWS_TOKZE_FLOAT,		"0.5", 3 },
		{ LWS_TOKZE_ENDED, NULL, 0 },
	},
	expected3[] = {
			{ LWS_TOKZE_TOKEN_NAME_EQUALS,	"quoted", 6 },
			{ LWS_TOKZE_QUOTED_STRING,	"things:", 7 },
		{ LWS_TOKZE_DELIMITER,			",", 1 },
			{ LWS_TOKZE_INTEGER,		"1234", 4 },
		{ LWS_TOKZE_ENDED, NULL, 0 },
	},
	expected4[] = {
		{ LWS_TOKZE_ERR_COMMA_LIST,		",", 1 },
	},
	expected5[] = {
			{ LWS_TOKZE_TOKEN,		"brokenlist2", 11 },
		{ LWS_TOKZE_DELIMITER, ",", 1 },
		{ LWS_TOKZE_ERR_COMMA_LIST,		",", 1 },
	},
	expected6[] = {
			{ LWS_TOKZE_TOKEN,		"brokenlist3", 11 },
		{ LWS_TOKZE_DELIMITER, ",", 1 },
		{ LWS_TOKZE_ERR_COMMA_LIST,		",", 1 },

	},
	expected7[] = {
			{ LWS_TOKZE_TOKEN, "fr", 2 },
			{ LWS_TOKZE_DELIMITER, "-", 1 },
			{ LWS_TOKZE_TOKEN, "CH", 2 },
			{ LWS_TOKZE_DELIMITER, ",", 1 },
			{ LWS_TOKZE_TOKEN, "fr", 2 },
			{ LWS_TOKZE_DELIMITER, ";", 1 },
			{ LWS_TOKZE_TOKEN_NAME_EQUALS, "q", 1 },
			{ LWS_TOKZE_FLOAT, "0.9", 3 },
			{ LWS_TOKZE_DELIMITER, ",", 1 },
			{ LWS_TOKZE_TOKEN, "en", 2 },
			{ LWS_TOKZE_DELIMITER, ";", 1 },
			{ LWS_TOKZE_TOKEN_NAME_EQUALS, "q", 1 },
			{ LWS_TOKZE_FLOAT, "0.8", 3 },
			{ LWS_TOKZE_DELIMITER, ",", 1 },
			{ LWS_TOKZE_TOKEN, "de", 2 },
			{ LWS_TOKZE_DELIMITER, ";", 1 },
			{ LWS_TOKZE_TOKEN_NAME_EQUALS, "q", 1 },
			{ LWS_TOKZE_FLOAT, "0.7", 3 },
			{ LWS_TOKZE_DELIMITER, ",", 1 },
			{ LWS_TOKZE_TOKEN, "*", 1 },
			{ LWS_TOKZE_DELIMITER, ";", 1 },
			{ LWS_TOKZE_TOKEN_NAME_EQUALS, "q", 1 },
			{ LWS_TOKZE_FLOAT, "0.5", 3 },
			{ LWS_TOKZE_ENDED, "", 0 },
	},
	expected8[] = {
		{ LWS_TOKZE_TOKEN, "Οὐχὶ", 10 },
		{ LWS_TOKZE_TOKEN, "ταὐτὰ", 12 },
		{ LWS_TOKZE_TOKEN, "παρίσταταί", 22 },
		{ LWS_TOKZE_TOKEN, "μοι", 6 },
		{ LWS_TOKZE_TOKEN, "γιγνώσκειν", 21 },
		{ LWS_TOKZE_DELIMITER, ",", 1 },
		{ LWS_TOKZE_TOKEN, "ὦ", 3 },
		{ LWS_TOKZE_TOKEN, "ἄνδρες", 13 },
		{ LWS_TOKZE_TOKEN, "᾿Αθηναῖοι", 20 },
		{ LWS_TOKZE_DELIMITER, ",", 1 },
		{ LWS_TOKZE_TOKEN, "greek", 5 },
		{ LWS_TOKZE_ENDED, "", 0 },
	},
	expected9[] = {
		/*
		 *  because the tokenizer scans ahead for = aggregation,
		 * it finds the broken utf8 before reporting the token
		 */
		{ LWS_TOKZE_ERR_BROKEN_UTF8, "", 0 },
	},
	expected10[] = {
		{ LWS_TOKZE_TOKEN, "badutf8-2", 9 },
		{ LWS_TOKZE_TOKEN, "퟿", 3 },
		{ LWS_TOKZE_DELIMITER, ",", 1 },
		{ LWS_TOKZE_ERR_BROKEN_UTF8, "", 0 },
	},
	expected11[] = {
		{ LWS_TOKZE_TOKEN, "1.myserver", 10 },
		{ LWS_TOKZE_DELIMITER, ".", 1 },
		{ LWS_TOKZE_TOKEN, "com", 3 },
		{ LWS_TOKZE_ENDED, "", 0 },
	},
	expected12[] = {
		{ LWS_TOKZE_TOKEN, "1.myserver.com", 14 },
		{ LWS_TOKZE_ENDED, "", 0 },
	},
	expected13[] = {
		{ LWS_TOKZE_TOKEN, "1.myserver.com", 14 },
		{ LWS_TOKZE_ENDED, "", 0 },
	},
	expected14[] = {
		{ LWS_TOKZE_INTEGER, "1", 1 },
		{ LWS_TOKZE_DELIMITER, ".", 1 },
		{ LWS_TOKZE_TOKEN, "myserver", 8 },
		{ LWS_TOKZE_DELIMITER, ".", 1 },
		{ LWS_TOKZE_TOKEN, "com", 3 },
		{ LWS_TOKZE_ENDED, "", 0 },
	},
	expected15[] = {
		{ LWS_TOKZE_TOKEN, "close", 5 },
		{ LWS_TOKZE_DELIMITER, ",", 1 },
		{ LWS_TOKZE_TOKEN, "Upgrade", 7 },
		{ LWS_TOKZE_ENDED, "", 0 },
	},
	expected16[] = {
		{ LWS_TOKZE_TOKEN_NAME_EQUALS, "a", 1 },
		{ LWS_TOKZE_TOKEN, "5", 1 },
		{ LWS_TOKZE_ENDED, "", 0 },
	},
	expected17[] = {
		{ LWS_TOKZE_TOKEN, "hello", 5 },
		{ LWS_TOKZE_ENDED, "", 0 },
	},
	expected18[] = {
		{ LWS_TOKZE_TOKEN, "x=y", 3 },
		{ LWS_TOKZE_ENDED, "", 0 },
	}
;

struct tests tests[] = {
	{
		" protocol-1, protocol_2\t,\tprotocol3\n",
		expected1, LWS_ARRAY_SIZE(expected1),
		LWS_TOKENIZE_F_MINUS_NONTERM | LWS_TOKENIZE_F_AGG_COLON
	}, {
		"Accept-Language: fr-CH, fr;q=0.9, en;q=0.8, de;q=0.7, *;q=0.5",
		expected2, LWS_ARRAY_SIZE(expected2),
		LWS_TOKENIZE_F_MINUS_NONTERM | LWS_TOKENIZE_F_AGG_COLON
	}, {
		"quoted = \"things:\", 1234",
		expected3, LWS_ARRAY_SIZE(expected3),
		LWS_TOKENIZE_F_MINUS_NONTERM | LWS_TOKENIZE_F_AGG_COLON
	}, {
		", brokenlist1",
		expected4, LWS_ARRAY_SIZE(expected4),
		LWS_TOKENIZE_F_COMMA_SEP_LIST
	}, {
		"brokenlist2,,",
		expected5, LWS_ARRAY_SIZE(expected5),
		LWS_TOKENIZE_F_COMMA_SEP_LIST
	}, {
		"brokenlist3,",
		expected6, LWS_ARRAY_SIZE(expected6),
		LWS_TOKENIZE_F_COMMA_SEP_LIST
	}, {
		"fr-CH, fr;q=0.9, en;q=0.8, de;q=0.7, *;q=0.5",
		expected7, LWS_ARRAY_SIZE(expected7),
		LWS_TOKENIZE_F_ASTERISK_NONTERM | LWS_TOKENIZE_F_RFC7230_DELIMS
	},
	{
		" Οὐχὶ ταὐτὰ παρίσταταί μοι γιγνώσκειν, ὦ ἄνδρες ᾿Αθηναῖοι, greek",
		expected8, LWS_ARRAY_SIZE(expected8),
		LWS_TOKENIZE_F_RFC7230_DELIMS
	},
	{
		"badutf8-1 \x80...",
		expected9, LWS_ARRAY_SIZE(expected9),
		LWS_TOKENIZE_F_MINUS_NONTERM | LWS_TOKENIZE_F_RFC7230_DELIMS
	},
	{
		"badutf8-2 \xed\x9f\xbf,\x80...",
		expected10, LWS_ARRAY_SIZE(expected10),
		LWS_TOKENIZE_F_MINUS_NONTERM | LWS_TOKENIZE_F_RFC7230_DELIMS
	},
	{
		"1.myserver.com",
		expected11, LWS_ARRAY_SIZE(expected11),
		0
	},
	{
		"1.myserver.com",
		expected12, LWS_ARRAY_SIZE(expected12),
		LWS_TOKENIZE_F_DOT_NONTERM
	},
	{
		"1.myserver.com",
		expected13, LWS_ARRAY_SIZE(expected13),
		LWS_TOKENIZE_F_DOT_NONTERM | LWS_TOKENIZE_F_NO_FLOATS
	},
	{
		"1.myserver.com",
		expected14, LWS_ARRAY_SIZE(expected14),
		LWS_TOKENIZE_F_NO_FLOATS
	},
	{
		"close,  Upgrade",
		expected15, LWS_ARRAY_SIZE(expected15),
		LWS_TOKENIZE_F_COMMA_SEP_LIST
	},
	{
		"a=5", expected16, LWS_ARRAY_SIZE(expected16),
		LWS_TOKENIZE_F_NO_INTEGERS
	},
	{
		"# comment1\r\nhello #comment2\r\n#comment3", expected17,
		LWS_ARRAY_SIZE(expected17), LWS_TOKENIZE_F_HASH_COMMENT
	},
	{
		"x=y", expected18,
		LWS_ARRAY_SIZE(expected18), LWS_TOKENIZE_F_EQUALS_NONTERM
	}
};

/*
 * add LWS_TOKZE_ERRS to the element index (which may be negative by that
 * amount) to index this array
 */

static const char *element_names[] = {
	"LWS_TOKZE_ERR_BROKEN_UTF8",
	"LWS_TOKZE_ERR_UNTERM_STRING",
	"LWS_TOKZE_ERR_MALFORMED_FLOAT",
	"LWS_TOKZE_ERR_NUM_ON_LHS",
	"LWS_TOKZE_ERR_COMMA_LIST",
	"LWS_TOKZE_ENDED",
	"LWS_TOKZE_DELIMITER",
	"LWS_TOKZE_TOKEN",
	"LWS_TOKZE_INTEGER",
	"LWS_TOKZE_FLOAT",
	"LWS_TOKZE_TOKEN_NAME_EQUALS",
	"LWS_TOKZE_TOKEN_NAME_COLON",
	"LWS_TOKZE_QUOTED_STRING",
};


int
exp_cb1(void *priv, const char *name, char *out, size_t *pos, size_t olen,
	size_t *exp_ofs)
{
	const char *replace = NULL;
	size_t total, budget;

	if (!strcmp(name, "test")) {
		replace = "replacement_string";
		total = strlen(replace);
		goto expand;
	}

	return LSTRX_FATAL_NAME_UNKNOWN;

expand:
	budget = olen - *pos;
	total -= *exp_ofs;
	if (total < budget)
		budget = total;

	if (out)
		memcpy(out + *pos, replace + (*exp_ofs), budget);
	*exp_ofs += budget;
	*pos += budget;

	if (budget == total)
		return LSTRX_DONE;

	return LSTRX_FILLED_OUT;
}

static const char *exp_inp1 = "this-is-a-${test}-for-strexp";

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *cx;
	struct lws_tokenize ts;
	lws_tokenize_elem e;
	const char *p;
	int n, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
			/* for LLL_ verbosity above NOTICE to be built into lws,
			 * lws must have been configured and built with
			 * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
			/* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
			/* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
			/* | LLL_DEBUG */;
	int fail = 0, ok = 0, flags = 0;
	char dotstar[512];

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS API selftest: lws_tokenize\n");

	if ((p = lws_cmdline_option(argc, argv, "-f")))
		flags = atoi(p);


	memset(&info, 0, sizeof info);
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
		       LWS_SERVER_OPTION_H2_JUST_FIX_WINDOW_UPDATE_OVERFLOW;

	/*
	 * since we know this lws context is only ever going to be used with
	 * one client wsis / fds / sockets at a time, let lws know it doesn't
	 * have to use the default allocations for fd tables up to ulimit -n.
	 * It will just allocate for 1 internal and 1 (+ 1 http2 nwsi) that we
	 * will use.
	 */
	info.fd_limit_per_thread = 1 + 1 + 1;

#if 0
#if defined(LWS_WITH_MBEDTLS) || defined(USE_WOLFSSL)
	/*
	 * OpenSSL uses the system trust store.  mbedTLS has to be told which
	 * CA to trust explicitly.
	 */
	info.client_ssl_ca_filepath = "./warmcat.com.cer";
#endif
#endif
#if 0
	n = open("./warmcat.com.cer", O_RDONLY);
	if (n >= 0) {
		info.client_ssl_ca_mem_len = read(n, memcert, sizeof(memcert));
		info.client_ssl_ca_mem = memcert;
		close(n);
		n = 0;
		memcert[info.client_ssl_ca_mem_len++] = '\0';
	}
#endif
	cx = lws_create_context(&info);

	/* lws_strexp */

	{
		size_t in_len, used_in, used_out;
		lws_strexp_t exp;
		char obuf[128];
		const char *p;

		obuf[0] = '\0';
		lws_strexp_init(&exp, NULL, exp_cb1, obuf, sizeof(obuf));
		n = lws_strexp_expand(&exp, exp_inp1, 28, &used_in, &used_out);
		if (n != LSTRX_DONE || used_in != 28 ||
		    strcmp(obuf, "this-is-a-replacement_string-for-strexp")) {
			lwsl_notice("%s: obuf %s\n", __func__, obuf);
			lwsl_err("%s: lws_strexp test 1 failed: %d\n", __func__, n);

			return 1;
		}

		/* as above, but don't generate output, just find the length */

		lws_strexp_init(&exp, NULL, exp_cb1, NULL, (size_t)-1);
		n = lws_strexp_expand(&exp, exp_inp1, 28, &used_in, &used_out);
		if (n != LSTRX_DONE || used_in != 28 || used_out != 39) {
			lwsl_err("%s: lws_strexp test 2 failed: %d, used_out: %d\n",
					__func__, n, (int)used_out);

			return 1;
		}

		p = exp_inp1;
		in_len = strlen(p);
		memset(obuf, 0, sizeof(obuf));
		lws_strexp_init(&exp, NULL, exp_cb1, obuf, 16);
		n = lws_strexp_expand(&exp, p, in_len, &used_in, &used_out);
		if (n != LSTRX_FILLED_OUT || used_in != 16 || used_out != 16) {
			lwsl_err("a\n");
			return 1;
		}

		p += used_in;
		in_len -= used_in;

		memset(obuf, 0, sizeof(obuf));
		lws_strexp_reset_out(&exp, obuf, 16);

		n = lws_strexp_expand(&exp, p, in_len, &used_in, &used_out);
		if (n != LSTRX_FILLED_OUT || used_in != 5 || used_out != 16) {
			lwsl_err("b: n %d, used_in %d, used_out %d\n", n,
					(int)used_in, (int)used_out);
			return 2;
		}

		p += used_in;
		in_len -= used_in;

		memset(obuf, 0, sizeof(obuf));
		lws_strexp_reset_out(&exp, obuf, 16);

		n = lws_strexp_expand(&exp, p, in_len, &used_in, &used_out);
		if (n != LSTRX_DONE || used_in != 7 || used_out != 7) {
			lwsl_err("c: n %d, used_in %d, used_out %d\n", n, (int)used_in, (int)used_out);
			return 2;
		}
	}

	/* sanity check lws_strnncpy() */

	lws_strnncpy(dotstar, "12345678", 4, sizeof(dotstar));
	if (strcmp(dotstar, "1234")) {
		lwsl_err("%s: lws_strnncpy check failed\n", __func__);

		return 1;
	}
	lws_strnncpy(dotstar, "12345678", 8, 6);
	if (strcmp(dotstar, "12345")) {
		lwsl_err("%s: lws_strnncpy check failed\n", __func__);

		return 1;
	}

	/* sanity check lws_nstrstr() */

	{
		static const char *t1 = "abc123456";
		const char *mcp;

		mcp = lws_nstrstr(t1, strlen(t1), "abc", 3);
		if (mcp != t1) {
			lwsl_err("%s: lws_nstrstr 1 failed\n", __func__);
			return 1;
		}
		mcp = lws_nstrstr(t1, strlen(t1), "def", 3);
		if (mcp != NULL) {
			lwsl_err("%s: lws_nstrstr 2 failed\n", __func__);
			return 1;
		}
		mcp = lws_nstrstr(t1, strlen(t1), "456", 3);
		if (mcp != t1 + 6) {
			lwsl_err("%s: lws_nstrstr 3 failed: %p\n", __func__, mcp);
			return 1;
		}
		mcp = lws_nstrstr(t1, strlen(t1), "1", 1);
		if (mcp != t1 + 3) {
			lwsl_err("%s: lws_nstrstr 4 failed\n", __func__);
			return 1;
		}
		mcp = lws_nstrstr(t1, strlen(t1), "abc1234567", 10);
		if (mcp != NULL) {
			lwsl_err("%s: lws_nstrstr 5 failed\n", __func__);
			return 1;
		}
	}

	/* sanity check lws_json_simple_find() */

	{
		static const char *t1 = "{\"myname1\":true,"
					 "\"myname2\":\"string\", "
					 "\"myname3\": 123}";
		size_t alen;
		const char *mcp;

		mcp = lws_json_simple_find(t1, strlen(t1), "\"myname1\":", &alen);
		if (mcp != t1 + 11 || alen != 4) {
			lwsl_err("%s: lws_json_simple_find 1 failed: (%d) %s\n",
				 __func__, (int)alen, mcp);
			return 1;
		}

		mcp = lws_json_simple_find(t1, strlen(t1), "\"myname2\":", &alen);
		if (mcp != t1 + 27 || alen != 6) {
			lwsl_err("%s: lws_json_simple_find 2 failed\n", __func__);
			return 1;
		}

		mcp = lws_json_simple_find(t1, strlen(t1), "\"myname3\":", &alen);
		if (mcp != t1 + 47 || alen != 3) {
			lwsl_err("%s: lws_json_simple_find 3 failed\n", __func__);
			return 1;
		}

		mcp = lws_json_simple_find(t1, strlen(t1), "\"nope\":", &alen);
		if (mcp != NULL) {
			lwsl_err("%s: lws_json_simple_find 4 failed\n", __func__);
			return 1;
		}
	}

	p = lws_cmdline_option(argc, argv, "-s");

	for (n = 0; n < (int)LWS_ARRAY_SIZE(tests); n++) {
		int m = 0, in_fail = fail;
		struct expected *exp = tests[n].exp;

		memset(&ts, 0, sizeof(ts));
		ts.start = tests[n].string;
		ts.len = strlen(ts.start);
		ts.flags = (uint16_t)tests[n].flags;

		do {
			e = lws_tokenize(&ts);

			lws_strnncpy(dotstar, ts.token, ts.token_len,
				     sizeof(dotstar));
			lwsl_info("{ %s, \"%s\", %d }\n",
				  element_names[e + LWS_TOKZE_ERRS], dotstar,
				  (int)ts.token_len);

			if (m == (int)tests[n].count) {
				lwsl_notice("fail: expected end earlier\n");
				fail++;
				break;
			}

			if (e != exp->e) {
				lwsl_notice("fail... tok %s vs expected %s\n",
					element_names[e + LWS_TOKZE_ERRS],
					element_names[exp->e + LWS_TOKZE_ERRS]);
				fail++;
				break;
			}

			if (e > 0 &&
			    (ts.token_len != exp->len ||
			     memcmp(exp->value, ts.token, exp->len))) {
				lws_strnncpy(dotstar, ts.token, ts.token_len,
					     sizeof(dotstar));
				lwsl_notice("fail token mismatch %d %d %s\n",
					    (int)ts.token_len, (int)exp->len,
					    dotstar);
				fail++;
				break;
			}

			m++;
			exp++;

		} while (e > 0);

		if (fail == in_fail)
			ok++;
	}

	if (p) {
		ts.start = p;
		ts.len = strlen(p);
		ts.flags = (uint16_t)flags;

		printf("\t{\n\t\t\"%s\",\n"
		       "\t\texpected%d, LWS_ARRAY_SIZE(expected%d),\n\t\t",
		       p, (int)LWS_ARRAY_SIZE(tests) + 1,
		       (int)LWS_ARRAY_SIZE(tests) + 1);

		if (!flags)
			printf("0\n\t},\n");
		else {
			if (flags & LWS_TOKENIZE_F_MINUS_NONTERM)
				printf("LWS_TOKENIZE_F_MINUS_NONTERM");
			if (flags & LWS_TOKENIZE_F_AGG_COLON) {
				if (flags & 1)
					printf(" | ");
				printf("LWS_TOKENIZE_F_AGG_COLON");
			}
			if (flags & LWS_TOKENIZE_F_COMMA_SEP_LIST) {
				if (flags & 3)
					printf(" | ");
				printf("LWS_TOKENIZE_F_COMMA_SEP_LIST");
			}
			if (flags & LWS_TOKENIZE_F_RFC7230_DELIMS) {
				if (flags & 7)
					printf(" | ");
				printf("LWS_TOKENIZE_F_RFC7230_DELIMS");
			}
			if (flags & LWS_TOKENIZE_F_DOT_NONTERM) {
				if (flags & 15)
					printf(" | ");
				printf("LWS_TOKENIZE_F_DOT_NONTERM");
			}
			if (flags & LWS_TOKENIZE_F_NO_FLOATS) {
				if (flags & 31)
					printf(" | ");
				printf("LWS_TOKENIZE_F_NO_FLOATS");
			}
			printf("\n\t},\n");
		}

		printf("\texpected%d[] = {\n", (int)LWS_ARRAY_SIZE(tests) + 1);

		do {
			e = lws_tokenize(&ts);

			lws_strnncpy(dotstar, ts.token, ts.token_len,
				     sizeof(dotstar));

			printf("\t\t{ %s, \"%s\", %d },\n",
				  element_names[e + LWS_TOKZE_ERRS],
				  dotstar, (int)ts.token_len);

		} while (e > 0);

		printf("\t}\n");
	}

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	{
		time_t t;

		if (lws_http_date_parse_unix("Tue, 15 Nov 1994 08:12:31 GMT", 29, &t)) {
			lwsl_err("%s: date parse failed\n", __func__);
			fail++;
		} else {
			/* lwsl_notice("%s: %llu\n", __func__, (unsigned long long)t); */
			if (t != (time_t)784887151) {
				lwsl_err("%s: date parse wrong\n", __func__);
				fail++;
			} else {
				char s[30];

				if (lws_http_date_render_from_unix(s, sizeof(s), &t)) {
					lwsl_err("%s: failed date render\n", __func__);
					fail++;
				} else {
					if (!strcmp(s, "Tue, 15 Nov 1994 08:12:31 GMT")) {
						lwsl_err("%s: date render wrong\n", __func__);
						fail++;
					}
				}
			}
		}
	}
#endif

	{
		char buf[24];
		int m;

		m = lws_humanize(buf, sizeof(buf), 0, humanize_schema_si);
		if (m != 1 || strcmp(buf, "0")) {
			lwsl_user("%s: humanize 1 fail '%s' (%d)\n", __func__, buf, m);
			fail++;
		}
		m = lws_humanize(buf, sizeof(buf), 2, humanize_schema_si);
		if (m != 1 || strcmp(buf, "2")) {
			lwsl_user("%s: humanize 2 fail '%s' (%d)\n", __func__, buf, m);
			fail++;
		}
		m = lws_humanize(buf, sizeof(buf), 999, humanize_schema_si);
		if (m != 3 || strcmp(buf, "999")) {
			lwsl_user("%s: humanize 3 fail '%s' (%d)\n", __func__, buf, m);
			fail++;
		}
		m = lws_humanize(buf, sizeof(buf), 1000, humanize_schema_si);
		if (m != 4 || strcmp(buf, "1000")) {
			lwsl_user("%s: humanize 4 fail '%s' (%d)\n", __func__, buf, m);
			fail++;
		}
		m = lws_humanize(buf, sizeof(buf), 1024, humanize_schema_si);
		if (m != 7 || strcmp(buf, "1.000Ki")) {
			lwsl_user("%s: humanize 5 fail '%s' (%d)\n", __func__, buf, m);
			fail++;
		}
	}

	if (lws_strcmp_wildcard("allied", 6, "allied", 6)) {
		lwsl_user("%s: wc 1 fail\n", __func__);
		fail++;
	}
	if (lws_strcmp_wildcard("a*", 2, "allied", 6)) {
		lwsl_user("%s: wc 2 fail\n", __func__);
		fail++;
	}
	if (lws_strcmp_wildcard("all*", 4, "allied", 6)) {
		lwsl_user("%s: wc 3 fail\n", __func__);
		fail++;
	}
	if (lws_strcmp_wildcard("all*d", 5, "allied", 6)) {
		lwsl_user("%s: wc 4 fail\n", __func__);
		fail++;
	}
	if (!lws_strcmp_wildcard("b*", 2, "allied", 6)) {
		lwsl_user("%s: wc 5 fail\n", __func__);
		fail++;
	}
	if (!lws_strcmp_wildcard("b*ed", 4, "allied", 6)) {
		lwsl_user("%s: wc 6 fail\n", __func__);
		fail++;
	}
	if (!lws_strcmp_wildcard("allie", 5, "allied", 6)) {
		lwsl_user("%s: wc 7 fail\n", __func__);
		fail++;
	}
	if (lws_strcmp_wildcard("allie*", 6, "allied", 6)) {
		lwsl_user("%s: wc 8 fail\n", __func__);
		fail++;
	}
	if (lws_strcmp_wildcard("*llie*", 6, "allied", 6)) {
		lwsl_user("%s: wc 9 fail\n", __func__);
		fail++;
	}
	if (lws_strcmp_wildcard("*llied", 6, "allied", 6)) {
		lwsl_user("%s: wc 10 fail\n", __func__);
		fail++;
	}
	if (!lws_strcmp_wildcard("*llie", 5, "allied", 6)) {
		lwsl_user("%s: wc 11 fail\n", __func__);
		fail++;
	}
	if (!lws_strcmp_wildcard("*nope", 5, "allied", 6)) {
		lwsl_user("%s: wc 12 fail\n", __func__);
		fail++;
	}
	if (lws_strcmp_wildcard("*li*", 4, "allied", 6)) {
		lwsl_user("%s: wc 13 fail\n", __func__);
		fail++;
	}
	if (lws_strcmp_wildcard("*", 1, "allied", 6)) {
		lwsl_user("%s: wc 14 fail\n", __func__);
		fail++;
	}
	if (lws_strcmp_wildcard("*abc*d", 6, "xxabyyabcdd", 11)) {
		lwsl_user("%s: wc 15 fail\n", __func__);
		fail++;
	}
	if (lws_strcmp_wildcard("ssproxy.n.cn.*", 14,
				"ssproxy.n.cn.failures", 21)) {
		lwsl_user("%s: wc 16 fail\n", __func__);
		fail++;
	}

	lwsl_user("Completed: PASS: %d, FAIL: %d\n", ok, fail);

	lws_context_destroy(cx);

	return !(ok && !fail);
}
