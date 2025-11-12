/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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

#include "private-lib-core.h"
#include "private-lib-tls-mbedtls.h"

#if defined(LWS_WITH_TLS_JIT_TRUST)

/*
 * We get called for each peer certificate that was provided in turn.
 *
 * Our job is just to collect the AKID and SKIDs into ssl->kid_chain, and walk
 * later at verification result time if it failed.
 *
 * None of these should be trusted, even if a misconfigured server sends us
 * his root CA.
 */

static int
lws_mbedtls_client_verify_callback(SSL *ssl, mbedtls_x509_crt *x509)
{
	union lws_tls_cert_info_results ci;

	/* we reached the max we can hold? */

	if (ssl->kid_chain.count == LWS_ARRAY_SIZE(ssl->kid_chain.akid))
		return 0;

	/* if not, stash the SKID and AKID into the next kid slot */

	if (!lws_tls_mbedtls_cert_info(x509, LWS_TLS_CERT_INFO_SUBJECT_KEY_ID,
				       &ci, 0))
		lws_tls_kid_copy(&ci,
				 &ssl->kid_chain.skid[ssl->kid_chain.count]);

	if (!lws_tls_mbedtls_cert_info(x509, LWS_TLS_CERT_INFO_AUTHORITY_KEY_ID,
				       &ci, 0))
		lws_tls_kid_copy(&ci,
				 &ssl->kid_chain.akid[ssl->kid_chain.count]);

	ssl->kid_chain.count++;

	// lwsl_notice("%s: %u\n", __func__, ssl->kid_chain.count);

	return 0;
}

#endif

