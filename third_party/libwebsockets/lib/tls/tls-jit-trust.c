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

void
lws_tls_kid_copy(union lws_tls_cert_info_results *ci, lws_tls_kid_t *kid)
{

	/*
	 * KIDs all seem to be 20 bytes / SHA1 or less.  If we get one that
	 * is bigger, treat only the first 20 bytes as significant.
	 */

	if ((size_t)ci->ns.len > sizeof(kid->kid))
		kid->kid_len = sizeof(kid->kid);
	else
		kid->kid_len = (uint8_t)ci->ns.len;

	memcpy(kid->kid, ci->ns.name, kid->kid_len);
}

void
lws_tls_kid_copy_kid(lws_tls_kid_t *kid, const lws_tls_kid_t *src)
{
	int klen = sizeof(kid->kid);

	if (src->kid_len < klen)
		klen = src->kid_len;

	kid->kid_len = (uint8_t)klen;

	memcpy(kid->kid, src->kid, (size_t)klen);
}

int
lws_tls_kid_cmp(const lws_tls_kid_t *a, const lws_tls_kid_t *b)
{
	if (a->kid_len != b->kid_len)
		return 1;

	return memcmp(a->kid, b->kid, a->kid_len);
}

/*
 * We have the SKID and AKID for every peer cert captured, but they may be
 * in any order, and eg, falsely have sent the root CA, or an attacker may
 * send unresolveable self-referencing loops of KIDs.
 *
 * Let's sort them into the SKID -> AKID hierarchy, so the last entry is the
 * server cert and the first entry is the highest parent that the server sent.
 * Normally the top one will be an intermediate, and its AKID is the ID of the
 * root CA cert we would need to trust to validate the chain.
 *
 * It's not unknown the server is misconfigured to also send the root CA, if so
 * the top slot's AKID is empty and we should look for its SKID in the trust
 * blob.
 *
 * If we return 0, we succeeded and the AKID of ch[0] is the SKID we want to see
 * try to import from the trust blob.
 *
 * If we return nonzero, we can't identify what we want and should abandon the
 * connection.
 */

int
lws_tls_jit_trust_sort_kids(struct lws *wsi, lws_tls_kid_chain_t *ch)
{
	size_t hl;
	lws_tls_jit_inflight_t *inf;
	int n, m, sanity = 10;
	const char *host = wsi->cli_hostname_copy;
	char more = 1;

	lwsl_info("%s\n", __func__);

	if (!host) {
		if (wsi->stash && wsi->stash->cis[CIS_HOST])
			host = wsi->stash->cis[CIS_HOST];
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
		else
			host = lws_hdr_simple_ptr(wsi,
					      _WSI_TOKEN_CLIENT_PEER_ADDRESS);
	}
#endif
	if (!host)
		return 1;

	hl = strlen(host);

	/* something to work with? */

	if (!ch->count)
		return 1;

	/* do we need to sort? */

	if (ch->count > 1) {

		/* okie... */

		while (more) {

			if (!sanity--)
				/* let's not get fooled into spinning */
				return 1;

			more = 0;
			for (n = 0; n < ch->count - 1; n++) {

				if (!lws_tls_kid_cmp(&ch->skid[n],
						     &ch->akid[n + 1]))
					/* next belongs with this one */
					continue;

				/*
				 * next doesn't belong with this one, let's
				 * try to figure out where this one does belong
				 * then
				 */

				for (m = 0; m < ch->count; m++) {
					if (n == m)
						continue;
					if (!lws_tls_kid_cmp(&ch->skid[n],
							     &ch->akid[m])) {
						lws_tls_kid_t t;

						/*
						 * m references us, so we
						 * need to go one step above m,
						 * swap m and n
						 */

						more = 1;
						t = ch->akid[m];
						ch->akid[m] = ch->akid[n];
						ch->akid[n] = t;
						t = ch->skid[m];
						ch->skid[m] = ch->skid[n];
						ch->skid[n] = t;

						break;
					}
				}

				if (more)
					break;
			}
		}

		/* then we should be sorted */
	}

	for (n = 0; n < ch->count; n++) {
		lwsl_info("%s: AKID[%d]\n", __func__, n);
		lwsl_hexdump_info(ch->akid[n].kid, ch->akid[n].kid_len);
		lwsl_info("%s: SKID[%d]\n", __func__, n);
		lwsl_hexdump_info(ch->skid[n].kid, ch->skid[n].kid_len);
	}

	/* to go further, user must provide a lookup helper */

	if (!wsi->a.context->system_ops ||
	    !wsi->a.context->system_ops->jit_trust_query)
		return 1;

	/*
	 * If there's already a pending lookup for this host, let's bail and
	 * just wait for that to complete (since it will be done async if we
	 * can see it)
	 */

	lws_start_foreach_dll(struct lws_dll2 *, d,
			      wsi->a.context->jit_inflight.head) {
		inf = lws_container_of(d, lws_tls_jit_inflight_t, list);

		if (!strcmp((const char *)&inf[1], host))
			/* already being handled */
			return 1;

	} lws_end_foreach_dll(d);

	/*
	 * No... let's make an inflight entry for this host, then
	 */

	inf = lws_zalloc(sizeof(*inf) + hl + 1, __func__);
	if (!inf)
		return 1;

	memcpy(&inf[1], host, hl + 1);
	inf->refcount = (char)ch->count;
	lws_dll2_add_tail(&inf->list, &wsi->a.context->jit_inflight);

	/*
	 * ...kid_chain[0] AKID should indicate the right CA SKID that we want.
	 *
	 * Because of cross-signing, we check all of them and accept we may get
	 * multiple (the inflight accepts up to 2) CAs needed.
	 */

	for (n = 0; n < ch->count; n++)
		wsi->a.context->system_ops->jit_trust_query(wsi->a.context,
			ch->akid[n].kid, (size_t)ch->akid[n].kid_len,
			(void *)inf);

	return 0;
}

