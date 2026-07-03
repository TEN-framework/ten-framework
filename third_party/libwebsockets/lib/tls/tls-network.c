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
 * fakes POLLIN on all tls guys with buffered rx
 *
 * returns nonzero if any tls guys had POLLIN faked
 */

int
lws_tls_fake_POLLIN_for_buffered(struct lws_context_per_thread *pt)
{
	int ret = 0;

	lws_start_foreach_dll_safe(struct lws_dll2 *, p, p1,
			lws_dll2_get_head(&pt->tls.dll_pending_tls_owner)) {
		struct lws *wsi = lws_container_of(p, struct lws,
						   tls.dll_pending_tls);

		if (wsi->position_in_fds_table >= 0) {

			pt->fds[wsi->position_in_fds_table].revents = (short)
				(pt->fds[wsi->position_in_fds_table].revents |
				 (pt->fds[wsi->position_in_fds_table].events & LWS_POLLIN));
			ret |= pt->fds[wsi->position_in_fds_table].revents & LWS_POLLIN;
		}

	} lws_end_foreach_dll_safe(p, p1);

	return !!ret;
}

void
__lws_ssl_remove_wsi_from_buffered_list(struct lws *wsi)
{
	lws_dll2_remove(&wsi->tls.dll_pending_tls);
}

void
lws_ssl_remove_wsi_from_buffered_list(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];

	lws_pt_lock(pt, __func__);
	__lws_ssl_remove_wsi_from_buffered_list(wsi);
	lws_pt_unlock(pt);
}

#if defined(LWS_WITH_SERVER)
int
lws_tls_check_cert_lifetime(struct lws_vhost *v)
{
	time_t now = (time_t)lws_now_secs(), life = 0;
	struct lws_acme_cert_aging_args caa;
	union lws_tls_cert_info_results ir;
	int n;

	if (v->tls.ssl_ctx && !v->tls.skipped_certs) {

		if (now < 1542933698) /* Nov 23 2018 00:42 UTC */
			/* our clock is wrong and we can't judge the certs */
			return -1;

		n = lws_tls_vhost_cert_info(v, LWS_TLS_CERT_INFO_VALIDITY_TO,
					    &ir, 0);
		if (n)
			return 1;

		life = (ir.time - now) / (24 * 3600);
		lwsl_vhost_notice(v, "   vhost %s: cert expiry: %dd", v->name,
			    (int)life);
	} else
		lwsl_vhost_info(v, "   vhost %s: no cert", v->name);

	memset(&caa, 0, sizeof(caa));
	caa.vh = v;
	lws_broadcast(&v->context->pt[0], LWS_CALLBACK_VHOST_CERT_AGING, (void *)&caa,
		      (size_t)(ssize_t)life);

	return 0;
}

int
lws_tls_check_all_cert_lifetimes(struct lws_context *context)
{
	struct lws_vhost *v = context->vhost_list;

	while (v) {
		if (lws_tls_check_cert_lifetime(v) < 0)
			return -1;
		v = v->vhost_next;
	}

	return 0;
}

/*
 * LWS_TLS_EXTANT_NO         : skip adding the cert
 * LWS_TLS_EXTANT_YES        : use the cert and private key paths normally
 * LWS_TLS_EXTANT_ALTERNATIVE: normal paths not usable, try alternate if poss
 */
