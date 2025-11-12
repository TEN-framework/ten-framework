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

enum lws_tls_cert_info {
	LWS_TLS_CERT_INFO_VALIDITY_FROM,
	/**< fills .time with the time_t the cert validity started from */
	LWS_TLS_CERT_INFO_VALIDITY_TO,
	/**< fills .time with the time_t the cert validity ends at */
	LWS_TLS_CERT_INFO_COMMON_NAME,
	/**< fills up to len bytes of .ns.name with the cert common name */
	LWS_TLS_CERT_INFO_ISSUER_NAME,
	/**< fills up to len bytes of .ns.name with the cert issuer name */
	LWS_TLS_CERT_INFO_USAGE,
	/**< fills verified with a bitfield asserting the valid uses */
	LWS_TLS_CERT_INFO_VERIFIED,
	/**< fills .verified with a bool representing peer cert validity,
	 *   call returns -1 if no cert */
	LWS_TLS_CERT_INFO_OPAQUE_PUBLIC_KEY,
	/**< the certificate's public key, as an opaque bytestream.  These
	 * opaque bytestreams can only be compared with each other using the
	 * same tls backend, ie, OpenSSL or mbedTLS.  The different backends
	 * produce different, incompatible representations for the same cert.
	 */
	LWS_TLS_CERT_INFO_DER_RAW,
	/**< the certificate's raw DER representation.  If it's too big,
	 * -1 is returned and the size will be returned in buf->ns.len.
	 * If the certificate cannot be found -1 is returned and 0 in
	 * buf->ns.len. */
	LWS_TLS_CERT_INFO_AUTHORITY_KEY_ID,
	/**< If the cert has one, the key ID responsible for the signature */
	LWS_TLS_CERT_INFO_AUTHORITY_KEY_ID_ISSUER,
	/**< If the cert has one, the issuer responsible for the signature */
	LWS_TLS_CERT_INFO_AUTHORITY_KEY_ID_SERIAL,
	/**< If the cert has one, serial number responsible for the signature */
	LWS_TLS_CERT_INFO_SUBJECT_KEY_ID,
	/**< If the cert has one, the cert's subject key ID */
};

union lws_tls_cert_info_results {
	unsigned int verified;
	time_t time;
	unsigned int usage;
	struct {
		int len;
		/* KEEP LAST... notice the [64] is only there because
		 * name[] is not allowed in a union.  The actual length of
		 * name[] is arbitrary and is passed into the api using the
		 * len parameter.  Eg
		 *
		 * char big[1024];
		 * union lws_tls_cert_info_results *buf =
		 * 	(union lws_tls_cert_info_results *)big;
		 *
		 * lws_tls_peer_cert_info(wsi, type, buf, sizeof(big) -
		 *			  sizeof(*buf) + sizeof(buf->ns.name));
		 */
		char name[64];
	} ns;
};

struct lws_x509_cert;
struct lws_jwk;

/**
 * lws_x509_create() - Allocate an lws_x509_cert object
 *
 * \param x509: pointer to lws_x509_cert pointer to be set to allocated object
 *
 * Allocates an lws_x509_cert object and set *x509 to point to it.
 */
LWS_VISIBLE LWS_EXTERN int
lws_x509_create(struct lws_x509_cert **x509);

/**
 * lws_x509_parse_from_pem() - Read one or more x509 certs in PEM format from memory
 *
 * \param x509: pointer to lws_x509_cert object
 * \param pem: pointer to PEM format content
 * \param len: length of PEM format content
 *
 * Parses PEM certificates in memory into a native x509 representation for the
 * TLS library.  If there are multiple PEM certs concatenated, they are all
 * read into the same object and exist as a "chain".
 *
 * IMPORTANT for compatibility with mbedtls, the last used byte of \p pem
 * must be '\0' and the \p len must include it.
 *
 * Returns 0 if all went OK, or nonzero for failure.
 */