int
lws_ssl_client_bio_create(struct lws *wsi)
{
	char hostname[128], *p;
	const char *alpn_comma = wsi->a.context->tls.alpn_default;
	struct alpn_ctx protos;
	int fl = SSL_VERIFY_PEER;

	if (wsi->stash)
		lws_strncpy(hostname, wsi->stash->cis[CIS_HOST], sizeof(hostname));
	else
		if (lws_hdr_copy(wsi, hostname, sizeof(hostname),
				_WSI_TOKEN_CLIENT_HOST) <= 0) {
			lwsl_err("%s: Unable to get hostname\n", __func__);

			return -1;
		}

	/*
	 * remove any :port part on the hostname... necessary for network
	 * connection but typical certificates do not contain it
	 */
	p = hostname;
	while (*p) {
		if (*p == ':') {
			*p = '\0';
			break;
		}
		p++;
	}

	wsi->tls.ssl = SSL_new(wsi->a.vhost->tls.ssl_client_ctx);
	if (!wsi->tls.ssl) {
		lwsl_info("%s: SSL_new() failed\n", __func__);
		return -1;
	}

#if defined(LWS_WITH_TLS_SESSIONS)
	if (!(wsi->a.vhost->options & LWS_SERVER_OPTION_DISABLE_TLS_SESSION_CACHE))
		lws_tls_reuse_session(wsi);
#endif

	if (wsi->a.vhost->tls.ssl_info_event_mask)
		SSL_set_info_callback(wsi->tls.ssl, lws_ssl_info_callback);

	if (!(wsi->tls.use_ssl & LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK)) {
		X509_VERIFY_PARAM *param = SSL_get0_param(wsi->tls.ssl);
		/* Enable automatic hostname checks */
	//	X509_VERIFY_PARAM_set_hostflags(param,
	//				X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
		lwsl_info("%s: setting hostname %s\n", __func__, hostname);
		if (X509_VERIFY_PARAM_set1_host(param, hostname, 0) != 1)
			return -1;
	}

	if (wsi->a.vhost->tls.alpn)
		alpn_comma = wsi->a.vhost->tls.alpn;

	if (wsi->stash) {
		lws_strncpy(hostname, wsi->stash->cis[CIS_HOST],
				sizeof(hostname));
		if (wsi->stash->cis[CIS_ALPN])
			alpn_comma = wsi->stash->cis[CIS_ALPN];
	} else {
		if (lws_hdr_copy(wsi, hostname, sizeof(hostname),
				_WSI_TOKEN_CLIENT_ALPN) > 0)
			alpn_comma = hostname;
	}

	protos.len = (uint8_t)lws_alpn_comma_to_openssl(alpn_comma, protos.data,
					       sizeof(protos.data) - 1);

	lwsl_info("%s: %s: client conn sending ALPN list '%s' (protos.len %d)\n",
		  __func__, lws_wsi_tag(wsi), alpn_comma, protos.len);

	/* with mbedtls, protos is not pointed to after exit from this call */
	SSL_set_alpn_select_cb(wsi->tls.ssl, &protos);

	if (wsi->flags & LCCSCF_ALLOW_SELFSIGNED) {
		lwsl_notice("%s: allowing selfsigned\n", __func__);
		fl = SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
	}

	if (wsi->flags & LCCSCF_ALLOW_INSECURE)
		fl = SSL_VERIFY_NONE;

	/*
	 * use server name indication (SNI), if supported,
	 * when establishing connection
	 */
#if defined(LWS_WITH_TLS_JIT_TRUST)
	SSL_set_verify(wsi->tls.ssl, SSL_VERIFY_PEER,
			lws_mbedtls_client_verify_callback);
	(void)fl;
#else
	SSL_set_verify(wsi->tls.ssl, fl, NULL);
#endif

	SSL_set_fd(wsi->tls.ssl, (int)wsi->desc.sockfd);

	if (wsi->sys_tls_client_cert) {
		lws_system_blob_t *b = lws_system_get_blob(wsi->a.context,
					LWS_SYSBLOB_TYPE_CLIENT_CERT_DER,
					wsi->sys_tls_client_cert - 1);
		const uint8_t *pem_data = NULL;
		uint8_t *data = NULL;
		lws_filepos_t flen;
		size_t size;
		int err = 0;

		if (!b)
			goto no_client_cert;

		/*
		 * Set up the per-connection client cert
		 */

		size = lws_system_blob_get_size(b);
		if (!size)
			goto no_client_cert;

		if (lws_system_blob_get_single_ptr(b, &pem_data))
			goto no_client_cert;

		if (lws_tls_alloc_pem_to_der_file(wsi->a.context, NULL,
						  (const char *)pem_data, size,
						  &data, &flen))
			goto no_client_cert;
		size = (size_t) flen;

		err = SSL_use_certificate_ASN1(wsi->tls.ssl, data, (int)size);
		lws_free_set_NULL(data);
		if (err != 1)
			goto no_client_cert;

		b = lws_system_get_blob(wsi->a.context,
					LWS_SYSBLOB_TYPE_CLIENT_KEY_DER,
					wsi->sys_tls_client_cert - 1);
		if (!b)
			goto no_client_cert;
		size = lws_system_blob_get_size(b);
		if (!size)
			goto no_client_cert;

		if (lws_system_blob_get_single_ptr(b, &pem_data))
			goto no_client_cert;

		if (lws_tls_alloc_pem_to_der_file(wsi->a.context, NULL,
						  (const char *)pem_data, size,
						  &data, &flen))
			goto no_client_cert;
		size = (size_t) flen;

		err = SSL_use_PrivateKey_ASN1(0, wsi->tls.ssl, data, (int)size);
		lws_free_set_NULL(data);
		if (err != 1)
			goto no_client_cert;

		/* no wrapper api for check key */

		lwsl_notice("%s: set system client cert %u\n", __func__,
				wsi->sys_tls_client_cert - 1);
	}

	return 0;

no_client_cert:
	lwsl_err("%s: unable to set up system client cert %d\n", __func__,
			wsi->sys_tls_client_cert - 1);

	return 1;
}

int ERR_get_error(void)
{
	return 0;
}

enum lws_ssl_capable_status
lws_tls_client_connect(struct lws *wsi, char *errbuf, size_t elen)
{
	int m, n = SSL_connect(wsi->tls.ssl), en;

	if (n == 1) {
		lws_tls_server_conn_alpn(wsi);
#if defined(LWS_WITH_TLS_SESSIONS)
		lws_tls_session_new_mbedtls(wsi);
#endif
		lwsl_info("%s: client connect OK\n", __func__);
		return LWS_SSL_CAPABLE_DONE;
	}

	en = (int)LWS_ERRNO;
	m = SSL_get_error(wsi->tls.ssl, n);

	if (m == SSL_ERROR_WANT_READ || SSL_want_read(wsi->tls.ssl))
		return LWS_SSL_CAPABLE_MORE_SERVICE_READ;

	if (m == SSL_ERROR_WANT_WRITE || SSL_want_write(wsi->tls.ssl))
		return LWS_SSL_CAPABLE_MORE_SERVICE_WRITE;

	if (!n) /* we don't know what he wants, but he says to retry */
		return LWS_SSL_CAPABLE_MORE_SERVICE;

	if (m == SSL_ERROR_SYSCALL && !en && n >= 0) /* otherwise we miss explicit failures and spin
						      * in hs state 17 until timeout... */
		return LWS_SSL_CAPABLE_MORE_SERVICE;

	lws_snprintf(errbuf, elen, "mbedtls connect %d %d %d", n, m, en);

	return LWS_SSL_CAPABLE_ERROR;
}

