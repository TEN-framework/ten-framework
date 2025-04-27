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

#include "private-lib-core.h"

/*
 * Care: many openssl apis return 1 for success.  These are translated to the
 * lws convention of 0 for success.
 */

extern int openssl_websocket_private_data_index,
	   openssl_SSL_CTX_private_data_index;

int lws_openssl_describe_cipher(struct lws *wsi);

static int
OpenSSL_verify_callback(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
	SSL *ssl;
	int n;
	struct lws *wsi;
	union lws_tls_cert_info_results ir;
	X509 *topcert = X509_STORE_CTX_get_current_cert(x509_ctx);

	ssl = X509_STORE_CTX_get_ex_data(x509_ctx,
		SSL_get_ex_data_X509_STORE_CTX_idx());

	/*
	 * !!! nasty openssl requires the index to come as a library-scope
	 * static
	 */
	wsi = SSL_get_ex_data(ssl, openssl_websocket_private_data_index);

	n = lws_tls_openssl_cert_info(topcert, LWS_TLS_CERT_INFO_COMMON_NAME,
				      &ir, sizeof(ir.ns.name));
	if (!n)
		lwsl_info("%s: client cert CN '%s'\n", __func__, ir.ns.name);
	else
		lwsl_info("%s: couldn't get client cert CN\n", __func__);

	n = wsi->a.vhost->protocols[0].callback(wsi,
			LWS_CALLBACK_OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION,
					   x509_ctx, ssl, (unsigned int)preverify_ok);

	/* convert return code from 0 = OK to 1 = OK */
	return !n;
}

int
lws_tls_server_client_cert_verify_config(struct lws_vhost *vh)
{
	int verify_options = SSL_VERIFY_PEER;

	/* as a server, are we requiring clients to identify themselves? */

	if (!lws_check_opt(vh->options,
			  LWS_SERVER_OPTION_REQUIRE_VALID_OPENSSL_CLIENT_CERT))
		return 0;

	if (!lws_check_opt(vh->options,
			   LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED))
		verify_options |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;

	SSL_CTX_set_session_id_context(vh->tls.ssl_ctx, (uint8_t *)vh->context,
				       sizeof(void *));

	/* absolutely require the client cert */
	SSL_CTX_set_verify(vh->tls.ssl_ctx, verify_options,
			   OpenSSL_verify_callback);

	return 0;
}

#if defined(SSL_TLSEXT_ERR_NOACK) && !defined(OPENSSL_NO_TLSEXT)
static int
lws_ssl_server_name_cb(SSL *ssl, int *ad, void *arg)
{
	struct lws_context *context = (struct lws_context *)arg;
	struct lws_vhost *vhost, *vh;
	const char *servername;

	if (!ssl)
		return SSL_TLSEXT_ERR_NOACK;

	/*
	 * We can only get ssl accepted connections by using a vhost's ssl_ctx
	 * find out which listening one took us and only match vhosts on the
	 * same port.
	 */
	vh = context->vhost_list;
	while (vh) {
		if (!vh->being_destroyed &&
		    vh->tls.ssl_ctx == SSL_get_SSL_CTX(ssl))
			break;
		vh = vh->vhost_next;
	}

	if (!vh) {
		assert(vh); /* can't match the incoming vh? */
		return SSL_TLSEXT_ERR_OK;
	}

	servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	if (!servername) {
		/* the client doesn't know what hostname it wants */
		lwsl_info("SNI: Unknown ServerName\n");

		return SSL_TLSEXT_ERR_OK;
	}

	vhost = lws_select_vhost(context, vh->listen_port, servername);
	if (!vhost) {
		lwsl_info("SNI: none: %s:%d\n", servername, vh->listen_port);

		return SSL_TLSEXT_ERR_OK;
	}

	lwsl_info("SNI: Found: %s:%d\n", servername, vh->listen_port);

	/* select the ssl ctx from the selected vhost for this conn */
	SSL_set_SSL_CTX(ssl, vhost->tls.ssl_ctx);

	return SSL_TLSEXT_ERR_OK;
}
#endif

/*
 * this may now get called after the vhost creation, when certs become
 * available.
 */
