/*
 * lws-minimal-crypto-cose-sign
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <libwebsockets.h>
#include <sys/types.h>
#include <fcntl.h>

static int fdin = 0, fdout = 1;
static uint8_t extra[4096];
static size_t ext_len;

int
_alloc_file(struct lws_context *context, const char *filename, uint8_t **buf,
		size_t *amount)
{
	FILE *f;
	size_t s;
	ssize_t m;
	int n = 0;

	f = fopen(filename, "rb");
	if (f == NULL) {
		n = 1;
		goto bail;
	}

	if (fseek(f, 0, SEEK_END) != 0) {
		n = 1;
		goto bail;
	}

	m = ftell(f);
	if (m == -1l) {
		n = 1;
		goto bail;
	}
	s = (size_t)m;

	if (fseek(f, 0, SEEK_SET) != 0) {
		n = 1;
		goto bail;
	}

	*buf = malloc(s + 1);
	if (!*buf) {
		n = 2;
		goto bail;
	}

	if (fread(*buf, s, 1, f) != 1) {
		free(*buf);
		n = 1;
		goto bail;
	}

	*amount = s;

bail:
	if (f)
		fclose(f);

	return n;

}

static int
extra_cb(lws_cose_sig_ext_pay_t *x)
{
	x->ext = extra;
	x->xl = ext_len;

	// lwsl_hexdump_notice(extra, ext_len);

	return 0;
}

int
pay_cb(struct lws_cose_validate_context *cps, void *opaque,
       const uint8_t *paychunk, size_t paychunk_len)
{
	write(fdout, paychunk, paychunk_len);

	return 0;
}

int main(int argc, const char **argv)
{
	uint8_t *ks, temp[256], *kid = NULL, ktmp[4096], sbuf[512];
	int n, m, sign = 0, result = 1,
	    logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
	enum lws_cose_sig_types sigtype = SIGTYPE_UNKNOWN;
	struct lws_cose_validate_context *cps = NULL;
	struct lws_cose_sign_context *csc = NULL;
	const struct lws_gencrypto_keyelem *ke;
	struct lws_context_creation_info info;
	lws_cose_validate_create_info_t vi;
	struct lws_buflist *paybuf = NULL;
	lws_cose_sign_create_info_t i;
	struct lws_context *context;
	size_t ks_len, kid_len = 0;
	lws_cose_key_t *ck = NULL;
	lws_dll2_owner_t *o, set;
	lws_lec_pctx_t lec;
	cose_param_t alg;
	const char *p;

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);

	lwsl_user("LWS cose-sign example tool -k keyset [-s alg-name kid ]\n");

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

	/*
	 * If no tag, you can tell it the signature type, otherwise it will
	 * use the tag to select the right type without these
	 */

	if (lws_cmdline_option(argc, argv, "--cose-sign"))
		sigtype = SIGTYPE_MULTI;

	if (lws_cmdline_option(argc, argv, "--cose-sign1"))
		sigtype = SIGTYPE_SINGLE;

	if (lws_cmdline_option(argc, argv, "--cose-mac"))
		sigtype = SIGTYPE_MAC;

	if (lws_cmdline_option(argc, argv, "--cose-mac0"))
		sigtype = SIGTYPE_MAC0;

	/* if signing, set the ciphers */

	if (lws_cmdline_option(argc, argv, "-s"))
		sign = 1;

	if ((p = lws_cmdline_option(argc, argv, "--kid"))) {
		kid = (uint8_t *)p;
		kid_len = strlen(p);
		//lwsl_hexdump_notice(kid, kid_len);
	}

	if ((p = lws_cmdline_option(argc, argv, "--kid-hex"))) {
		kid_len = (size_t)lws_hex_to_byte_array(p, ktmp, sizeof(ktmp));
		kid = (uint8_t *)ktmp;
	}

	if ((p = lws_cmdline_option(argc, argv, "--extra"))) {
		ext_len = (size_t)lws_hex_to_byte_array(p, extra, sizeof(extra));
		lwsl_notice("%llu\n", (unsigned long long)ext_len);
		if (ext_len == (size_t)-1ll)
			ext_len = 0;
	}

	/* grab the key */

	if (!(p = lws_cmdline_option(argc, argv, "-k"))) {
		lwsl_err("-k <key set file> is required\n");
		goto bail;
	}

	if (_alloc_file(context, p, &ks, &ks_len)) {
		lwsl_err("%s: unable to load %s\n", __func__, p);
		goto bail;
	}

	lws_dll2_owner_clear(&set);
	if (!lws_cose_key_import(&set, NULL, NULL, ks, ks_len)) {
		lwsl_notice("%s: key import fail\n", __func__);
		free(ks);
		goto bail2;
	}

	free(ks);

	if (!fdin) {
		struct timeval	timeout;
		fd_set	fds;

		FD_ZERO(&fds);
		FD_SET(0, &fds);

		timeout.tv_sec  = 0;
		timeout.tv_usec = 1000;

		if (select(fdin + 1, &fds, NULL, NULL, &timeout) < 0 ||
		    !FD_ISSET(0, &fds)) {
			lwsl_err("%s: pass cose_sign or plaintext "
				 "on stdin or --stdin\n", __func__);
			goto bail2;
		}
	}

	if (sign) {
		uint8_t *ppay;
		size_t s;

		p = lws_cmdline_option(argc, argv, "--alg");
		if (!p) {
			lwsl_err("%s: need to specify alg (eg, ES256) "
				 "when signing\n", __func__);
			goto bail2;
		}
		alg = lws_cose_name_to_alg(p);

		lws_lec_init(&lec, sbuf, sizeof(sbuf));
		memset(&i, 0, sizeof(i));
		i.cx		= context;
		i.keyset	= &set;
		i.lec		= &lec;
		i.flags		= LCSC_FL_ADD_CBOR_TAG |
				  LCSC_FL_ADD_CBOR_PREFER_MAC0;
		i.sigtype	= sigtype;

		/*
		 * Unfortunately, with COSE we must know the payload length
		 * before we have seen the payload.  It's illegal to use
		 * indeterminite lengths inside COSE objects.
		 */

		do {
			n = (int)read(fdin, temp, sizeof(temp));
			if (n < 0)
				goto bail3;
			if (!n)
				break;

			s = (size_t)n;

			if (lws_buflist_append_segment(&paybuf, temp, s) < 0)
				goto bail3;
			i.inline_payload_len += s;

		} while (1);

	//	lwsl_notice("%s: inline_payload_len %llu\n", __func__,
	//			(unsigned long long)i.inline_payload_len);

		csc = lws_cose_sign_create(&i);
		if (!csc)
			goto bail2;
		ck = lws_cose_key_from_set(&set, kid, kid_len);
		if (!ck)
			goto bail2;

		if (lws_cose_sign_add(csc, alg, ck))
			goto bail2;

		do {
			s = lws_buflist_next_segment_len(&paybuf, &ppay);
			if (!s)
				break;

			do {
				m = (int)lws_cose_sign_payload_chunk(csc,
								     ppay, s);
				if (lec.used) {
					// lwsl_hexdump_err(sbuf, lec.used);
					write(fdout, sbuf, lec.used);
					lws_lec_setbuf(&lec, sbuf, sizeof(sbuf));
				}
			} while (m == LCOSESIGEXTCB_RET_AGAIN);

			if (m == LWS_LECPCTX_RET_FAIL)
				goto bail2;

			if (lec.used) {
				write(fdout, sbuf, lec.used);
				lws_lec_setbuf(&lec, sbuf, sizeof(sbuf));
			}

			lws_buflist_use_segment(&paybuf, s);
		} while(1);

	} else {
		memset(&vi, 0, sizeof(vi));

		vi.cx		= context;
		vi.keyset	= &set;
		vi.sigtype	= sigtype;
		vi.ext_cb	= extra_cb;
		vi.ext_opaque	= extra;
		vi.ext_len	= ext_len;
		vi.pay_cb	= pay_cb;

		cps = lws_cose_validate_create(&vi);
		if (!cps) {
			lwsl_notice("%s: sign_val_create fail\n", __func__);
			goto bail;
		}

		do {
			n = (int)read(fdin, temp, sizeof(temp));
			if (n < 0)
				goto bail3;
			if (!n)
				break;

			n = lws_cose_validate_chunk(cps, temp, (size_t)n, NULL);
			if (n && n != LECP_CONTINUE) {
				lwsl_err("%s: chunk validation failed: %d\n",
						__func__, n);
				goto bail2;
			}
		} while (1);
	}