LWS_VISIBLE LWS_EXTERN int
lws_x509_parse_from_pem(struct lws_x509_cert *x509, const void *pem, size_t len);

/**
 * lws_x509_verify() - Validate signing relationship between one or more certs
 *		       and a trusted CA cert
 *
 * \param x509: pointer to lws_x509_cert object, may contain multiple
 * \param trusted: a single, trusted cert object that we are checking for
 * \param common_name: NULL, or required CN (Common Name) of \p x509
 *
 * Returns 0 if the cert or certs in \p x509 represent a complete chain that is
 * ultimately signed by the cert in \p trusted.  Returns nonzero if that's not
 * the case.
 */
LWS_VISIBLE LWS_EXTERN int
lws_x509_verify(struct lws_x509_cert *x509, struct lws_x509_cert *trusted,
		const char *common_name);

/**
 * lws_x509_public_to_jwk() - Copy the public key out of a cert and into a JWK
 *
 * \param jwk: pointer to the jwk to initialize and set to the public key
 * \param x509: pointer to lws_x509_cert object that has the public key
 * \param curves: NULL to disallow EC, else a comma-separated list of valid
 *		  curves using the JWA naming, eg, "P-256,P-384,P-521".
 * \param rsabits: minimum number of RSA bits required in the cert if RSA
 *
 * Returns 0 if JWK was set to the certificate public key correctly and the
 * curve / the RSA key size was acceptable.  Automatically produces an RSA or
 * EC JWK depending on what the cert had.
 */
LWS_VISIBLE LWS_EXTERN int
lws_x509_public_to_jwk(struct lws_jwk *jwk, struct lws_x509_cert *x509,
		       const char *curves, int rsabits);

/**
 * lws_x509_jwk_privkey_pem() - Copy a private key PEM into a jwk that has the
 *				public part already
 *
 * \param cx: lws_context (for random)
 * \param jwk: pointer to the jwk to initialize and set to the public key
 * \param pem: pointer to PEM private key in memory
 * \param len: length of PEM private key in memory
 * \param passphrase: NULL or passphrase needed to decrypt private key
 *
 * IMPORTANT for compatibility with mbedtls, the last used byte of \p pem
 * must be '\0' and the \p len must include it.
 *
 * Returns 0 if the private key was successfully added to the JWK, else
 * nonzero if failed.
 *
 * The PEM image in memory is zeroed down on both successful and failed exits.
 * The caller should take care to zero down passphrase if used.
 */
LWS_VISIBLE LWS_EXTERN int
lws_x509_jwk_privkey_pem(struct lws_context *cx, struct lws_jwk *jwk,
			 void *pem, size_t len, const char *passphrase);

/**
 * lws_x509_destroy() - Destroy a previously allocated lws_x509_cert object
 *
 * \param x509: pointer to lws_x509_cert pointer
 *
 * Deallocates an lws_x509_cert object and sets its pointer to NULL.
 */
LWS_VISIBLE LWS_EXTERN void
lws_x509_destroy(struct lws_x509_cert **x509);

LWS_VISIBLE LWS_EXTERN int
lws_x509_info(struct lws_x509_cert *x509, enum lws_tls_cert_info type,
	      union lws_tls_cert_info_results *buf, size_t len);

/**
 * lws_tls_peer_cert_info() - get information from the peer's TLS cert
 *
 * \param wsi: the connection to query
 * \param type: one of LWS_TLS_CERT_INFO_
 * \param buf: pointer to union to take result
 * \param len: when result is a string, the true length of buf->ns.name[]
 *
 * lws_tls_peer_cert_info() lets you get hold of information from the peer
 * certificate.
 *
 * Return 0 if there is a result in \p buf, or nonzero indicating there was no
 * cert, or another problem.
 *
 * This function works the same no matter if the TLS backend is OpenSSL or
 * mbedTLS.
 */
