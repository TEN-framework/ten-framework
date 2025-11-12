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
 */

#define WIN32_LEAN_AND_MEAN
#include "private-lib-core.h"
#include "private-lib-tls-openssl.h"

#if !defined(LWS_PLAT_OPTEE)
static int
dec(char c)
{
	return c - '0';
}
#endif

static time_t
lws_tls_openssl_asn1time_to_unix(ASN1_TIME *as)
{
#if !defined(LWS_PLAT_OPTEE)

	const char *p = (const char *)as->data;
	struct tm t;

	/* [YY]YYMMDDHHMMSSZ */

	memset(&t, 0, sizeof(t));

	if (strlen(p) == 13) {
		t.tm_year = (dec(p[0]) * 10) + dec(p[1]) + 100;
		p += 2;
	} else {
		t.tm_year = (dec(p[0]) * 1000) + (dec(p[1]) * 100) +
			    (dec(p[2]) * 10) + dec(p[3]);
		p += 4;
	}
	t.tm_mon = (dec(p[0]) * 10) + dec(p[1]) - 1;
	p += 2;
	t.tm_mday = (dec(p[0]) * 10) + dec(p[1]) - 1;
	p += 2;
	t.tm_hour = (dec(p[0]) * 10) + dec(p[1]);
	p += 2;
	t.tm_min = (dec(p[0]) * 10) + dec(p[1]);
	p += 2;
	t.tm_sec = (dec(p[0]) * 10) + dec(p[1]);
	t.tm_isdst = 0;

	return mktime(&t);
#else
	return (time_t)-1;
#endif
}

#if defined(USE_WOLFSSL)
#define AUTHORITY_KEYID WOLFSSL_AUTHORITY_KEYID
#endif

int
lws_tls_openssl_cert_info(X509 *x509, enum lws_tls_cert_info type,
			  union lws_tls_cert_info_results *buf, size_t len)
{
#ifndef USE_WOLFSSL
	const unsigned char *dp;
	ASN1_OCTET_STRING *val;
	AUTHORITY_KEYID *akid;
	X509_EXTENSION *ext;
	int tag, xclass, r = 1;
	long xlen, loc;
#endif
	X509_NAME *xn;
#if !defined(LWS_PLAT_OPTEE)
	char *p;
#endif

	buf->ns.len = 0;

	if (!x509)
		return -1;
	if (!len)
		len = sizeof(buf->ns.name);

#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(X509_get_notBefore)
#define X509_get_notBefore(x)	X509_getm_notBefore(x)
#define X509_get_notAfter(x)	X509_getm_notAfter(x)
#endif