int
lws_tls_server_certs_load(struct lws_vhost *vhost, struct lws *wsi,
			  const char *cert, const char *private_key,
			  const char *mem_cert, size_t mem_cert_len,
			  const char *mem_privkey, size_t mem_privkey_len)
{
#if !defined(OPENSSL_NO_EC) && defined(LWS_HAVE_EC_KEY_new_by_curve_name) && \
    ((OPENSSL_VERSION_NUMBER < 0x30000000l) || \
     defined(LWS_SUPPRESS_DEPRECATED_API_WARNINGS))
	const char *ecdh_curve = "prime256v1";
#if !defined(LWS_WITH_BORINGSSL) && defined(LWS_HAVE_SSL_EXTRA_CHAIN_CERTS)
	STACK_OF(X509) *extra_certs = NULL;
#endif
	EC_KEY *ecdh, *EC_key = NULL;
	EVP_PKEY *pkey;
	X509 *x = NULL;
	int ecdh_nid;
	int KeyType;
#endif
	unsigned long error;
	lws_filepos_t flen;
	uint8_t *p;
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	int ret;
#endif
	int n = (int)lws_tls_generic_cert_checks(vhost, cert, private_key), m;

	if (!cert && !private_key)
		n = LWS_TLS_EXTANT_ALTERNATIVE;

	if (n == LWS_TLS_EXTANT_NO && (!mem_cert || !mem_privkey))
		return 0;
	if (n == LWS_TLS_EXTANT_NO)
		n = LWS_TLS_EXTANT_ALTERNATIVE;

	if (n == LWS_TLS_EXTANT_ALTERNATIVE && (!mem_cert || !mem_privkey))
		return 1; /* no alternative */

	if (n == LWS_TLS_EXTANT_ALTERNATIVE) {

#if OPENSSL_VERSION_NUMBER >= 0x10100000L

		/*
		 * Although we have prepared update certs, we no longer have
		 * the rights to read our own cert + key we saved.
		 *
		 * If we were passed copies in memory buffers, use those
		 * in favour of the filepaths we normally want.
		 */
		cert = NULL;
		private_key = NULL;
	}

	/*
	 * use the multi-cert interface for backwards compatibility in the
	 * both simple files case
	 */

	if (n != LWS_TLS_EXTANT_ALTERNATIVE && cert) {

		/* set the local certificate from CertFile */
		m = SSL_CTX_use_certificate_chain_file(vhost->tls.ssl_ctx, cert);
		if (m != 1) {
			const char *s;
			error = ERR_get_error();

			s = ERR_error_string(
#if defined(LWS_WITH_BORINGSSL)
				(uint32_t)
#endif
					error,
				       (char *)vhost->context->pt[0].serv_buf);

			lwsl_err("problem getting cert '%s' %lu: %s\n",
				 cert, error, s);

			return 1;
		}

		if (private_key) {
			/* set the private key from KeyFile */
			if (SSL_CTX_use_PrivateKey_file(vhost->tls.ssl_ctx, private_key,
							SSL_FILETYPE_PEM) != 1) {
				const char *s;
				error = ERR_get_error();
				s = ERR_error_string(
	#if defined(LWS_WITH_BORINGSSL)
					(uint32_t)
	#endif
						error,
					       (char *)vhost->context->pt[0].serv_buf);
				lwsl_err("ssl problem getting key '%s' %lu: %s\n",
					 private_key, error, s);
				return 1;
			}
		} else {
			if (vhost->protocols[0].callback(wsi,
				      LWS_CALLBACK_OPENSSL_CONTEXT_REQUIRES_PRIVATE_KEY,
							 vhost->tls.ssl_ctx, NULL, 0)) {
				lwsl_err("ssl private key not set\n");

				return 1;
			}
		}

		return 0;
	}

	/* otherwise allow for DER or PEM, file or memory image */

	if (lws_tls_alloc_pem_to_der_file(vhost->context, cert, mem_cert,
					  mem_cert_len, &p, &flen)) {
		lwsl_err("%s: couldn't read cert file\n", __func__);

		return 1;
	}

#if !defined(USE_WOLFSSL)
	ret = SSL_CTX_use_certificate_ASN1(vhost->tls.ssl_ctx,
#if defined(LWS_WITH_BORINGSSL)
				(size_t)
#else
				(int)
#endif
				flen, p);
#else
	ret = wolfSSL_CTX_use_certificate_buffer(vhost->tls.ssl_ctx,
						 (uint8_t *)p, (int)flen,
						 WOLFSSL_FILETYPE_ASN1);
#endif
	lws_free_set_NULL(p);
	if (ret != 1) {
		lwsl_err("%s: Problem loading cert\n", __func__);

		return 1;
	}

	if (lws_tls_alloc_pem_to_der_file(vhost->context, private_key,
					  mem_privkey, mem_privkey_len,
					  &p, &flen)) {
		lwsl_notice("unable to convert memory privkey\n");

		return 1;
	}

#if !defined(USE_WOLFSSL)
	ret = SSL_CTX_use_PrivateKey_ASN1(EVP_PKEY_RSA, vhost->tls.ssl_ctx, p,
#if defined(LWS_WITH_BORINGSSL)
			(size_t)
#else
					  (long)(long long)
#endif
					  flen);
	if (ret != 1) {
		ret = SSL_CTX_use_PrivateKey_ASN1(EVP_PKEY_EC,
						  vhost->tls.ssl_ctx, p,
#if defined(LWS_WITH_BORINGSSL)
			(size_t)
#else
					  (long)(long long)
#endif
						  flen);
	}
#else
	ret = wolfSSL_CTX_use_PrivateKey_buffer(vhost->tls.ssl_ctx, p, flen,
						WOLFSSL_FILETYPE_ASN1);
#endif
	lws_free_set_NULL(p);
	if (ret != 1)  {
		lwsl_notice("unable to use memory privkey\n");

		return 1;
	}

#else
		/*
		 * Although we have prepared update certs, we no longer have
		 * the rights to read our own cert + key we saved.
		 *
		 * If we were passed copies in memory buffers, use those
		 * instead.
		 *
		 * The passed memory-buffer cert image is in DER, and the
		 * memory-buffer private key image is PEM.
		 */
#ifndef USE_WOLFSSL
		if (lws_tls_alloc_pem_to_der_file(vhost->context, cert, mem_cert,
						  mem_cert_len, &p, &flen)) {
			lwsl_err("%s: couldn't convert pem to der\n", __func__);
			return 1;
		}
		if (SSL_CTX_use_certificate_ASN1(vhost->tls.ssl_ctx,
						 (int)flen,
						 (uint8_t *)p) != 1) {
#else
		if (wolfSSL_CTX_use_certificate_buffer(vhost->tls.ssl_ctx,
						 (uint8_t *)mem_cert,
						 (int)mem_cert_len,
						 WOLFSSL_FILETYPE_ASN1) != 1) {

#endif
			lwsl_err("Problem loading update cert\n");

			return 1;
		}

		if (lws_tls_alloc_pem_to_der_file(vhost->context, NULL,
						  mem_privkey, mem_privkey_len,
						  &p, &flen)) {
			lwsl_notice("unable to convert memory privkey\n");

			return 1;
		}
#ifndef USE_WOLFSSL
		if (SSL_CTX_use_PrivateKey_ASN1(EVP_PKEY_RSA,
						vhost->tls.ssl_ctx, p,
						(long)(long long)flen) != 1) {
#else
		if (wolfSSL_CTX_use_PrivateKey_buffer(vhost->tls.ssl_ctx, p,
					    (long)flen, WOLFSSL_FILETYPE_ASN1) != 1) {
#endif
			lwsl_notice("unable to use memory privkey\n");

			return 1;
		}

		goto check_key;
	}

	/* set the local certificate from CertFile */
	m = SSL_CTX_use_certificate_chain_file(vhost->tls.ssl_ctx, cert);
	if (m != 1) {
		error = ERR_get_error();
		lwsl_err("problem getting cert '%s' %lu: %s\n",
			 cert, error, ERR_error_string(error,
			       (char *)vhost->context->pt[0].serv_buf));

		return 1;
	}

	if (n != LWS_TLS_EXTANT_ALTERNATIVE && private_key) {
		/* set the private key from KeyFile */
		if (SSL_CTX_use_PrivateKey_file(vhost->tls.ssl_ctx, private_key,
					        SSL_FILETYPE_PEM) != 1) {
			error = ERR_get_error();
			lwsl_err("ssl problem getting key '%s' %lu: %s\n",
				 private_key, error,
				 ERR_error_string(error,
				      (char *)vhost->context->pt[0].serv_buf));
			return 1;
		}
	} else {
		if (vhost->protocols[0].callback(wsi,
			      LWS_CALLBACK_OPENSSL_CONTEXT_REQUIRES_PRIVATE_KEY,
						 vhost->tls.ssl_ctx, NULL, 0)) {
			lwsl_err("ssl private key not set\n");

			return 1;
		}
	}

check_key:
#endif

	/* verify private key */
	if (!SSL_CTX_check_private_key(vhost->tls.ssl_ctx)) {
		lwsl_err("Private SSL key doesn't match cert\n");

		return 1;
	}


#if !defined(OPENSSL_NO_EC) && defined(LWS_HAVE_EC_KEY_new_by_curve_name) && \
    ((OPENSSL_VERSION_NUMBER < 0x30000000l) || \
     defined(LWS_SUPPRESS_DEPRECATED_API_WARNINGS))
	if (vhost->tls.ecdh_curve[0])
		ecdh_curve = vhost->tls.ecdh_curve;

	ecdh_nid = OBJ_sn2nid(ecdh_curve);
	if (NID_undef == ecdh_nid) {
		lwsl_err("SSL: Unknown curve name '%s'", ecdh_curve);
		return 1;
	}

	ecdh = EC_KEY_new_by_curve_name(ecdh_nid);
	if (NULL == ecdh) {
		lwsl_err("SSL: Unable to create curve '%s'", ecdh_curve);
		return 1;
	}
	SSL_CTX_set_tmp_ecdh(vhost->tls.ssl_ctx, ecdh);
	EC_KEY_free(ecdh);

	SSL_CTX_set_options(vhost->tls.ssl_ctx, SSL_OP_SINGLE_ECDH_USE);

	lwsl_notice(" SSL ECDH curve '%s'\n", ecdh_curve);

	if (lws_check_opt(vhost->context->options, LWS_SERVER_OPTION_SSL_ECDH))
		lwsl_notice(" Using ECDH certificate support\n");

	/* Get X509 certificate from ssl context */
#if !defined(LWS_WITH_BORINGSSL)
#if !defined(LWS_HAVE_SSL_EXTRA_CHAIN_CERTS)
	x = sk_X509_value(vhost->tls.ssl_ctx->extra_certs, 0);
#else
	SSL_CTX_get_extra_chain_certs_only(vhost->tls.ssl_ctx, &extra_certs);
	if (extra_certs)
		x = sk_X509_value(extra_certs, 0);
	else
		lwsl_info("%s: no extra certs\n", __func__);
#endif
	if (!x) {
		//lwsl_err("%s: x is NULL\n", __func__);
		goto post_ecdh;
	}
#else
	return 0;
#endif /* !boringssl */

	/* Get the public key from certificate */
	pkey = X509_get_pubkey(x);
	if (!pkey) {
		lwsl_err("%s: pkey is NULL\n", __func__);

		return 1;
	}
	/* Get the key type */
	KeyType = EVP_PKEY_type(EVP_PKEY_id(pkey));

	if (EVP_PKEY_EC != KeyType) {
		lwsl_notice("Key type is not EC\n");
		return 0;
	}
	/* Get the key */
	EC_key = EVP_PKEY_get1_EC_KEY(pkey);
	/* Set ECDH parameter */
	if (!EC_key) {
		lwsl_err("%s: ECDH key is NULL \n", __func__);
		return 1;
	}
	SSL_CTX_set_tmp_ecdh(vhost->tls.ssl_ctx, EC_key);

	EC_KEY_free(EC_key);

#if !defined(OPENSSL_NO_EC) && !defined(LWS_WITH_BORINGSSL)
post_ecdh:
#endif
	vhost->tls.skipped_certs = 0;
#else
	lwsl_notice(" OpenSSL doesn't support ECDH\n");
#endif

	return 0;
}

int
lws_tls_server_vhost_backend_init(const struct lws_context_creation_info *info,
				  struct lws_vhost *vhost, struct lws *wsi)
{
	unsigned long error;
	SSL_METHOD *method = (SSL_METHOD *)SSLv23_server_method();

	if (!method) {
		const char *s;
		error = ERR_get_error();
		s = ERR_error_string(
#if defined(LWS_WITH_BORINGSSL)
			(uint32_t)
#endif
				error,
			       (char *)vhost->context->pt[0].serv_buf);

		lwsl_err("problem creating ssl method %lu: %s\n",
				error, s);
		return 1;
	}
	vhost->tls.ssl_ctx = SSL_CTX_new(method);	/* create context */
	if (!vhost->tls.ssl_ctx) {
		const char *s;

		error = ERR_get_error();
		s = ERR_error_string(
#if defined(LWS_WITH_BORINGSSL)
			(uint32_t)
#endif
				error,
			       (char *)vhost->context->pt[0].serv_buf);
		lwsl_err("problem creating ssl context %lu: %s\n",
				error, s);
		return 1;
	}

	SSL_CTX_set_ex_data(vhost->tls.ssl_ctx,
			    openssl_SSL_CTX_private_data_index,
			    (char *)vhost->context);
	/* Disable SSLv2 and SSLv3 */
	SSL_CTX_set_options(vhost->tls.ssl_ctx, SSL_OP_NO_SSLv2 |
						SSL_OP_NO_SSLv3);
#ifdef SSL_OP_NO_COMPRESSION
	SSL_CTX_set_options(vhost->tls.ssl_ctx, SSL_OP_NO_COMPRESSION);
#endif
	SSL_CTX_set_options(vhost->tls.ssl_ctx, SSL_OP_SINGLE_DH_USE);
	SSL_CTX_set_options(vhost->tls.ssl_ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);

	if (info->ssl_cipher_list)
		SSL_CTX_set_cipher_list(vhost->tls.ssl_ctx, info->ssl_cipher_list);

#if defined(LWS_HAVE_SSL_CTX_set_ciphersuites)
	if (info->tls1_3_plus_cipher_list)
		SSL_CTX_set_ciphersuites(vhost->tls.ssl_ctx,
					 info->tls1_3_plus_cipher_list);
#endif

#if !defined(OPENSSL_NO_TLSEXT)
	SSL_CTX_set_tlsext_servername_callback(vhost->tls.ssl_ctx,
					       lws_ssl_server_name_cb);
	SSL_CTX_set_tlsext_servername_arg(vhost->tls.ssl_ctx, vhost->context);
#endif

	if (info->ssl_ca_filepath &&
#if defined(LWS_HAVE_SSL_CTX_load_verify_file)
	    !SSL_CTX_load_verify_file(vhost->tls.ssl_ctx,
				      info->ssl_ca_filepath)) {
#else
	    !SSL_CTX_load_verify_locations(vhost->tls.ssl_ctx,
					   info->ssl_ca_filepath, NULL)) {
#endif
		lwsl_err("%s: SSL_CTX_load_verify_locations unhappy\n",
			 __func__);
	}

#if defined(USE_WOLFSSL)
		long
#else
#if defined(LWS_WITH_BORINGSSL)
		uint32_t
#else
#if (OPENSSL_VERSION_NUMBER >= 0x10003000l) && !defined(LIBRESSL_VERSION_NUMBER) /* not documented by openssl */
		unsigned long
#else
		long
#endif
#endif
#endif
			ssl_options_set_value =
#if defined(USE_WOLFSSL)
				(long)
#else
#if defined(LWS_WITH_BORINGSSL)
				(uint32_t)
#else
#if (OPENSSL_VERSION_NUMBER >= 0x10003000l) && !defined(LIBRESSL_VERSION_NUMBER) /* not documented by openssl */
				(unsigned long)
#else
				(long)
#endif
#endif
#endif
					info->ssl_options_set;

	if (info->ssl_options_set)
		SSL_CTX_set_options(vhost->tls.ssl_ctx, ssl_options_set_value);

#if (OPENSSL_VERSION_NUMBER >= 0x009080df) && !defined(USE_WOLFSSL)

/* SSL_clear_options introduced in 0.9.8m */
#if defined(LWS_WITH_BORINGSSL)
	uint32_t
#else
#if (OPENSSL_VERSION_NUMBER >= 0x10003000l)  && !defined(LIBRESSL_VERSION_NUMBER)/* not documented by openssl */
	unsigned long
#else
	long
#endif
#endif

	ssl_options_clear_value =
#if defined(LWS_WITH_BORINGSSL)
				(uint32_t)
#else
#if (OPENSSL_VERSION_NUMBER >= 0x10003000l)  && !defined(LIBRESSL_VERSION_NUMBER)/* not documented by openssl */
				(unsigned long)
#else
				(long)
#endif
#endif
					info->ssl_options_clear;

	if (info->ssl_options_clear) {
		SSL_CTX_clear_options(vhost->tls.ssl_ctx, ssl_options_clear_value);
	}

	lwsl_info(" SSL options 0x%lX\n",
			(unsigned long)SSL_CTX_get_options(vhost->tls.ssl_ctx));
#endif

	if (!vhost->tls.use_ssl ||
	    (!info->ssl_cert_filepath && !info->server_ssl_cert_mem))
		return 0;

	lws_ssl_bind_passphrase(vhost->tls.ssl_ctx, 0, info);

	return lws_tls_server_certs_load(vhost, wsi, info->ssl_cert_filepath,
					 info->ssl_private_key_filepath,
					 info->server_ssl_cert_mem,
					 info->server_ssl_cert_mem_len,
					 info->server_ssl_private_key_mem,
					 info->server_ssl_private_key_mem_len);
}

int
lws_tls_server_new_nonblocking(struct lws *wsi, lws_sockfd_type accept_fd)
{
#if !defined(USE_WOLFSSL)
	BIO *bio;
#endif

	errno = 0;
	ERR_clear_error();
	wsi->tls.ssl = SSL_new(wsi->a.vhost->tls.ssl_ctx);
	if (wsi->tls.ssl == NULL) {
		lwsl_err("SSL_new failed: %d (errno %d)\n",
			 lws_ssl_get_error(wsi, 0), errno);

		lws_tls_err_describe_clear();
		return 1;
	}

	SSL_set_ex_data(wsi->tls.ssl, openssl_websocket_private_data_index, wsi);
	SSL_set_fd(wsi->tls.ssl, (int)(lws_intptr_t)accept_fd);

#ifdef USE_WOLFSSL
#ifdef USE_OLD_CYASSL
	CyaSSL_set_using_nonblock(wsi->tls.ssl, 1);
#else
	wolfSSL_set_using_nonblock(wsi->tls.ssl, 1);
#endif
#else

	SSL_set_mode(wsi->tls.ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER |
				   SSL_MODE_RELEASE_BUFFERS);
	bio = SSL_get_rbio(wsi->tls.ssl);
	if (bio)
		BIO_set_nbio(bio, 1); /* nonblocking */
	else
		lwsl_notice("NULL rbio\n");
	bio = SSL_get_wbio(wsi->tls.ssl);
	if (bio)
		BIO_set_nbio(bio, 1); /* nonblocking */
	else
		lwsl_notice("NULL rbio\n");
#endif

#if defined (LWS_HAVE_SSL_SET_INFO_CALLBACK)
		if (wsi->a.vhost->tls.ssl_info_event_mask)
			SSL_set_info_callback(wsi->tls.ssl, lws_ssl_info_callback);
#endif

	return 0;
}

int
lws_tls_server_abort_connection(struct lws *wsi)
{
	if (wsi->tls.use_ssl)
		SSL_shutdown(wsi->tls.ssl);
	SSL_free(wsi->tls.ssl);

	return 0;
}

enum lws_ssl_capable_status
lws_tls_server_accept(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	union lws_tls_cert_info_results ir;
	int m, n;

	errno = 0;
	ERR_clear_error();
	n = SSL_accept(wsi->tls.ssl);

	wsi->skip_fallback = 1;

	if (n == 1) {
		n = lws_tls_peer_cert_info(wsi, LWS_TLS_CERT_INFO_COMMON_NAME, &ir,
					   sizeof(ir.ns.name));
		if (!n)
			lwsl_notice("%s: client cert CN '%s'\n", __func__,
				    ir.ns.name);
		else
			lwsl_info("%s: no client cert CN\n", __func__);

		lws_openssl_describe_cipher(wsi);

		if (SSL_pending(wsi->tls.ssl) &&
		    lws_dll2_is_detached(&wsi->tls.dll_pending_tls))
			lws_dll2_add_head(&wsi->tls.dll_pending_tls,
					  &pt->tls.dll_pending_tls_owner);

		return LWS_SSL_CAPABLE_DONE;
	}

	m = lws_ssl_get_error(wsi, n);
	lws_tls_err_describe_clear();

	if (m == SSL_ERROR_SYSCALL || m == SSL_ERROR_SSL)
		return LWS_SSL_CAPABLE_ERROR;

	if (m == SSL_ERROR_WANT_READ ||
	    (m != SSL_ERROR_ZERO_RETURN && SSL_want_read(wsi->tls.ssl))) {
		if (lws_change_pollfd(wsi, 0, LWS_POLLIN)) {
			lwsl_info("%s: WANT_READ change_pollfd failed\n",
				  __func__);
			return LWS_SSL_CAPABLE_ERROR;
		}

		lwsl_info("SSL_ERROR_WANT_READ: m %d\n", m);
		return LWS_SSL_CAPABLE_MORE_SERVICE_READ;
	}
	if (m == SSL_ERROR_WANT_WRITE || SSL_want_write(wsi->tls.ssl)) {
		lwsl_debug("%s: WANT_WRITE\n", __func__);

		if (lws_change_pollfd(wsi, 0, LWS_POLLOUT)) {
			lwsl_info("%s: WANT_WRITE change_pollfd failed\n",
				  __func__);
			return LWS_SSL_CAPABLE_ERROR;
		}
		return LWS_SSL_CAPABLE_MORE_SERVICE_WRITE;
	}

	return LWS_SSL_CAPABLE_ERROR;
}

#if defined(LWS_WITH_ACME)
static int
lws_tls_openssl_rsa_new_key(RSA **rsa, int bits)
{
	BIGNUM *bn = BN_new();
	int n;

	if (!bn)
		return 1;

	if (BN_set_word(bn, RSA_F4) != 1) {
		BN_free(bn);
		return 1;
	}

	*rsa = RSA_new();
	if (!*rsa) {
		BN_free(bn);
		return 1;
	}

	n = RSA_generate_key_ex(*rsa, bits, bn, NULL);
	BN_free(bn);
	if (n == 1)
		return 0;

	RSA_free(*rsa);
	*rsa = NULL;

	return 1;
}

struct lws_tls_ss_pieces {
	X509 *x509;
	EVP_PKEY *pkey;
	RSA *rsa;
};

int
lws_tls_acme_sni_cert_create(struct lws_vhost *vhost, const char *san_a,
			     const char *san_b)
{
	GENERAL_NAMES *gens = sk_GENERAL_NAME_new_null();
	GENERAL_NAME *gen = NULL;
	ASN1_IA5STRING *ia5 = NULL;
	X509_NAME *name;

	if (!gens)
		return 1;

	vhost->tls.ss = lws_zalloc(sizeof(*vhost->tls.ss), "sni cert");
	if (!vhost->tls.ss) {
		GENERAL_NAMES_free(gens);
		return 1;
	}

	vhost->tls.ss->x509 = X509_new();
	if (!vhost->tls.ss->x509)
		goto bail;

	ASN1_INTEGER_set(X509_get_serialNumber(vhost->tls.ss->x509), 1);
	X509_gmtime_adj(X509_get_notBefore(vhost->tls.ss->x509), 0);
	X509_gmtime_adj(X509_get_notAfter(vhost->tls.ss->x509), 3600);

	vhost->tls.ss->pkey = EVP_PKEY_new();
	if (!vhost->tls.ss->pkey)
		goto bail0;

	if (lws_tls_openssl_rsa_new_key(&vhost->tls.ss->rsa, 4096))
		goto bail1;

	if (!EVP_PKEY_assign_RSA(vhost->tls.ss->pkey, vhost->tls.ss->rsa))
		goto bail2;

	X509_set_pubkey(vhost->tls.ss->x509, vhost->tls.ss->pkey);

	name = X509_get_subject_name(vhost->tls.ss->x509);
	X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC,
				   (unsigned char *)"GB",          -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC,
				   (unsigned char *)"somecompany", -1, -1, 0);
	if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
				   (unsigned char *)"temp.acme.invalid",
				   	   	   -1, -1, 0) != 1) {
		lwsl_notice("failed to add CN\n");
		goto bail2;
	}
	X509_set_issuer_name(vhost->tls.ss->x509, name);

	/* add the SAN payloads */

	gen = GENERAL_NAME_new();
	ia5 = ASN1_IA5STRING_new();
	if (!ASN1_STRING_set(ia5, san_a, -1)) {
		lwsl_notice("failed to set ia5\n");
		GENERAL_NAME_free(gen);
		goto bail2;
	}
	GENERAL_NAME_set0_value(gen, GEN_DNS, ia5);
	sk_GENERAL_NAME_push(gens, gen);

	if (X509_add1_ext_i2d(vhost->tls.ss->x509, NID_subject_alt_name,
			    gens, 0, X509V3_ADD_APPEND) != 1)
		goto bail2;

	GENERAL_NAMES_free(gens);

	if (san_b && san_b[0]) {
		gens = sk_GENERAL_NAME_new_null();
		gen = GENERAL_NAME_new();
		ia5 = ASN1_IA5STRING_new();
		if (!ASN1_STRING_set(ia5, san_a, -1)) {
			lwsl_notice("failed to set ia5\n");
			GENERAL_NAME_free(gen);
			goto bail2;
		}
		GENERAL_NAME_set0_value(gen, GEN_DNS, ia5);
		sk_GENERAL_NAME_push(gens, gen);

		if (X509_add1_ext_i2d(vhost->tls.ss->x509, NID_subject_alt_name,
				    gens, 0, X509V3_ADD_APPEND) != 1)
			goto bail2;

		GENERAL_NAMES_free(gens);
	}

	/* sign it with our private key */
	if (!X509_sign(vhost->tls.ss->x509, vhost->tls.ss->pkey, EVP_sha256()))
		goto bail2;

