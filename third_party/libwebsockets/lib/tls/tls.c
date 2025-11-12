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
#include "private-lib-tls.h"

#if defined(LWS_WITH_NETWORK)
#if defined(LWS_WITH_MBEDTLS) || (defined(OPENSSL_VERSION_NUMBER) && \
				  OPENSSL_VERSION_NUMBER >= 0x10002000L)
static int
alpn_cb(SSL *s, const unsigned char **out, unsigned char *outlen,
	const unsigned char *in, unsigned int inlen, void *arg)
{
#if !defined(LWS_WITH_MBEDTLS)
	struct alpn_ctx *alpn_ctx = (struct alpn_ctx *)arg;

	if (SSL_select_next_proto((unsigned char **)out, outlen, alpn_ctx->data,
				  alpn_ctx->len, in, inlen) !=
	    OPENSSL_NPN_NEGOTIATED)
		return SSL_TLSEXT_ERR_NOACK;
#endif

	return SSL_TLSEXT_ERR_OK;
}
#endif

int
lws_tls_restrict_borrow(struct lws *wsi)
{
	struct lws_context *cx = wsi->a.context;

	if (cx->simultaneous_ssl_restriction &&
	    cx->simultaneous_ssl >= cx->simultaneous_ssl_restriction) {
		lwsl_notice("%s: tls connection limit %d\n", __func__,
			    cx->simultaneous_ssl);
		return 1;
	}

	if (cx->simultaneous_ssl_handshake_restriction &&
	    cx->simultaneous_ssl_handshake >=
			    cx->simultaneous_ssl_handshake_restriction) {
		lwsl_notice("%s: tls handshake limit %d\n", __func__,
			    cx->simultaneous_ssl);
		return 1;
	}

	cx->simultaneous_ssl++;
	cx->simultaneous_ssl_handshake++;
	wsi->tls_borrowed_hs = 1;
	wsi->tls_borrowed = 1;

	lwsl_info("%s: %d -> %d\n", __func__,
		  cx->simultaneous_ssl - 1,
		  cx->simultaneous_ssl);

	assert(!cx->simultaneous_ssl_restriction ||
			cx->simultaneous_ssl <=
				cx->simultaneous_ssl_restriction);
	assert(!cx->simultaneous_ssl_handshake_restriction ||
			cx->simultaneous_ssl_handshake <=
				cx->simultaneous_ssl_handshake_restriction);

#if defined(LWS_WITH_SERVER)
	lws_gate_accepts(cx,
			(cx->simultaneous_ssl_restriction &&
			 cx->simultaneous_ssl == cx->simultaneous_ssl_restriction) ||
			(cx->simultaneous_ssl_handshake_restriction &&
			 cx->simultaneous_ssl_handshake == cx->simultaneous_ssl_handshake_restriction));
#endif

	return 0;
}

static void
_lws_tls_restrict_return(struct lws *wsi)
{
#if defined(LWS_WITH_SERVER)
	struct lws_context *cx = wsi->a.context;

	assert(cx->simultaneous_ssl_handshake >= 0);
	assert(cx->simultaneous_ssl >= 0);

	lws_gate_accepts(cx,
			(cx->simultaneous_ssl_restriction &&
			 cx->simultaneous_ssl == cx->simultaneous_ssl_restriction) ||
			(cx->simultaneous_ssl_handshake_restriction &&
			 cx->simultaneous_ssl_handshake == cx->simultaneous_ssl_handshake_restriction));
#endif
}

void
lws_tls_restrict_return_handshake(struct lws *wsi)
{
	struct lws_context *cx = wsi->a.context;

	/* we're just returning the hs part */

	if (!wsi->tls_borrowed_hs)
		return;

	wsi->tls_borrowed_hs = 0; /* return it one time per wsi */
	cx->simultaneous_ssl_handshake--;

	lwsl_info("%s:  %d -> %d\n", __func__,
		  cx->simultaneous_ssl_handshake + 1,
		  cx->simultaneous_ssl_handshake);

	_lws_tls_restrict_return(wsi);
}

void
lws_tls_restrict_return(struct lws *wsi)
{
	struct lws_context *cx = wsi->a.context;

	if (!wsi->tls_borrowed)
		return;

	wsi->tls_borrowed = 0;
	cx->simultaneous_ssl--;

	lwsl_info("%s: %d -> %d\n", __func__,
		  cx->simultaneous_ssl + 1,
		  cx->simultaneous_ssl);

	/* We're returning everything, even if hs didn't complete */

	if (wsi->tls_borrowed_hs)
		lws_tls_restrict_return_handshake(wsi);
	else
		_lws_tls_restrict_return(wsi);
}