	switch (type) {
	case LWS_TLS_CERT_INFO_VALIDITY_FROM:
		buf->time = lws_tls_openssl_asn1time_to_unix(
					X509_get_notBefore(x509));
		if (buf->time == (time_t)-1)
			return -1;
		break;

	case LWS_TLS_CERT_INFO_VALIDITY_TO:
		buf->time = lws_tls_openssl_asn1time_to_unix(
					X509_get_notAfter(x509));
		if (buf->time == (time_t)-1)
			return -1;
		break;

	case LWS_TLS_CERT_INFO_COMMON_NAME:
#if defined(LWS_PLAT_OPTEE)
		return -1;
#else
		xn = X509_get_subject_name(x509);
		if (!xn)
			return -1;
		X509_NAME_oneline(xn, buf->ns.name, (int)len - 2);
		p = strstr(buf->ns.name, "/CN=");
		if (p)
			memmove(buf->ns.name, p + 4, strlen(p + 4) + 1);
		buf->ns.len = (int)strlen(buf->ns.name);
		return 0;
#endif
	case LWS_TLS_CERT_INFO_ISSUER_NAME:
		xn = X509_get_issuer_name(x509);
		if (!xn)
			return -1;
		X509_NAME_oneline(xn, buf->ns.name, (int)len - 1);
		buf->ns.len = (int)strlen(buf->ns.name);
		return 0;

	case LWS_TLS_CERT_INFO_USAGE:
#if defined(LWS_HAVE_X509_get_key_usage)
		buf->usage = X509_get_key_usage(x509);
		break;
#else
		return -1;
#endif

	case LWS_TLS_CERT_INFO_OPAQUE_PUBLIC_KEY:
	{
#ifndef USE_WOLFSSL
		size_t klen = (unsigned int)i2d_X509_PUBKEY(X509_get_X509_PUBKEY(x509), NULL);
		uint8_t *tmp, *ptmp;

		if (!klen || klen > len)
			return -1;

		tmp = (uint8_t *)OPENSSL_malloc(klen);
		if (!tmp)
			return -1;

		ptmp = tmp;
		if (i2d_X509_PUBKEY(
			      X509_get_X509_PUBKEY(x509), &ptmp) != (int)klen ||
		    !ptmp || lws_ptr_diff(ptmp, tmp) != (int)klen) {
			lwsl_info("%s: cert public key extraction failed\n",
				  __func__);
			if (ptmp)
				OPENSSL_free(tmp);

			return -1;
		}

		buf->ns.len = (int)klen;
		memcpy(buf->ns.name, tmp, klen);
		OPENSSL_free(tmp);
#endif
		return 0;
	}
	case LWS_TLS_CERT_INFO_DER_RAW:
	{
		int der_len = i2d_X509(x509, NULL);
		uint8_t *tmp = (uint8_t *)buf->ns.name;

		buf->ns.len = der_len < 0 ? 0 : der_len;

		if (der_len < 0 || (size_t)der_len > len)
			return -1;

		der_len = i2d_X509(x509, &tmp);
		if (der_len < 0)
			return -1;

		return 0;
	}

#ifndef USE_WOLFSSL

	case LWS_TLS_CERT_INFO_AUTHORITY_KEY_ID:
		loc = X509_get_ext_by_NID(x509, NID_authority_key_identifier, -1);
		if (loc < 0)
			return 1;

		ext = X509_get_ext(x509, (int)loc);
		if (!ext)
			return 1;
#ifndef USE_WOLFSSL
		akid = (AUTHORITY_KEYID *)X509V3_EXT_d2i(ext);
#else
		akid = (AUTHORITY_KEYID *)wolfSSL_X509V3_EXT_d2i(ext);
#endif
		if (!akid || !akid->keyid)
			return 1;
		val = akid->keyid;
		dp = (const unsigned char *)val->data;
		xlen = val->length;

		buf->ns.len = (int)xlen;
		if (len < (size_t)buf->ns.len)
			return -1;

		memcpy(buf->ns.name, dp, (size_t)buf->ns.len);

		AUTHORITY_KEYID_free(akid);
		break;

	case LWS_TLS_CERT_INFO_AUTHORITY_KEY_ID_ISSUER:
		loc = X509_get_ext_by_NID(x509, NID_authority_key_identifier, -1);
		if (loc < 0)
			return 1;

		ext = X509_get_ext(x509, (int)loc);
		if (!ext)
			return 1;

#ifndef USE_WOLFSSL
		akid = (AUTHORITY_KEYID *)X509V3_EXT_d2i(ext);
#else
		akid = (AUTHORITY_KEYID *)wolfSSL_X509V3_EXT_d2i(ext);
#endif
		if (!akid || !akid->issuer)
			return 1;

#if defined(LWS_HAVE_OPENSSL_STACK)
		{
			const X509V3_EXT_METHOD* method = X509V3_EXT_get(ext);
			STACK_OF(CONF_VALUE) *cv;
			int j;

			cv = i2v_GENERAL_NAMES((X509V3_EXT_METHOD*)method, akid->issuer, NULL);
			if (!cv)
				goto bail_ak;

		        for (j = 0; j < OPENSSL_sk_num((const OPENSSL_STACK *)&cv); j++) {
		            CONF_VALUE *nval = OPENSSL_sk_value((const OPENSSL_STACK *)&cv, j);
		            size_t ln = (nval->name ? strlen(nval->name) : 0),
		        	   lv = (nval->value ? strlen(nval->value) : 0),
		        	   l = ln + lv;

		            if (len > l) {
		        	    if (nval->name)
		        		    memcpy(buf->ns.name + buf->ns.len, nval->name, ln);
		        	    if (nval->value)
		        		    memcpy(buf->ns.name + buf->ns.len + ln, nval->value, lv);
		        	    buf->ns.len = (int)((size_t)buf->ns.len + l);
		        	    len -= l;
		        	    buf->ns.name[buf->ns.len] = '\0';

		        	    r = 0;
		            }
		        }
		}

bail_ak:
#endif
		AUTHORITY_KEYID_free(akid);

		return r;

	case LWS_TLS_CERT_INFO_AUTHORITY_KEY_ID_SERIAL:
		loc = X509_get_ext_by_NID(x509, NID_authority_key_identifier, -1);
		if (loc < 0)
			return 1;

		ext = X509_get_ext(x509, (int)loc);
		if (!ext)
			return 1;
		akid = (AUTHORITY_KEYID *)X509V3_EXT_d2i(ext);
		if (!akid || !akid->serial)
			return 1;

#if 0
		// need to handle blobs, and ASN1_INTEGER_get_uint64 not
		// available on older openssl
		{
			uint64_t res;
			if (ASN1_INTEGER_get_uint64(&res, akid->serial) != 1)
				break;
			buf->ns.len = lws_snprintf(buf->ns.name, len, "%llu",
					(unsigned long long)res);
		}
#endif
		break;

	case LWS_TLS_CERT_INFO_SUBJECT_KEY_ID:

		loc = X509_get_ext_by_NID(x509, NID_subject_key_identifier, -1);
		if (loc < 0)
			return 1;

		ext = X509_get_ext(x509, (int)loc);
		if (!ext)
			return 1;

		val = X509_EXTENSION_get_data(ext);
		if (!val)
			return 1;

#if defined(USE_WOLFSSL)
		return 1;
#else
		dp = (const unsigned char *)val->data;

		if (ASN1_get_object(&dp, &xlen,
				    &tag, &xclass, val->length) & 0x80)
			return -1;

		if (tag != V_ASN1_OCTET_STRING) {
			lwsl_notice("not octet string %d\n", (int)tag);
			return 1;
		}
#endif
		buf->ns.len = (int)xlen;
		if (len < (size_t)buf->ns.len)
			return -1;

		memcpy(buf->ns.name, dp, (size_t)buf->ns.len);
		break;
#endif

	default:
		return -1;
	}

