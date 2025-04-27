/*
 * lws-minimal-crypto-cose-key
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>
#include <sys/select.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>

static int fdin = 0, fdout = 1;

static const char *meta_names[] = {
	"kty", "kid", "use", "key_ops", "base_iv", "alg"
};

static const char *oct_names[] = {
	"k"
};

static const char *rsa_names[] = {
	"e", "n", "d", "p", "q", "dp", "dq", "qi", "other", "ri", "di", "ti"
};

static const char *ec_names[] = {
	"crv", "x", "d", "y",
};

static void
cose_key_dump(const struct lws_cose_key *ck)
{
	const char **enames;
	char hex[2048], dump[3072];
	int elems;
	size_t l;
	int n;

	(void)enames;
	(void)meta_names;

	switch (ck->gencrypto_kty) {

	case LWS_GENCRYPTO_KTY_OCT:
		elems = LWS_GENCRYPTO_OCT_KEYEL_COUNT;
		enames = oct_names;
		break;
	case LWS_GENCRYPTO_KTY_RSA:
		elems = LWS_GENCRYPTO_RSA_KEYEL_COUNT;
		enames = rsa_names;
		break;
	case LWS_GENCRYPTO_KTY_EC:
		elems = LWS_GENCRYPTO_EC_KEYEL_COUNT;
		enames = ec_names;
		break;

	default:
		lwsl_err("%s: jwk %p: unknown type\n", __func__, ck);

		return;
	}

	for (n = 0; n < LWS_COUNT_COSE_KEY_ELEMENTS; n++) {
		if (ck->meta[n].buf) {
			if (n < 2) {
				l = (size_t)lws_snprintf(dump, sizeof(dump),
						 "  %s: %.*s\n", meta_names[n],
						 (int)ck->meta[n].len,
						 ck->meta[n].buf);
				write(fdout, dump, l);
			} else {
				l = (size_t)lws_snprintf(dump, sizeof(dump),
						 "  %s: ", meta_names[n]);
				write(fdout, dump, l);
				lws_hex_from_byte_array(ck->meta[n].buf,
							ck->meta[n].len,
							hex, sizeof(hex));
				write(fdout, hex, strlen(hex));
				write(fdout, "\n", 1);
			}
		}
	}

	for (n = 0; n < elems; n++) {
		if (ck->e[n].buf) {
			if (!n && ck->gencrypto_kty == LWS_GENCRYPTO_KTY_EC) {
				l = (size_t)lws_snprintf(dump, sizeof(dump),
						 "  %s: %.*s\n", enames[n],
						 (int)ck->e[n].len,
						 ck->e[n].buf);
				write(fdout, dump, l);
			} else {
				l = (size_t)lws_snprintf(dump, sizeof(dump),
						 "  %s: ", enames[n]);
				write(fdout, dump, l);
				lws_hex_from_byte_array(ck->e[n].buf,
							ck->e[n].len,
							hex, sizeof(hex));
				write(fdout, hex, strlen(hex));
				write(fdout, "\n", 1);
			}
		}
	}
}

int main(int argc, const char **argv)
{
	uint8_t *kid = NULL, ktmp[4096], set_temp[32 * 1024], temp[256];
	int result = 1, bits = 0,
	    logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	struct lws_context_creation_info info;
	size_t kid_len = 0, stp = 0;
	struct lws_context *context;
	lws_cose_key_t *ck = NULL;
	cose_param_t cose_kty = 0;
	lws_dll2_owner_t set;
	const char *p, *crv;
	lws_lec_pctx_t lec;

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);

	lwsl_user("LWS cose-key example tool -k keyset [-s alg-name kid ]\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
#if defined(LWS_WITH_NETWORK)
	info.port = CONTEXT_PORT_NO_LISTEN;
#endif

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	if ((p = lws_cmdline_option(argc, argv, "--stdin"))) {
		fdin = open(p, LWS_O_RDONLY, 0);
		if (fdin < 0) {
			lwsl_err("%s: unable to open stdin file\n", __func__);
			return 1;
		}
	}

	if ((p = lws_cmdline_option(argc, argv, "--stdout"))) {
		fdout = open(p, LWS_O_WRONLY | LWS_O_CREAT | LWS_O_TRUNC, 0600);
		if (fdout < 0) {
			lwsl_err("%s: unable to open stdout file\n", __func__);
			goto bail_early;
		}
	}

	if ((p = lws_cmdline_option(argc, argv, "--kid"))) {
		kid = (uint8_t *)p;
		kid_len = strlen(p);
		//lwsl_hexdump_notice(kid, kid_len);
	}

	if ((p = lws_cmdline_option(argc, argv, "--kid-hex"))) {
		kid_len = (size_t)lws_hex_to_byte_array(p, ktmp, sizeof(ktmp));
		kid = (uint8_t *)ktmp;
	}

	/*
	 * If we have some stdin queued up, we understand we are dumping
	 * an existing cose_key or key_set from stdin
	 */

	if (!fdin) {
		struct timeval	timeout;
		fd_set	fds;

		FD_ZERO(&fds);
		FD_SET(0, &fds);

		timeout.tv_sec  = 0;
		timeout.tv_usec = 1000;

		if (select(fdin + 1, &fds, NULL, NULL, &timeout) < 0)
			goto no_stdin;

		if (!FD_ISSET(0, &fds))
			goto no_stdin;
	}

	do {
		int n = (int)read(fdin, temp, sizeof(temp));

		if (n < 0)
			goto bail;
		if (!n) {
			int kc = 0;

			if (!stp)
				/* there was no stdin */
				break;

			lwsl_notice("%s: importing\n", __func__);

			lws_dll2_owner_clear(&set);
			ck = lws_cose_key_import(&set, NULL, NULL, set_temp, stp);
			if (!ck) {
				lwsl_err("%s: import failed\n", __func__);
				goto bail;
			}

			lws_start_foreach_dll(struct lws_dll2 *, p,
						lws_dll2_get_head(&set)) {
				lws_cose_key_t *ck = lws_container_of(p,
							lws_cose_key_t, list);
				struct lws_gencrypto_keyelem *ke =
						&ck->meta[COSEKEY_META_KID];

				kc++;

				if (!kid_len || (ke->len &&
				    ke->len == (uint32_t)kid_len &&
				    !memcmp(ke->buf, kid, kid_len))) {
					    printf("Cose key #%d\n", kc);
					    cose_key_dump(ck);
				}

			} lws_end_foreach_dll(p);

			lws_cose_key_set_destroy(&set);
			result = 0;
			goto bail;

		}

		if (stp + (size_t)n > sizeof(set_temp)) {
			lwsl_err("%s: stdin bigger than our buffer\n", __func__);
			goto bail;
		}
		memcpy(set_temp + stp, temp, (size_t)n);
		stp += (size_t)n;
	} while (1);