enum lws_tls_extant
lws_tls_generic_cert_checks(struct lws_vhost *vhost, const char *cert,
			    const char *private_key)
{
	int n, m;

	/*
	 * The user code can choose to either pass the cert and
	 * key filepaths using the info members like this, or it can
	 * leave them NULL; force the vhost SSL_CTX init using the info
	 * options flag LWS_SERVER_OPTION_CREATE_VHOST_SSL_CTX; and
	 * set up the cert himself using the user callback
	 * LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS, which
	 * happened just above and has the vhost SSL_CTX * in the user
	 * parameter.
	 */

	if (!cert || !private_key)
		return LWS_TLS_EXTANT_NO;

	n = (int)lws_tls_use_any_upgrade_check_extant(cert);
	if (n == LWS_TLS_EXTANT_ALTERNATIVE)
		return LWS_TLS_EXTANT_ALTERNATIVE;
	m = (int)lws_tls_use_any_upgrade_check_extant(private_key);
	if (m == LWS_TLS_EXTANT_ALTERNATIVE)
		return LWS_TLS_EXTANT_ALTERNATIVE;

	if ((n == LWS_TLS_EXTANT_NO || m == LWS_TLS_EXTANT_NO) &&
	    (vhost->options & LWS_SERVER_OPTION_IGNORE_MISSING_CERT)) {
		lwsl_vhost_notice(vhost, "Ignoring missing %s or %s", cert, private_key);
		vhost->tls.skipped_certs = 1;

		return LWS_TLS_EXTANT_NO;
	}

	/*
	 * the cert + key exist
	 */

	return LWS_TLS_EXTANT_YES;
}

/*
 * update the cert for every vhost using the given path
 */

int
lws_tls_cert_updated(struct lws_context *context, const char *certpath,
		     const char *keypath,
		     const char *mem_cert, size_t len_mem_cert,
		     const char *mem_privkey, size_t len_mem_privkey)
{
	struct lws wsi;

	wsi.a.context = context;

	lws_start_foreach_ll(struct lws_vhost *, v, context->vhost_list) {
		wsi.a.vhost = v; /* not a real bound wsi */
		if (v->tls.alloc_cert_path && v->tls.key_path &&
		    !strcmp(v->tls.alloc_cert_path, certpath) &&
		    !strcmp(v->tls.key_path, keypath)) {
			lws_tls_server_certs_load(v, &wsi, certpath, keypath,
						  mem_cert, len_mem_cert,
						  mem_privkey, len_mem_privkey);

			if (v->tls.skipped_certs)
				lwsl_vhost_notice(v, "vhost %s: cert unset", v->name);
		}
	} lws_end_foreach_ll(v, vhost_next);

	return 0;
}

int
lws_gate_accepts(struct lws_context *context, int on)
{
	struct lws_vhost *v = context->vhost_list;

	lwsl_notice("%s: on = %d\n", __func__, on);

	if (context->tls_gate_accepts == (char)on)
		return 0;

	context->tls_gate_accepts = (char)on;

	while (v) {
		lws_start_foreach_dll(struct lws_dll2 *, d,
				      lws_dll2_get_head(&v->listen_wsi)) {
			struct lws *wsi = lws_container_of(d, struct lws,
							   listen_list);

			if (v->tls.use_ssl &&
			    lws_change_pollfd(wsi, on ? LWS_POLLIN : 0,
						   on ? 0 : LWS_POLLIN))
				lwsl_cx_notice(context, "Unable to set POLLIN %d", on);
		} lws_end_foreach_dll(d);

		v = v->vhost_next;
	}

	return 0;
}
#endif

/* comma-separated alpn list, like "h2,http/1.1" to openssl alpn format */

int
lws_alpn_comma_to_openssl(const char *comma, uint8_t *os, int len)
{
	uint8_t *oos = os, *plen = NULL;

	if (!comma)
		return 0;

	while (*comma && len > 2) {
		if (!plen && *comma == ' ') {
			comma++;
			continue;
		}
		if (!plen) {
			plen = os++;
			len--;
		}

		if (*comma == ',') {
			*plen = (uint8_t)lws_ptr_diff(os, plen + 1);
			plen = NULL;
			comma++;
		} else {
			*os++ = (uint8_t)*comma++;
			len--;
		}
	}

	if (plen)
		*plen = (uint8_t)lws_ptr_diff(os, plen + 1);

	*os = 0;

	return lws_ptr_diff(os, oos);
}