static void
tag_to_vh_name(char *result, size_t max, uint32_t tag)
{
	lws_snprintf(result, max, "jitt-%08X", tag);
}

int
lws_tls_jit_trust_vhost_bind(struct lws_context *cx, const char *address,
			     struct lws_vhost **pvh)
{
	lws_tls_jit_cache_item_t *ci, jci;
	lws_tls_jit_inflight_t *inf;
	char vhtag[32];
	size_t size;
	int n;

	if (lws_cache_item_get(cx->trust_cache, address, (const void **)&ci,
									&size))
		/*
		 * There's no cached info, we have to start from scratch on
		 * this one
		 */
		return 1;

	/* gotten cache item may be evicted by jit_trust_query */
	jci = *ci;

	/*
	 * We have some trust cache information for this host already, it tells
	 * us the trusted CA SKIDs we found before, and the xor tag used to name
	 * the vhost configured for these trust CAs in its SSL_CTX.
	 *
	 * Let's check first if the correct prepared vhost already exists, if
	 * so, we can just bind to that and go.
	 */

	tag_to_vh_name(vhtag, sizeof(vhtag), jci.xor_tag);

	*pvh = lws_get_vhost_by_name(cx, vhtag);
	if (*pvh) {
		lwsl_info("%s: %s -> existing %s\n", __func__, address, vhtag);
		/* hit, let's just use that then */
		return 0;
	}

	/*
	 * ... so, we know the SKIDs of the missing CAs, but we don't have the
	 * DERs for them, and so no configured vhost trusting them yet.  We have
	 * had the DERs at some point, but we can't afford to cache them, so
	 * we will have to get them again.
	 *
	 * Let's make an inflight for this, it will create the vhost when it
	 * completes.  If syncrhronous, then it will complete before we leave
	 * here, otherwise it will have a life of its own until all the
	 * queries use the cb to succeed or fail.
	 */

	size = strlen(address);
	inf = lws_zalloc(sizeof(*inf) + size + 1, __func__);
	if (!inf)
		return 1;

	memcpy(&inf[1], address, size + 1);
	inf->refcount = (char)jci.count_skids;
	lws_dll2_add_tail(&inf->list, &cx->jit_inflight);

	/*
	 * ...kid_chain[0] AKID should indicate the right CA SKID that we want.
	 *
	 * Because of cross-signing, we check all of them and accept we may get
	 * multiple (we can handle 3) CAs needed.
	 */