	return 0;
}

int
lws_x509_info(struct lws_x509_cert *x509, enum lws_tls_cert_info type,
	      union lws_tls_cert_info_results *buf, size_t len)
{
	return lws_tls_openssl_cert_info(x509->cert, type, buf, len);
}

#if defined(LWS_WITH_NETWORK)
int
lws_tls_vhost_cert_info(struct lws_vhost *vhost, enum lws_tls_cert_info type,
		        union lws_tls_cert_info_results *buf, size_t len)
{
#if defined(LWS_HAVE_SSL_CTX_get0_certificate)
	X509 *x509 = SSL_CTX_get0_certificate(vhost->tls.ssl_ctx);

	return lws_tls_openssl_cert_info(x509, type, buf, len);
#else
	lwsl_notice("openssl is too old to support %s\n", __func__);

	return -1;
#endif
}



int
lws_tls_peer_cert_info(struct lws *wsi, enum lws_tls_cert_info type,
		       union lws_tls_cert_info_results *buf, size_t len)
{
	int rc = 0;
	X509 *x509;

	wsi = lws_get_network_wsi(wsi);

	x509 = SSL_get_peer_certificate(wsi->tls.ssl);

	if (!x509) {
		lwsl_debug("no peer cert\n");

		return -1;
	}

	switch (type) {
	case LWS_TLS_CERT_INFO_VERIFIED:
		buf->verified = SSL_get_verify_result(wsi->tls.ssl) ==
					X509_V_OK;
		break;
	default:
		rc = lws_tls_openssl_cert_info(x509, type, buf, len);
	}

	X509_free(x509);

	return rc;
}
#endif

int
lws_x509_create(struct lws_x509_cert **x509)
{
	*x509 = lws_malloc(sizeof(**x509), __func__);
	if (*x509)
		(*x509)->cert = NULL;

	return !(*x509);
}