bail3:

	result = 0;

	if (!sign) {
		char buf[2048];
		int os;

		o = lws_cose_validate_results(cps);
		if (!o)
			result = 1;
		else {
			os = lws_snprintf(buf, sizeof(buf),
					  "\nresults count %d\n", o->count);
			write(fdout, buf, (size_t)os);

			if (!o->count)
				result = 1;
		}

		lws_start_foreach_dll_safe(struct lws_dll2 *, p, tp,
					   lws_dll2_get_head(o)) {
			lws_cose_validate_res_t *res = lws_container_of(p,
						lws_cose_validate_res_t, list);
			char khr[256];

			khr[0] = '\0';
			if (res->cose_key) {
				ke = &res->cose_key->meta[COSEKEY_META_KID];
				if (ke && ke->buf)
					lws_hex_from_byte_array(ke->buf, ke->len,
							khr, sizeof(khr));
			}
			os = lws_snprintf(buf, sizeof(buf),
				    " result: %d (alg %s, kid %s)\n",
				    res->result,
				    lws_cose_alg_to_name(res->cose_alg), khr);
			write(fdout, buf, (size_t)os);
			result |= res->result;
		} lws_end_foreach_dll_safe(p, tp);
	}

bail2:
	if (!sign)
		lws_cose_validate_destroy(&cps);
	else {
		lws_buflist_destroy_all_segments(&paybuf);
		lws_cose_sign_destroy(&csc);
	}
//bail1:
	lws_cose_key_set_destroy(&set);
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