	for (n = 0; n < jci.count_skids; n++)
		cx->system_ops->jit_trust_query(cx, jci.skids[n].kid,
						(size_t)jci.skids[n].kid_len,
						(void *)inf);

	/* ... in case synchronous and it already finished the queries */

	*pvh = lws_get_vhost_by_name(cx, vhtag);
	if (*pvh) {
		/* hit, let's just use that then */
		lwsl_info("%s: bind to created vhost %s\n", __func__, vhtag);
		return 0;
	} else
		lwsl_err("%s: unable to bind to %s\n", __func__, vhtag);

	/* right now, nothing to offer */

	return 1;
}

void
lws_tls_jit_trust_inflight_destroy(lws_tls_jit_inflight_t *inf)
{
	int n;

	for (n = 0; n < inf->ders; n++)
		lws_free_set_NULL(inf->der[n]);
	lws_dll2_remove(&inf->list);

	lws_free(inf);
}

static int
inflight_destroy(struct lws_dll2 *d, void *user)
{
	lws_tls_jit_inflight_t *inf;

	inf = lws_container_of(d, lws_tls_jit_inflight_t, list);

	lws_tls_jit_trust_inflight_destroy(inf);

	return 0;
}

void
lws_tls_jit_trust_inflight_destroy_all(struct lws_context *cx)
{
	lws_dll2_foreach_safe(&cx->jit_inflight, cx, inflight_destroy);
}

static void
unref_vh_grace_cb(lws_sorted_usec_list_t *sul)
{
	struct lws_vhost *vh = lws_container_of(sul, struct lws_vhost,
						sul_unref);

	lwsl_info("%s: %s\n", __func__, vh->lc.gutag);

	lws_vhost_destroy(vh);
}

void
lws_tls_jit_trust_vh_start_grace(struct lws_vhost *vh)
{
	lwsl_info("%s: %s: unused, grace %dms\n", __func__, vh->lc.gutag,
			vh->context->vh_idle_grace_ms);
	lws_sul_schedule(vh->context, 0, &vh->sul_unref, unref_vh_grace_cb,
			 (lws_usec_t)vh->context->vh_idle_grace_ms *
								LWS_US_PER_MS);
}

#if defined(_DEBUG)
static void
lws_tls_jit_trust_cert_info(const uint8_t *der, size_t der_len)
{
	struct lws_x509_cert *x;
	union lws_tls_cert_info_results *u;
	char p = 0, buf[192 + sizeof(*u)];

	if (lws_x509_create(&x))
		return;

	if (!lws_x509_parse_from_pem(x, der, der_len)) {

		u = (union lws_tls_cert_info_results *)buf;

		if (!lws_x509_info(x, LWS_TLS_CERT_INFO_ISSUER_NAME, u, 192)) {
			lwsl_info("ISS: %s\n", u->ns.name);
			p = 1;
		}
		if (!lws_x509_info(x, LWS_TLS_CERT_INFO_COMMON_NAME, u, 192)) {
			lwsl_info("CN: %s\n", u->ns.name);
			p = 1;
		}

		if (!p) {
			lwsl_err("%s: unable to get any info\n", __func__);
			lwsl_hexdump_err(der, der_len);
		}
	} else
		lwsl_err("%s: unable to load DER\n", __func__);

	lws_x509_destroy(&x);
}
#endif

/*
 * This processes the JIT Trust lookup results independent of the tls backend.
 */