#if 0
	{/* useful to take a sample of a working cert for mbedtls to crib */
		FILE *fp = fopen("/tmp/acme-temp-cert", "w+");

		i2d_X509_fp(fp, vhost->tls.ss->x509);
		fclose(fp);
	}
#endif

	/* tell the vhost to use our crafted certificate */
	SSL_CTX_use_certificate(vhost->tls.ssl_ctx, vhost->tls.ss->x509);
	/* and to use our generated private key */
	SSL_CTX_use_PrivateKey(vhost->tls.ssl_ctx, vhost->tls.ss->pkey);

	return 0;

bail2:
	RSA_free(vhost->tls.ss->rsa);
bail1:
	EVP_PKEY_free(vhost->tls.ss->pkey);
bail0:
	X509_free(vhost->tls.ss->x509);
bail:
	lws_free(vhost->tls.ss);
	GENERAL_NAMES_free(gens);

	return 1;
}

void
lws_tls_acme_sni_cert_destroy(struct lws_vhost *vhost)
{
	if (!vhost->tls.ss)
		return;

	EVP_PKEY_free(vhost->tls.ss->pkey);
	X509_free(vhost->tls.ss->x509);
	lws_free_set_NULL(vhost->tls.ss);
}

static int
lws_tls_openssl_add_nid(X509_NAME *name, int nid, const char *value)
{
	X509_NAME_ENTRY *e;
	int n;

	if (!value || value[0] == '\0')
		value = "none";

	e = X509_NAME_ENTRY_create_by_NID(NULL, nid, MBSTRING_ASC,
					  (unsigned char *)value, -1);
	if (!e)
		return 1;
	n = X509_NAME_add_entry(name, e, -1, 0);
	X509_NAME_ENTRY_free(e);

	return n != 1;
}

