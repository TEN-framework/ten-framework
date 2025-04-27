 /*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2019 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 *  lws_genec provides an EC abstraction api in lws that works the
 *  same whether you are using openssl or mbedtls crypto functions underneath.
 */
#include "private-lib-core.h"

const struct lws_ec_curves *
lws_genec_curve(const struct lws_ec_curves *table, const char *name)
{
	const struct lws_ec_curves *c = lws_ec_curves;

	if (table)
		c = table;

	while (c->name) {
		if (!strcmp(name, c->name))
			return c;
		c++;
	}

	return NULL;
}

//extern const struct lws_ec_curves *lws_ec_curves;

int
lws_genec_confirm_curve_allowed_by_tls_id(const char *allowed, int id,
					  struct lws_jwk *jwk)
{
	struct lws_tokenize ts;
	lws_tokenize_elem e;
	size_t len;
	int n;

	lws_tokenize_init(&ts, allowed, LWS_TOKENIZE_F_COMMA_SEP_LIST |
				       LWS_TOKENIZE_F_MINUS_NONTERM);
	ts.len = strlen(allowed);
	do {
		e = lws_tokenize(&ts);
		switch (e) {
		case LWS_TOKZE_TOKEN:
			n = 0;
			while (lws_ec_curves[n].name) {
				if (id != lws_ec_curves[n].tls_lib_nid) {
					n++;
					continue;
				}
				lwsl_info("match curve %s\n",
					  lws_ec_curves[n].name);
				len = strlen(lws_ec_curves[n].name);
				jwk->e[LWS_GENCRYPTO_EC_KEYEL_CRV].len = (uint32_t)len;
				jwk->e[LWS_GENCRYPTO_EC_KEYEL_CRV].buf =
						lws_malloc(len + 1, "cert crv");
				if (!jwk->e[LWS_GENCRYPTO_EC_KEYEL_CRV].buf) {
					lwsl_err("%s: OOM\n", __func__);
					return 1;
				}
				memcpy(jwk->e[LWS_GENCRYPTO_EC_KEYEL_CRV].buf,
				       lws_ec_curves[n].name, len + 1);
				return 0;
			}
			break;

		case LWS_TOKZE_DELIMITER:
			break;

		default: /* includes ENDED */
			lwsl_err("%s: malformed or curve name in list\n",
				 __func__);

			return -1;
		}
	} while (e > 0);

	lwsl_err("%s: unsupported curve group nid %d\n", __func__, id);

	return -1;
}

void
lws_genec_destroy_elements(struct lws_gencrypto_keyelem *el)
{
	int n;

	for (n = 0; n < LWS_GENCRYPTO_EC_KEYEL_COUNT; n++)
		if (el[n].buf)
			lws_free_set_NULL(el[n].buf);
}

static const char *enames[] = { "crv", "x", "d", "y" };

int
lws_genec_dump(struct lws_gencrypto_keyelem *el)
{
	int n;

	(void)enames;

	lwsl_info("  genec %p: crv: '%s'\n", el,
		  !!el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf ?
		  (char *)el[LWS_GENCRYPTO_EC_KEYEL_CRV].buf: "no curve name");

	for (n = LWS_GENCRYPTO_EC_KEYEL_X; n < LWS_GENCRYPTO_EC_KEYEL_COUNT;
	     n++) {
		lwsl_info("  e: %s\n", enames[n]);
		lwsl_hexdump_info(el[n].buf, el[n].len);
	}

	lwsl_info("\n");

	return 0;
}