int
lws_tls_client_confirm_peer_cert(struct lws *wsi, char *ebuf, size_t ebuf_len)
{
	int n;
	unsigned int avoid = 0;
	X509 *peer = SSL_get_peer_certificate(wsi->tls.ssl);
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	const char *type = "";
	char *sb = (char *)&pt->serv_buf[0];

	if (!peer) {
#if defined(LWS_WITH_SYS_METRICS)
		lws_metrics_hist_bump_describe_wsi(wsi, lws_metrics_priv_to_pub(
					wsi->a.context->mth_conn_failures),
						   "tls=\"nocert\"");
#endif
		lwsl_info("peer did not provide cert\n");
		lws_snprintf(ebuf, ebuf_len, "no peer cert");

		return -1;
	}

	n = (int)SSL_get_verify_result(wsi->tls.ssl);
	lwsl_debug("get_verify says %d\n", n);

	switch (n) {
	case X509_V_OK:
		return 0;

	case X509_V_ERR_HOSTNAME_MISMATCH:
		type = "hostname";
		avoid = LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
		break;

	case X509_V_ERR_INVALID_CA:
		type = "invalidca";
		avoid = LCCSCF_ALLOW_SELFSIGNED;
		break;

	case X509_V_ERR_CERT_NOT_YET_VALID:
		type = "notyetvalid";
		avoid = LCCSCF_ALLOW_EXPIRED;
		break;

	case X509_V_ERR_CERT_HAS_EXPIRED:
		type = "expired";
		avoid = LCCSCF_ALLOW_EXPIRED;
		break;
	}

	lwsl_info("%s: cert problem: %s\n", __func__, type);
#if defined(LWS_WITH_SYS_METRICS)
	{
		char buckname[64];
		lws_snprintf(buckname, sizeof(buckname), "tls=\"%s\"", type);
		lws_metrics_hist_bump_describe_wsi(wsi,
		     lws_metrics_priv_to_pub(wsi->a.context->mth_conn_failures),
			      buckname);
	}
#endif
	if (wsi->tls.use_ssl & avoid) {
		lwsl_info("%s: allowing anyway\n", __func__);

		return 0;
	}

#if defined(LWS_WITH_TLS_JIT_TRUST)
	if (n == X509_V_ERR_INVALID_CA)
	    lws_tls_jit_trust_sort_kids(wsi, &wsi->tls.ssl->kid_chain);
#endif
	lws_snprintf(ebuf, ebuf_len,
		"server's cert didn't look good, %s (use_ssl 0x%x) X509_V_ERR = %d: %s\n",
		type, (unsigned int)wsi->tls.use_ssl, n,
		ERR_error_string((unsigned long)n, sb));

	lwsl_info("%s\n", ebuf);

	lws_tls_err_describe_clear();

	return -1;
}