static int nid_list[] = {
	NID_countryName,		/* LWS_TLS_REQ_ELEMENT_COUNTRY */
	NID_stateOrProvinceName,	/* LWS_TLS_REQ_ELEMENT_STATE */
	NID_localityName,		/* LWS_TLS_REQ_ELEMENT_LOCALITY */
	NID_organizationName,		/* LWS_TLS_REQ_ELEMENT_ORGANIZATION */
	NID_commonName,			/* LWS_TLS_REQ_ELEMENT_COMMON_NAME */
	NID_subject_alt_name,		/* LWS_TLS_REQ_ELEMENT_SUBJECT_ALT_NAME */
	NID_pkcs9_emailAddress,		/* LWS_TLS_REQ_ELEMENT_EMAIL */
};

int
lws_tls_acme_sni_csr_create(struct lws_context *context, const char *elements[],
			    uint8_t *csr, size_t csr_len, char **privkey_pem,
			    size_t *privkey_len)
{
	uint8_t *csr_in = csr;
	RSA *rsakey;
	X509_REQ *req;
	X509_NAME *subj;
	EVP_PKEY *pkey;
	char *p, *end;
	BIO *bio;
	long bio_len;
	int n, ret = -1;

	if (lws_tls_openssl_rsa_new_key(&rsakey, 4096))
		return -1;

	pkey = EVP_PKEY_new();
	if (!pkey)
		goto bail0;
	if (!EVP_PKEY_set1_RSA(pkey, rsakey))
		goto bail1;

	req = X509_REQ_new();
	if (!req)
	        goto bail1;

	X509_REQ_set_pubkey(req, pkey);

	subj = X509_NAME_new();
	if (!subj)
		goto bail2;

	for (n = 0; n < LWS_TLS_REQ_ELEMENT_COUNT; n++)
		if (elements[n] &&
			lws_tls_openssl_add_nid(subj, nid_list[n],
				elements[n])) {
				lwsl_notice("%s: failed to add element %d\n",
						__func__, n);
			goto bail3;
		}

	if (X509_REQ_set_subject_name(req, subj) != 1)
		goto bail3;

	if (elements[LWS_TLS_REQ_ELEMENT_SUBJECT_ALT_NAME]) {
		STACK_OF(X509_EXTENSION) *exts;
		X509_EXTENSION *ext;
		char san[256];

		exts = sk_X509_EXTENSION_new_null();
		if (!exts)
			goto bail3;

		lws_snprintf(san, sizeof(san), "DNS:%s,DNS:%s",
				elements[LWS_TLS_REQ_ELEMENT_COMMON_NAME],
				elements[LWS_TLS_REQ_ELEMENT_SUBJECT_ALT_NAME]);

		ext = X509V3_EXT_conf_nid(NULL, NULL, NID_subject_alt_name,
				san);
		if (!ext) {
			sk_X509_EXTENSION_pop_free(exts, X509_EXTENSION_free);
			goto bail3;
		}
		sk_X509_EXTENSION_push(exts, ext);

		if (!X509_REQ_add_extensions(req, exts)) {
			sk_X509_EXTENSION_pop_free(exts, X509_EXTENSION_free);
			goto bail3;
		}
		sk_X509_EXTENSION_pop_free(exts, X509_EXTENSION_free);
	}

	if (!X509_REQ_sign(req, pkey, EVP_sha256()))
		goto bail3;

	/*
	 * issue the CSR as PEM to a BIO, and translate to b64urlenc without
	 * headers, trailers, or whitespace
	 */

	bio = BIO_new(BIO_s_mem());
	if (!bio)
		goto bail3;

	if (PEM_write_bio_X509_REQ(bio, req) != 1) {
		BIO_free(bio);
		goto bail3;
	}

	bio_len = BIO_get_mem_data(bio, &p);
	end = p + bio_len;

	/* strip the header line */
	while (p < end && *p != '\n')
		p++;

	while (p < end && csr_len) {
		if (*p == '\n') {
			p++;
			continue;
		}

		if (*p == '-')
			break;

		if (*p == '+')
			*csr++ = '-';
		else
			if (*p == '/')
				*csr++ = '_';
			else
				*csr++ = (uint8_t)*p;
		p++;
		csr_len--;
	}
	BIO_free(bio);
	if (!csr_len) {
		lwsl_notice("%s: need %ld for CSR\n", __func__, bio_len);
		goto bail3;
	}

	/*
	 * Also return the private key as a PEM in memory
	 * (platform may not have a filesystem)
	 */
	bio = BIO_new(BIO_s_mem());
	if (!bio)
		goto bail3;

	if (PEM_write_bio_PrivateKey(bio, pkey, NULL, NULL, 0, 0, NULL) != 1) {
		BIO_free(bio);
		goto bail3;
	}
	bio_len = BIO_get_mem_data(bio, &p);
	*privkey_pem = malloc((unsigned long)bio_len); /* malloc so user code can own / free */
	*privkey_len = (size_t)bio_len;
	if (!*privkey_pem) {
		lwsl_notice("%s: need %ld for private key\n", __func__,
			    bio_len);
		BIO_free(bio);
		goto bail3;
	}
	memcpy(*privkey_pem, p, (unsigned int)(int)(long long)bio_len);
	BIO_free(bio);

	ret = lws_ptr_diff(csr, csr_in);

bail3:
	X509_NAME_free(subj);
bail2:
	X509_REQ_free(req);
bail1:
	EVP_PKEY_free(pkey);
bail0:
	RSA_free(rsakey);

	return ret;
}
#endif