void
lws_context_init_alpn(struct lws_vhost *vhost)
{
#if defined(LWS_WITH_MBEDTLS) || (defined(OPENSSL_VERSION_NUMBER) && \
				  OPENSSL_VERSION_NUMBER >= 0x10002000L)
	const char *alpn_comma = vhost->context->tls.alpn_default;

	if (vhost->tls.alpn)
		alpn_comma = vhost->tls.alpn;

	lwsl_info(" Server '%s' advertising ALPN: %s\n",
		    vhost->name, alpn_comma);

	vhost->tls.alpn_ctx.len = (uint8_t)lws_alpn_comma_to_openssl(alpn_comma,
					vhost->tls.alpn_ctx.data,
					sizeof(vhost->tls.alpn_ctx.data) - 1);

	SSL_CTX_set_alpn_select_cb(vhost->tls.ssl_ctx, alpn_cb,
				   &vhost->tls.alpn_ctx);
#else
	lwsl_err(" HTTP2 / ALPN configured "
		 "but not supported by OpenSSL 0x%lx\n",
		 OPENSSL_VERSION_NUMBER);
#endif // OPENSSL_VERSION_NUMBER >= 0x10002000L
}

int
lws_tls_server_conn_alpn(struct lws *wsi)
{
#if defined(LWS_WITH_MBEDTLS) || (defined(OPENSSL_VERSION_NUMBER) && \
				  OPENSSL_VERSION_NUMBER >= 0x10002000L)
	const unsigned char *name = NULL;
	char cstr[10];
	unsigned len;

	lwsl_info("%s\n", __func__);

	if (!wsi->tls.ssl) {
		lwsl_err("%s: non-ssl\n", __func__);
		return 0;
	}

	SSL_get0_alpn_selected(wsi->tls.ssl, &name, &len);
	if (!len) {
		lwsl_info("no ALPN upgrade\n");
		return 0;
	}

	if (len > sizeof(cstr) - 1)
		len = sizeof(cstr) - 1;

	memcpy(cstr, name, len);
	cstr[len] = '\0';

	lwsl_info("%s: negotiated '%s' using ALPN\n", __func__, cstr);
	wsi->tls.use_ssl |= LCCSCF_USE_SSL;

	return lws_role_call_alpn_negotiated(wsi, (const char *)cstr);
#else
	lwsl_err("%s: openssl too old\n", __func__);
#endif // OPENSSL_VERSION_NUMBER >= 0x10002000L

	return 0;
}
#endif

#if !defined(LWS_PLAT_OPTEE) && !defined(OPTEE_DEV_KIT)
#if defined(LWS_PLAT_FREERTOS) && !defined(LWS_AMAZON_RTOS)
int alloc_file(struct lws_context *context, const char *filename, uint8_t **buf,
	       lws_filepos_t *amount)
{
	nvs_handle nvh;
	size_t s;
	int n = 0;

	ESP_ERROR_CHECK(nvs_open("lws-station", NVS_READWRITE, &nvh));
	if (nvs_get_blob(nvh, filename, NULL, &s) != ESP_OK) {
		n = 1;
		goto bail;
	}
	*buf = lws_malloc(s + 1, "alloc_file");
	if (!*buf) {
		n = 2;
		goto bail;
	}
	if (nvs_get_blob(nvh, filename, (char *)*buf, &s) != ESP_OK) {
		lws_free(*buf);
		n = 1;
		goto bail;
	}

	*amount = s;
	(*buf)[s] = '\0';

	lwsl_notice("%s: nvs: read %s, %d bytes\n", __func__, filename, (int)s);

bail:
	nvs_close(nvh);

	return n;
}
#else
int alloc_file(struct lws_context *context, const char *filename, uint8_t **buf,
		lws_filepos_t *amount)
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

	m = (ssize_t)ftell(f);
	if (m == -1l) {
		n = 1;
		goto bail;
	}
	s = (size_t)m;

	if (fseek(f, 0, SEEK_SET) != 0) {
		n = 1;
		goto bail;
	}

	*buf = lws_malloc(s + 1, "alloc_file");
	if (!*buf) {
		n = 2;
		goto bail;
	}

	if (fread(*buf, s, 1, f) != 1) {
		lws_free(*buf);
		n = 1;
		goto bail;
	}

	*amount = s;

bail:
	if (f)
		fclose(f);

	return n;

}
#endif

/*
 * filename: NULL means use buffer inbuf length inlen directly, otherwise
 *           load the file "filename" into an allocated buffer.
 *
 * Allocates a separate DER output buffer if inbuf / inlen are the input,
 * since the
 *
 * Contents may be PEM or DER: returns with buf pointing to DER and amount
 * set to the DER length.
 */