int
lws_tls_jit_trust_got_cert_cb(struct lws_context *cx, void *got_opaque,
			      const uint8_t *skid, size_t skid_len,
			      const uint8_t *der, size_t der_len)
{
	lws_tls_jit_inflight_t *inf = (lws_tls_jit_inflight_t *)got_opaque;
	struct lws_context_creation_info info;
	lws_tls_jit_cache_item_t jci;
	struct lws_vhost *v;
	char vhtag[20];
	char hit = 0;
	int n;

	/*
	 * Before anything else, check the inf is still valid.  In the low
	 * probability but possible case it was reallocated to be a different
	 * inflight, that may cause different CA certs to apply to a connection,
	 * but since mbedtls will then validate the server cert using the wrong
	 * trusted CA, it will just cause temporary conn fail.
	 */

	lws_start_foreach_dll(struct lws_dll2 *, e, cx->jit_inflight.head) {
		lws_tls_jit_inflight_t *i = lws_container_of(e,
						lws_tls_jit_inflight_t, list);
		if (i == inf) {
			hit = 1;
			break;
		}

	} lws_end_foreach_dll(e);

	if (!hit)
		/* inf has already gone */
		return 1;

	inf->refcount--;

	if (skid_len >= 4)
		inf->tag ^= *((uint32_t *)skid);

	if (der && inf->ders < (int)LWS_ARRAY_SIZE(inf->der) && inf->refcount) {
		/*
		 * We have a trusted CA, but more results coming... stash it
		 * in heap.
		 */

		inf->kid[inf->ders].kid_len = (uint8_t)((skid_len >
				     (uint8_t)sizeof(inf->kid[inf->ders].kid)) ?
				     sizeof(inf->kid[inf->ders].kid) : skid_len);
		memcpy(inf->kid[inf->ders].kid, skid,
		       inf->kid[inf->ders].kid_len);

		inf->der[inf->ders] = lws_malloc(der_len, __func__);
		if (!inf->der[inf->ders])
			return 1;
		memcpy(inf->der[inf->ders], der, der_len);
		inf->der_len[inf->ders] = (short)der_len;
		inf->ders++;

		return 0;
	}

	/*
	 * We accept up to three valid CA, and then end the inflight early.
	 * Any further pending results are dropped, since we got all we could
	 * use.  Up to two valid CA would be held in the inflight and the other
	 * provided in the params.
	 *
	 * If we did not already fill up the inflight, keep waiting for any
	 * others expected
	 */

	if (inf->refcount && inf->ders < (int)LWS_ARRAY_SIZE(inf->der))
		return 0;

	if (!der && !inf->ders) {
		lwsl_warn("%s: no trusted CA certs matching\n", __func__);

		goto destroy_inf;
	}

	tag_to_vh_name(vhtag, sizeof(vhtag), inf->tag);

	/*
	 * We have got at least one CA, it's all the CAs we're going to get,
	 * or that we can handle.  So we have to process and drop the inf.
	 *
	 * First let's make a cache entry with a shortish ttl, mapping the
	 * hostname we were trying to connect to, to the SKIDs that actually
	 * had trust results.  This may come in handy later when we want to
	 * connect to the same host again, but any vhost from before has been
	 * removed... we can just ask for the specific CAs to regenerate the
	 * vhost, without having to first fail the connection attempt to get the
	 * server cert.
	 *
	 * The cache entry can be evicted at any time, so it is selfcontained.
	 * If it's also lost, we start over with the initial failing connection
	 * to figure out what we need to make it work.
	 */

	memset(&jci, 0, sizeof(jci));

	jci.xor_tag = inf->tag;

	/* copy the SKIDs from the inflight and params into the cache item */

	for (n = 0; n < (int)LWS_ARRAY_SIZE(inf->der); n++)
		if (inf->kid[n].kid_len)
			lws_tls_kid_copy_kid(&jci.skids[jci.count_skids++],
						&inf->kid[n]);

	if (skid_len) {
		if (skid_len > sizeof(inf->kid[0].kid))
			skid_len = sizeof(inf->kid[0].kid);
		jci.skids[jci.count_skids].kid_len = (uint8_t)skid_len;
		memcpy(jci.skids[jci.count_skids++].kid, skid, skid_len);
	}

	lwsl_info("%s: adding cache mapping %s -> %s\n", __func__,
			(const char *)&inf[1], vhtag);

	if (lws_cache_write_through(cx->trust_cache, (const char *)&inf[1],
				    (const uint8_t *)&jci, sizeof(jci),
				    lws_now_usecs() + (3600ll *LWS_US_PER_SEC),
				    NULL))
		lwsl_warn("%s: add to cache failed\n", __func__);

	/* is there already a vhost for this commutative-xor SKID trust? */

	if (lws_get_vhost_by_name(cx, vhtag)) {
		lwsl_info("%s: tag vhost %s already exists, skipping\n",
				__func__, vhtag);
		goto destroy_inf;
	}

	/*
	 * We only end up here when we attempted a connection to this hostname.
	 *
	 * We have the identified CA trust DER(s) to hand, let's create the
	 * necessary vhost + prepared SSL_CTX for it to use on the retry, it
	 * will be used straight away if the retry comes before the idle vhost
	 * timeout.
	 *
	 * We also use this path in the case we have the cache entry but no
	 * matching vhost already existing, to create one.
	 */

	memset(&info, 0, sizeof(info));
	info.vhost_name = vhtag;
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.options = cx->options;

	/*
	 * We have to create the vhost with the first valid trusted DER...
	 * if we have a params one, use that so the rest are all from inflight
	 */

	if (der) {
		info.client_ssl_ca_mem = der;
		info.client_ssl_ca_mem_len = (unsigned int)der_len;
		n = 0;
	} else {
		info.client_ssl_ca_mem = inf->der[0];
		info.client_ssl_ca_mem_len = (unsigned int)inf->der_len[0];
		n = 1;
	}

#if defined(_DEBUG)
	lws_tls_jit_trust_cert_info(info.client_ssl_ca_mem,
				    info.client_ssl_ca_mem_len);
#endif

	info.protocols = cx->protocols_copy;

	v = lws_create_vhost(cx, &info);
	if (!v)
		lwsl_err("%s: failed to create vh %s\n", __func__, vhtag);

	v->grace_after_unref = 1;
	lws_tls_jit_trust_vh_start_grace(v);

	/*
	 * Do we need to add more trusted certs from inflight?
	 */

	while (n < inf->ders) {

#if defined(_DEBUG)
		lws_tls_jit_trust_cert_info(inf->der[n],
					    (size_t)inf->der_len[n]);
#endif

		if (lws_tls_client_vhost_extra_cert_mem(v, inf->der[n],
						(size_t)inf->der_len[n]))
			lwsl_err("%s: add extra cert failed\n", __func__);
		n++;
	}

	lwsl_info("%s: created jitt %s -> vh %s\n", __func__,
				(const char *)&inf[1], vhtag);

destroy_inf:
	lws_tls_jit_trust_inflight_destroy(inf);

	return 0;
}