int
lws_tls_client_create_vhost_context(struct lws_vhost *vh,
				    const struct lws_context_creation_info *info,
				    const char *cipher_list,
				    const char *ca_filepath,
				    const void *ca_mem,
				    unsigned int ca_mem_len,
				    const char *cert_filepath,
				    const void *cert_mem,
				    unsigned int cert_mem_len,
				    const char *private_key_filepath,
					const void *key_mem,
					unsigned int key_mem_len
					)
{
	X509 *d2i_X509(X509 **cert, const unsigned char *buffer, long len);
	SSL_METHOD *method = (SSL_METHOD *)TLS_client_method();
	unsigned long error;
	int n;

#if defined(LWS_WITH_TLS_SESSIONS)
	vh->tls_session_cache_max = info->tls_session_cache_max ?
				    info->tls_session_cache_max : 10;
	lws_tls_session_cache(vh, info->tls_session_timeout);
#endif

	if (!method) {
		error = (unsigned long)ERR_get_error();
		lwsl_err("problem creating ssl method %lu: %s\n",
			error, ERR_error_string(error,
				      (char *)vh->context->pt[0].serv_buf));
		return 1;
	}
	/* create context */
	vh->tls.ssl_client_ctx = SSL_CTX_new(method, &vh->context->mcdc);
	if (!vh->tls.ssl_client_ctx) {
		error = (unsigned long)ERR_get_error();
		lwsl_err("problem creating ssl context %lu: %s\n",
			error, ERR_error_string(error,
				      (char *)vh->context->pt[0].serv_buf));
		return 1;
	}

	if (!ca_filepath && (!ca_mem || !ca_mem_len))
		return 0;

	if (ca_filepath) {
#if !defined(LWS_PLAT_OPTEE)
		uint8_t *buf;
		lws_filepos_t len;

		if (alloc_file(vh->context, ca_filepath, &buf, &len)) {
			lwsl_err("Load CA cert file %s failed\n", ca_filepath);
			return 1;
		}
		vh->tls.x509_client_CA = d2i_X509(NULL, buf, (long)len);
		free(buf);

		lwsl_info("Loading vh %s client CA for verification %s\n", vh->name, ca_filepath);
#endif
	} else {
		vh->tls.x509_client_CA = d2i_X509(NULL, (uint8_t*)ca_mem, (long)ca_mem_len);
		lwsl_info("%s: using mem client CA cert %d\n",
			    __func__, ca_mem_len);
	}

	if (!vh->tls.x509_client_CA) {
		lwsl_err("client CA: x509 parse failed\n");
		return 1;
	}

	if (!vh->tls.ssl_ctx)
		SSL_CTX_add_client_CA(vh->tls.ssl_client_ctx, vh->tls.x509_client_CA);
	else
		SSL_CTX_add_client_CA(vh->tls.ssl_ctx, vh->tls.x509_client_CA);

	/* support for client-side certificate authentication */
	if (cert_filepath) {
#if !defined(LWS_PLAT_OPTEE)
		uint8_t *buf;
		lws_filepos_t amount;

		if (lws_tls_use_any_upgrade_check_extant(cert_filepath) !=
				LWS_TLS_EXTANT_YES &&
		    (info->options & LWS_SERVER_OPTION_IGNORE_MISSING_CERT))
			return 0;

		lwsl_notice("%s: doing cert filepath %s\n", __func__,
				cert_filepath);

		if (alloc_file(vh->context, cert_filepath, &buf, &amount))
			return 1;

		buf[amount++] = '\0';

		n = SSL_CTX_use_certificate_ASN1(vh->tls.ssl_client_ctx,
				(int)amount, buf);
		lws_free(buf);
		if (n < 1) {
			lwsl_err("problem %d getting cert '%s'\n", n,
				 cert_filepath);
			lws_tls_err_describe_clear();
			return 1;
		}

		lwsl_info("Loaded client cert %s\n", cert_filepath);
#endif
	} else if (cert_mem && cert_mem_len) {
		/* lwsl_hexdump_notice(cert_mem, cert_mem_len - 1); */
		n = SSL_CTX_use_certificate_ASN1(vh->tls.ssl_client_ctx,
						 (int)cert_mem_len, cert_mem);
		if (n < 1) {
			lwsl_err("%s: (mbedtls) problem interpreting client cert\n",
				 __func__);
			lws_tls_err_describe_clear();
			return 1;
		}
		lwsl_info("%s: using mem client cert %d\n",
			    __func__, cert_mem_len);
	}

	if (private_key_filepath) {
#if !defined(LWS_PLAT_OPTEE)

		uint8_t *buf;
		lws_filepos_t amount;

		lwsl_notice("%s: doing private key filepath %s\n", __func__,
				private_key_filepath);
		if (alloc_file(vh->context, private_key_filepath, &buf, &amount))
			return 1;

		buf[amount++] = '\0';

		n = SSL_CTX_use_PrivateKey_ASN1(0, vh->tls.ssl_client_ctx,
				buf, (long)amount);

		lws_free(buf);
		if (n < 1) {
			lwsl_err("problem %d getting private key '%s'\n", n,
				 private_key_filepath);
			lws_tls_err_describe_clear();
			return 1;
		}

		lwsl_notice("Loaded private key %s\n", private_key_filepath);
#endif
	} else if (key_mem && key_mem_len) {
		/* lwsl_hexdump_notice(cert_mem, cert_mem_len - 1); */
		n = SSL_CTX_use_PrivateKey_ASN1(0, vh->tls.ssl_client_ctx,
				key_mem, (long)key_mem_len - 1);

		if (n < 1) {
			lwsl_err("%s: (mbedtls) problem interpreting private key\n",
				 __func__);
			lws_tls_err_describe_clear();
			return 1;
		}
		lwsl_info("%s: using mem private key %d\n",
			    __func__, key_mem_len);

	}
	return 0;
}

int
lws_tls_client_vhost_extra_cert_mem(struct lws_vhost *vh,
                const uint8_t *der, size_t der_len)
{
	if (SSL_CTX_add_client_CA_ASN1(vh->tls.ssl_client_ctx, (int)der_len, der) != 1) {
		lwsl_err("%s: failed\n", __func__);
			return 1;
	}

	return 0;
}