int
lws_tls_alloc_pem_to_der_file(struct lws_context *context, const char *filename,
			      const char *inbuf, lws_filepos_t inlen,
			      uint8_t **buf, lws_filepos_t *amount)
{
	uint8_t *pem = NULL, *p, *end, *opem;
	lws_filepos_t len;
	uint8_t *q;
	int n;

	if (filename) {
		n = alloc_file(context, filename, (uint8_t **)&pem, &len);
		if (n)
			return n;
	} else {
		pem = (uint8_t *)inbuf;
		len = inlen;
	}

	opem = p = pem;
	end = p + len;

	if (strncmp((char *)p, "-----", 5)) {

		/* take it as being already DER */

		pem = lws_malloc((size_t)inlen, "alloc_der");
		if (!pem)
			return 1;

		memcpy(pem, inbuf, (size_t)inlen);

		*buf = pem;
		*amount = inlen;

		return 0;
	}

	/* PEM -> DER */

	if (!filename) {
		/* we don't know if it's in const memory... alloc the output */
		pem = lws_malloc(((size_t)inlen * 3) / 4, "alloc_der");
		if (!pem) {
			lwsl_err("a\n");
			return 1;
		}


	} /* else overwrite the allocated, b64 input with decoded DER */

	/* trim the first line */

	p += 5;
	while (p < end && *p != '\n' && *p != '-')
		p++;

	if (*p != '-') {
		goto bail;
	}

	while (p < end && *p != '\n')
		p++;

	if (p >= end) {
		goto bail;
	}

	p++;

	/* trim the last line */

	q = (uint8_t *)end - 2;

	while (q > opem && *q != '\n')
		q--;

	if (*q != '\n')
		goto bail;

	/* we can't write into the input buffer for mem, since it may be in RO
	 * const segment
	 */
	if (filename)
		*q = '\0';

	n = lws_ptr_diff(q, p);
	if (n == -1) /* coverity */
		goto bail;
	*amount = (unsigned int)lws_b64_decode_string_len((char *)p, n,
					    (char *)pem, (int)(long long)len);
	*buf = (uint8_t *)pem;

	return 0;

bail:
	lws_free((uint8_t *)pem);

	return 4;
}


#endif

#if !defined(LWS_PLAT_FREERTOS) && !defined(LWS_PLAT_OPTEE) && !defined(OPTEE_DEV_KIT)


static int
lws_tls_extant(const char *name)
{
	/* it exists if we can open it... */
	int fd = open(name, O_RDONLY);
	char buf[1];
	ssize_t n;

	if (fd < 0)
		return 1;

	/* and we can read at least one byte out of it */
	n = read(fd, buf, 1);
	close(fd);

	return n != 1;
}
#endif
/*
 * Returns 0 if the filepath "name" exists and can be read from.
 *
 * In addition, if "name".upd exists, backup "name" to "name.old.1"
 * and rename "name".upd to "name" before reporting its existence.
 *
 * There are four situations and three results possible:
 *
 * 1) LWS_TLS_EXTANT_NO: There are no certs at all (we are waiting for them to
 *    be provisioned).  We also feel like this if we need privs we don't have
 *    any more to look in the directory.
 *
 * 2) There are provisioned certs written (xxx.upd) and we still have root
 *    privs... in this case we rename any existing cert to have a backup name
 *    and move the upd cert into place with the correct name.  This then becomes
 *    situation 4 for the caller.
 *
 * 3) LWS_TLS_EXTANT_ALTERNATIVE: There are provisioned certs written (xxx.upd)
 *    but we no longer have the privs needed to read or rename them.  In this
 *    case, indicate that the caller should use temp copies if any we do have
 *    rights to access.  This is normal after we have updated the cert.
 *
 *    But if we dropped privs, we can't detect the provisioned xxx.upd cert +
 *    key, because we can't see in the dir.  So we have to upgrade NO to
 *    ALTERNATIVE when we actually have the in-memory alternative.
 *
 * 4) LWS_TLS_EXTANT_YES: The certs are present with the correct name and we
 *    have the rights to read them.
 */

enum lws_tls_extant
lws_tls_use_any_upgrade_check_extant(const char *name)
{
#if !defined(LWS_PLAT_OPTEE) && !defined(LWS_AMAZON_RTOS)

	int n;

#if !defined(LWS_PLAT_FREERTOS)
	char buf[256];

	lws_snprintf(buf, sizeof(buf) - 1, "%s.upd", name);
	if (!lws_tls_extant(buf)) {
		/* ah there is an updated file... how about the desired file? */
		if (!lws_tls_extant(name)) {
			/* rename the desired file */
			for (n = 0; n < 50; n++) {
				lws_snprintf(buf, sizeof(buf) - 1,
					     "%s.old.%d", name, n);
				if (!rename(name, buf))
					break;
			}
			if (n == 50) {
				lwsl_notice("unable to rename %s\n", name);

				return LWS_TLS_EXTANT_ALTERNATIVE;
			}
			lws_snprintf(buf, sizeof(buf) - 1, "%s.upd", name);
		}
		/* desired file is out of the way, rename the updated file */
		if (rename(buf, name)) {
			lwsl_notice("unable to rename %s to %s\n", buf, name);

			return LWS_TLS_EXTANT_ALTERNATIVE;
		}
	}

	if (lws_tls_extant(name))
		return LWS_TLS_EXTANT_NO;
#else
	nvs_handle nvh;
	size_t s = 8192;

	if (nvs_open("lws-station", NVS_READWRITE, &nvh)) {
		lwsl_notice("%s: can't open nvs\n", __func__);
		return LWS_TLS_EXTANT_NO;
	}

	n = nvs_get_blob(nvh, name, NULL, &s);
	nvs_close(nvh);

	if (n)
		return LWS_TLS_EXTANT_NO;
#endif
#endif
	return LWS_TLS_EXTANT_YES;
}