/*
 * Refer to ./READMEs/README.jit-trust.md for blob layout specification
 */

int
lws_tls_jit_trust_blob_queury_skid(const void *_blob, size_t blen,
				   const uint8_t *skid, size_t skid_len,
				   const uint8_t **prpder, size_t *prder_len)
{
	const uint8_t *pskidlen, *pskids, *pder, *blob = (uint8_t *)_blob;
	const uint16_t *pderlen;
	int certs;

	/* sanity check blob length and magic */

	if (blen < 32768 ||
	   lws_ser_ru32be(blob) != LWS_JIT_TRUST_MAGIC_BE ||
	   lws_ser_ru32be(blob + LJT_OFS_END) != blen) {
		lwsl_err("%s: blob not sane\n", __func__);

		return -1;
	}

	if (!skid_len)
		return 1;

	/* point into the various sub-tables */

	certs		= (int)lws_ser_ru16be(blob + LJT_OFS_32_COUNT_CERTS);

	pderlen		= (uint16_t *)(blob + lws_ser_ru32be(blob +
							LJT_OFS_32_DERLEN));
	pskidlen	= blob + lws_ser_ru32be(blob + LJT_OFS_32_SKIDLEN);
	pskids		= blob + lws_ser_ru32be(blob + LJT_OFS_32_SKID);
	pder		= blob + LJT_OFS_DER;

	/* check each cert SKID in turn, return the DER if found */

	while (certs--) {

		/* paranoia / sanity */

		assert(pskids < blob + blen);
		assert(pder < blob + blen);
		assert(pskidlen < blob + blen);
		assert((uint8_t *)pderlen < blob + blen);

		/* we will accept to match on truncated SKIDs */

		if (*pskidlen >= skid_len &&
		    !memcmp(skid, pskids, skid_len)) {
			/*
			 * We found a trusted CA cert of the right SKID
			 */
		        *prpder = pder;
		        *prder_len = lws_ser_ru16be((uint8_t *)pderlen);

		        return 0;
		}

		pder += lws_ser_ru16be((uint8_t *)pderlen);
		pskids += *pskidlen;
		pderlen++;
		pskidlen++;
	}

	return 1;
}