int
lws_x509_parse_from_pem(struct lws_x509_cert *x509, const void *pem, size_t len)
{
	BIO* bio = BIO_new(BIO_s_mem());

	BIO_write(bio, pem, (int)len);
	x509->cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	BIO_free(bio);
	if (!x509->cert) {
		lwsl_err("%s: unable to parse PEM cert\n", __func__);
		lws_tls_err_describe_clear();

		return -1;
	}

	return 0;
}

int
lws_x509_verify(struct lws_x509_cert *x509, struct lws_x509_cert *trusted,
		const char *common_name)
{
	char c[32], *p;
	int ret;

	if (common_name) {
		X509_NAME *xn = X509_get_subject_name(x509->cert);
		if (!xn)
			return -1;
		X509_NAME_oneline(xn, c, (int)sizeof(c) - 2);
		p = strstr(c, "/CN=");
		if (p)
			p = p + 4;
		else
			p = c;

		if (strcmp(p, common_name)) {
			lwsl_err("%s: common name mismatch\n", __func__);
			return -1;
		}
	}

	ret = X509_check_issued(trusted->cert, x509->cert);
	if (ret != X509_V_OK) {
		lwsl_err("%s: unable to verify cert relationship\n", __func__);
		lws_tls_err_describe_clear();

		return -1;
	}

	return 0;
}