LWS_VISIBLE LWS_EXTERN int
lws_tls_peer_cert_info(struct lws *wsi, enum lws_tls_cert_info type,
		       union lws_tls_cert_info_results *buf, size_t len);

/**
 * lws_tls_vhost_cert_info() - get information from the vhost's own TLS cert
 *
 * \param vhost: the vhost to query
 * \param type: one of LWS_TLS_CERT_INFO_
 * \param buf: pointer to union to take result
 * \param len: when result is a string, the true length of buf->ns.name[]
 *
 * lws_tls_vhost_cert_info() lets you get hold of information from the vhost
 * certificate.
 *
 * Return 0 if there is a result in \p buf, or nonzero indicating there was no
 * cert, or another problem.
 *
 * This function works the same no matter if the TLS backend is OpenSSL or
 * mbedTLS.
 */
LWS_VISIBLE LWS_EXTERN int
lws_tls_vhost_cert_info(struct lws_vhost *vhost, enum lws_tls_cert_info type,
		        union lws_tls_cert_info_results *buf, size_t len);

/**
 * lws_tls_acme_sni_cert_create() - creates a temp selfsigned cert
 *				    and attaches to a vhost
 *
 * \param vhost: the vhost to acquire the selfsigned cert
 * \param san_a: SAN written into the certificate
 * \param san_b: second SAN written into the certificate
 *
 *
 * Returns 0 if created and attached to the vhost.  Returns nonzero if problems,
 * and frees all allocations before returning.
 *
 * On success, any allocations are destroyed at vhost destruction automatically.
 */
LWS_VISIBLE LWS_EXTERN int
lws_tls_acme_sni_cert_create(struct lws_vhost *vhost, const char *san_a,
			     const char *san_b);

/**
 * lws_tls_acme_sni_csr_create() - creates a CSR and related private key PEM
 *
 * \param context: lws_context used for random
 * \param elements: array of LWS_TLS_REQ_ELEMENT_COUNT const char *
 * \param csr: buffer that will get the b64URL(ASN-1 CSR)
 * \param csr_len: max length of the csr buffer
 * \param privkey_pem: pointer to pointer allocated to hold the privkey_pem
 * \param privkey_len: pointer to size_t set to the length of the privkey_pem
 *
 * Creates a CSR according to the information in \p elements, and a private
 * RSA key used to sign the CSR.
 *
 * The outputs are the b64URL(ASN-1 CSR) into csr, and the PEM private key into
 * privkey_pem.
 *
 * Notice that \p elements points to an array of const char *s pointing to the
 * information listed in the enum above.  If an entry is NULL or an empty
 * string, the element is set to "none" in the CSR.
 *
 * Returns 0 on success or nonzero for failure.
 */
LWS_VISIBLE LWS_EXTERN int
lws_tls_acme_sni_csr_create(struct lws_context *context, const char *elements[],
			    uint8_t *csr, size_t csr_len, char **privkey_pem,
			    size_t *privkey_len);

/**
 * lws_tls_cert_updated() - update every vhost using the given cert path
 *
 * \param context: our lws_context
 * \param certpath: the filepath to the certificate
 * \param keypath: the filepath to the private key of the certificate
 * \param mem_cert: copy of the cert in memory
 * \param len_mem_cert: length of the copy of the cert in memory
 * \param mem_privkey: copy of the private key in memory
 * \param len_mem_privkey: length of the copy of the private key in memory
 *
 * Checks every vhost to see if it is the using certificate described by the
 * the given filepaths.  If so, it attempts to update the vhost ssl_ctx to use
 * the new certificate.
 *
 * Returns 0 on success or nonzero for failure.
 */
LWS_VISIBLE LWS_EXTERN int
lws_tls_cert_updated(struct lws_context *context, const char *certpath,
		     const char *keypath,
		     const char *mem_cert, size_t len_mem_cert,
		     const char *mem_privkey, size_t len_mem_privkey);