no_stdin:

	/*
	 *
	 */

	p = lws_cmdline_option(argc, argv, "--kty");
	if (!p) {
		lwsl_err("%s: use --kty OKP|EC2|RSA|SYMMETRIC\n",
					__func__);
		goto bail;
	}

	if (!strcmp(p, "OKP"))
		cose_kty = LWSCOSE_WKKTV_OKP;
	if (!strcmp(p, "EC2"))
		cose_kty = LWSCOSE_WKKTV_EC2;
	if (!strcmp(p, "RSA"))
		cose_kty = LWSCOSE_WKKTV_RSA;
	if (!strcmp(p, "SYMMETRIC") || !strcmp(p, "SYM"))
		cose_kty = LWSCOSE_WKKTV_SYMMETRIC;

	if (!cose_kty) {
		lwsl_err("%s: use --kty OKP|EC2|RSA|SYMMETRIC\n",
			 __func__);
		goto bail;
	}

	crv = NULL;
	if (cose_kty == LWSCOSE_WKKTV_OKP ||
	    cose_kty == LWSCOSE_WKKTV_EC2) {
		crv = lws_cmdline_option(argc, argv, "--curve");
		if (!crv) {
			lwsl_err("%s: use --curve P-256 etc\n", __func__);
			goto bail;
		}
	}

	p = lws_cmdline_option(argc, argv, "--bits");
	if (p)
		bits = atoi(p);

	ck = lws_cose_key_generate(context, cose_kty, 0, bits, crv,
				   kid, kid_len);
	if (!ck)
		goto bail;

	lws_lec_init(&lec, ktmp, sizeof(ktmp));
	lws_cose_key_export(ck, &lec, LWSJWKF_EXPORT_PRIVATE);
	write(fdout, ktmp, lec.used);

	lws_cose_key_destroy(&ck);
	result = 0;

bail:
	lws_context_destroy(context);

	if (result)
		lwsl_err("%s: FAIL: %d\n", __func__, result);
	else
		lwsl_notice("%s: PASS\n", __func__);

bail_early:
	if (fdin > 0)
		close(fdin);
	if (fdout != 1 && fdout >= 0)
		close(fdout);

	return result;
}