#if defined(LWS_WITH_JOSE)
int
lws_x509_public_to_jwk(struct lws_jwk *jwk, struct lws_x509_cert *x509,
		       const char *curves, int rsa_min_bits)
{
	int id, n, ret = -1, count;
	ASN1_OBJECT *obj = NULL;
	const EC_POINT *ecpoint;
	const EC_GROUP *ecgroup;
	EC_KEY *ecpub = NULL;
	X509_PUBKEY *pubkey;
	RSA *rsapub = NULL;
	BIGNUM *mpi[4];
	EVP_PKEY *pkey;

	memset(jwk, 0, sizeof(*jwk));

	pubkey = X509_get_X509_PUBKEY(x509->cert);
	if (!pubkey) {
		lwsl_err("%s: missing pubkey alg in cert\n", __func__);

		goto bail;
	}

	if (X509_PUBKEY_get0_param(&obj, NULL, NULL, NULL, pubkey) != 1) {
		lwsl_err("%s: missing pubkey alg in cert\n", __func__);

		goto bail;
	}

	id = OBJ_obj2nid(obj);
	if (id == NID_undef) {
		lwsl_err("%s: missing pubkey alg in cert\n", __func__);

		goto bail;
	}

	lwsl_debug("%s: key type %d \"%s\"\n", __func__, id, OBJ_nid2ln(id));

	pkey = X509_get_pubkey(x509->cert);
	if (!pkey) {
		lwsl_notice("%s: unable to extract pubkey", __func__);

		goto bail;
	}

	switch (id) {
	case NID_X9_62_id_ecPublicKey:
		lwsl_debug("%s: EC key\n", __func__);
		jwk->kty = LWS_GENCRYPTO_KTY_EC;

		if (!curves) {
			lwsl_err("%s: ec curves not allowed\n", __func__);

			goto bail1;
		}

		ecpub = EVP_PKEY_get1_EC_KEY(pkey);
		if (!ecpub) {
			lwsl_notice("%s: missing EC pubkey\n", __func__);

			goto bail1;
		}

		ecpoint = EC_KEY_get0_public_key(ecpub);
		if (!ecpoint) {
			lwsl_err("%s: EC_KEY_get0_public_key failed\n", __func__);
			goto bail2;
		}

		ecgroup = EC_KEY_get0_group(ecpub);
		if (!ecgroup) {
			lwsl_err("%s: EC_KEY_get0_group failed\n", __func__);
			goto bail2;
		}

		/* validate the curve against ones we allow */

		if (lws_genec_confirm_curve_allowed_by_tls_id(curves,
				EC_GROUP_get_curve_name(ecgroup), jwk))
			/* already logged */
			goto bail2;

		mpi[LWS_GENCRYPTO_EC_KEYEL_CRV] = NULL;
		mpi[LWS_GENCRYPTO_EC_KEYEL_X] = BN_new(); /* X */
		mpi[LWS_GENCRYPTO_EC_KEYEL_D] = NULL;
		mpi[LWS_GENCRYPTO_EC_KEYEL_Y] = BN_new(); /* Y */

#if defined(LWS_HAVE_EC_POINT_get_affine_coordinates)
		if (EC_POINT_get_affine_coordinates(ecgroup, ecpoint,
#else
		if (EC_POINT_get_affine_coordinates_GFp(ecgroup, ecpoint,
#endif
						  mpi[LWS_GENCRYPTO_EC_KEYEL_X],
						  mpi[LWS_GENCRYPTO_EC_KEYEL_Y],
							  NULL) != 1) {
			BN_clear_free(mpi[LWS_GENCRYPTO_EC_KEYEL_X]);
			BN_clear_free(mpi[LWS_GENCRYPTO_EC_KEYEL_Y]);
			lwsl_err("%s: EC_POINT_get_aff failed\n", __func__);
			goto bail2;
		}
		count = LWS_GENCRYPTO_EC_KEYEL_COUNT;
		n = LWS_GENCRYPTO_EC_KEYEL_X;
		break;

	case NID_rsaEncryption:
		lwsl_debug("%s: rsa key\n", __func__);
		jwk->kty = LWS_GENCRYPTO_KTY_RSA;

		rsapub = EVP_PKEY_get1_RSA(pkey);
		if (!rsapub) {
			lwsl_notice("%s: missing RSA pubkey\n", __func__);

			goto bail1;
		}

		if ((size_t)RSA_size(rsapub) * 8 < (size_t)rsa_min_bits) {
			lwsl_err("%s: key bits %d less than minimum %d\n",
				 __func__, RSA_size(rsapub) * 8, rsa_min_bits);

			goto bail2;
		}

#if defined(LWS_HAVE_RSA_SET0_KEY)
		/* we don't need d... but the api wants to write it */
		RSA_get0_key(rsapub,
			    (const BIGNUM **)&mpi[LWS_GENCRYPTO_RSA_KEYEL_N],
			    (const BIGNUM **)&mpi[LWS_GENCRYPTO_RSA_KEYEL_E],
			    (const BIGNUM **)&mpi[LWS_GENCRYPTO_RSA_KEYEL_D]);
#else
		mpi[LWS_GENCRYPTO_RSA_KEYEL_E] = rsapub->e;
		mpi[LWS_GENCRYPTO_RSA_KEYEL_N] = rsapub->n;
		mpi[LWS_GENCRYPTO_RSA_KEYEL_D] = NULL;
#endif
		count = LWS_GENCRYPTO_RSA_KEYEL_D;
		n = LWS_GENCRYPTO_RSA_KEYEL_E;
		break;
	default:
		lwsl_err("%s: unknown NID\n", __func__);
		goto bail2;
	}

	for (; n < count; n++) {
		if (!mpi[n])
			continue;
		jwk->e[n].len = (unsigned int)BN_num_bytes(mpi[n]);
		jwk->e[n].buf = lws_malloc(jwk->e[n].len, "certkeyimp");
		if (!jwk->e[n].buf) {
			if (id == NID_X9_62_id_ecPublicKey) {
				BN_clear_free(mpi[LWS_GENCRYPTO_EC_KEYEL_X]);
				BN_clear_free(mpi[LWS_GENCRYPTO_EC_KEYEL_Y]);
			}
			goto bail2;
		}
		BN_bn2bin(mpi[n], jwk->e[n].buf);
	}

	if (id == NID_X9_62_id_ecPublicKey) {
		BN_clear_free(mpi[LWS_GENCRYPTO_EC_KEYEL_X]);
		BN_clear_free(mpi[LWS_GENCRYPTO_EC_KEYEL_Y]);
	}

	ret = 0;

bail2:
	if (id == NID_X9_62_id_ecPublicKey)
		EC_KEY_free(ecpub);
	else
		RSA_free(rsapub);

bail1:
	EVP_PKEY_free(pkey);
bail:
	/* jwk destroy will clean any partial state */
	if (ret)
		lws_jwk_destroy(jwk);

	return ret;
}

static int
lws_x509_jwk_privkey_pem_pp_cb(char *buf, int size, int rwflag, void *u)
{
	const char *pp = (const char *)u;
	size_t n = strlen(pp);

	if ((int)n > size - 1)
		return -1;

	memcpy(buf, pp, n + 1);

	return (int)n;
}

int
lws_x509_jwk_privkey_pem(struct lws_context *cx, struct lws_jwk *jwk,
			 void *pem, size_t len, const char *passphrase)
{
	BIO* bio = BIO_new(BIO_s_mem());
	BIGNUM *mpi, *dummy[6];
	EVP_PKEY *pkey = NULL;
	EC_KEY *ecpriv = NULL;
	RSA *rsapriv = NULL;
	const BIGNUM *cmpi;
	int n, m, ret = -1;

	BIO_write(bio, pem, (int)len);
	PEM_read_bio_PrivateKey(bio, &pkey, lws_x509_jwk_privkey_pem_pp_cb,
				(void *)passphrase);
	BIO_free(bio);
	lws_explicit_bzero((void *)pem, len);
	if (!pkey) {
		lwsl_err("%s: unable to parse PEM privkey\n", __func__);
		lws_tls_err_describe_clear();

		return -1;
	}

	/* confirm the key type matches the existing jwk situation */

	switch (jwk->kty) {
	case LWS_GENCRYPTO_KTY_EC:
		if (EVP_PKEY_type(EVP_PKEY_id(pkey)) != EVP_PKEY_EC) {
			lwsl_err("%s: jwk is EC but privkey isn't\n", __func__);

			goto bail;
		}
		ecpriv = EVP_PKEY_get1_EC_KEY(pkey);
		if (!ecpriv) {
			lwsl_notice("%s: missing EC key\n", __func__);

			goto bail;
		}

		cmpi = EC_KEY_get0_private_key(ecpriv);

		/* quick size check first */

		n = BN_num_bytes(cmpi);
		if (jwk->e[LWS_GENCRYPTO_EC_KEYEL_Y].len != (uint32_t)n) {
			lwsl_err("%s: jwk key size doesn't match\n", __func__);

			goto bail1;
		}

		/* TODO.. check public curve / group + point */

		jwk->e[LWS_GENCRYPTO_EC_KEYEL_D].len = (unsigned int)n;
		jwk->e[LWS_GENCRYPTO_EC_KEYEL_D].buf = lws_malloc((unsigned int)n, "ec");
		if (!jwk->e[LWS_GENCRYPTO_EC_KEYEL_D].buf)
			goto bail1;

		m = BN_bn2binpad(cmpi, jwk->e[LWS_GENCRYPTO_EC_KEYEL_D].buf,
				      (int32_t)jwk->e[LWS_GENCRYPTO_EC_KEYEL_D].len);
		if ((unsigned int)m != (unsigned int)BN_num_bytes(cmpi))
			goto bail1;

		break;

	case LWS_GENCRYPTO_KTY_RSA:
		if (EVP_PKEY_type(EVP_PKEY_id(pkey)) != EVP_PKEY_RSA) {
			lwsl_err("%s: RSA jwk, non-RSA privkey\n", __func__);

			goto bail;
		}
		rsapriv = EVP_PKEY_get1_RSA(pkey);
		if (!rsapriv) {
			lwsl_notice("%s: missing RSA key\n", __func__);

			goto bail;
		}

#if defined(LWS_HAVE_RSA_SET0_KEY) && !defined(USE_WOLFSSL)
		RSA_get0_key(rsapriv, (const BIGNUM **)&dummy[0], /* n */
				      (const BIGNUM **)&dummy[1], /* e */
				      (const BIGNUM **)&mpi);	  /* d */
		RSA_get0_factors(rsapriv, (const BIGNUM **)&dummy[4],  /* p */
					  (const BIGNUM **)&dummy[5]); /* q */
#else
		dummy[0] = rsapriv->n;
		dummy[1] = rsapriv->e;
		dummy[4] = rsapriv->p;
		dummy[5] = rsapriv->q;
		mpi = rsapriv->d;
#endif

		/* quick size check first */

		n = BN_num_bytes(mpi);
		if (jwk->e[LWS_GENCRYPTO_RSA_KEYEL_N].len != (uint32_t)n) {
			lwsl_err("%s: jwk key size doesn't match\n", __func__);

			goto bail1;
		}

		/* then check that n & e match what we got from the cert */

		dummy[2] = BN_bin2bn(jwk->e[LWS_GENCRYPTO_RSA_KEYEL_N].buf,
				     (int32_t)jwk->e[LWS_GENCRYPTO_RSA_KEYEL_N].len,
				     NULL);
		dummy[3] = BN_bin2bn(jwk->e[LWS_GENCRYPTO_RSA_KEYEL_E].buf,
				     (int32_t)jwk->e[LWS_GENCRYPTO_RSA_KEYEL_E].len,
				     NULL);

		m = BN_cmp(dummy[2], dummy[0]) | BN_cmp(dummy[3], dummy[1]);
		BN_clear_free(dummy[2]);
		BN_clear_free(dummy[3]);
		if (m) {
			lwsl_err("%s: privkey doesn't match jwk pubkey\n",
				 __func__);

			goto bail1;
		}

		/* accept d from the PEM privkey into the JWK */

		jwk->e[LWS_GENCRYPTO_RSA_KEYEL_D].len = (unsigned int)n;
		jwk->e[LWS_GENCRYPTO_RSA_KEYEL_D].buf = lws_malloc((unsigned int)n, "privjk");
		if (!jwk->e[LWS_GENCRYPTO_RSA_KEYEL_D].buf)
			goto bail1;

		BN_bn2bin(mpi, jwk->e[LWS_GENCRYPTO_RSA_KEYEL_D].buf);

		/* accept p and q from the PEM privkey into the JWK */

		jwk->e[LWS_GENCRYPTO_RSA_KEYEL_P].len = (unsigned int)BN_num_bytes(dummy[4]);
		jwk->e[LWS_GENCRYPTO_RSA_KEYEL_P].buf = lws_malloc((unsigned int)n, "privjk");
		if (!jwk->e[LWS_GENCRYPTO_RSA_KEYEL_P].buf) {
			lws_free_set_NULL(jwk->e[LWS_GENCRYPTO_RSA_KEYEL_D].buf);
			goto bail1;
		}
		BN_bn2bin(dummy[4], jwk->e[LWS_GENCRYPTO_RSA_KEYEL_P].buf);

		jwk->e[LWS_GENCRYPTO_RSA_KEYEL_Q].len = (unsigned int)BN_num_bytes(dummy[5]);
		jwk->e[LWS_GENCRYPTO_RSA_KEYEL_Q].buf = lws_malloc((unsigned int)n, "privjk");
		if (!jwk->e[LWS_GENCRYPTO_RSA_KEYEL_Q].buf) {
			lws_free_set_NULL(jwk->e[LWS_GENCRYPTO_RSA_KEYEL_D].buf);
			lws_free_set_NULL(jwk->e[LWS_GENCRYPTO_RSA_KEYEL_P].buf);
			goto bail1;
		}
		BN_bn2bin(dummy[5], jwk->e[LWS_GENCRYPTO_RSA_KEYEL_Q].buf);
		break;
	default:
		lwsl_err("%s: JWK has unknown kty %d\n", __func__, jwk->kty);
		return -1;
	}

	ret = 0;

bail1:
	if (jwk->kty == LWS_GENCRYPTO_KTY_EC)
		EC_KEY_free(ecpriv);
	else
		RSA_free(rsapriv);

bail:
	EVP_PKEY_free(pkey);

	return ret;
}
#endif

void
lws_x509_destroy(struct lws_x509_cert **x509)
{
	if (!*x509)
		return;

	if ((*x509)->cert) {
		X509_free((*x509)->cert);
		(*x509)->cert = NULL;
	}

	lws_free_set_NULL(*x509);
}
