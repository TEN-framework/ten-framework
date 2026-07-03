/*
 * LWA auth support for Secure Streams
 *
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2019 - 2020 Andy Green <andy@warmcat.com>
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

#include <private-lib-core.h>

typedef struct ss_api_amazon_auth {
	struct lws_ss_handle 	*ss;
	void			*opaque_data;
	/* ... application specific state ... */
	struct lejp_ctx		jctx;
	size_t			pos;
	int			expires_secs;
} ss_api_amazon_auth_t;

static const char * const lejp_tokens_lwa[] = {
	"access_token",
	"expires_in",
};

typedef enum {
	LSSPPT_ACCESS_TOKEN,
	LSSPPT_EXPIRES_IN,
} lejp_tokens_t;

enum {
	AUTH_IDX_LWA,
	AUTH_IDX_ROOT,
};

static void
lws_ss_sys_auth_api_amazon_com_kick(lws_sorted_usec_list_t *sul)
{
	struct lws_context *context = lws_container_of(sul, struct lws_context,
						       sul_api_amazon_com_kick);

	lws_state_transition_steps(&context->mgr_system,
				   LWS_SYSTATE_OPERATIONAL);
}

static void
lws_ss_sys_auth_api_amazon_com_renew(lws_sorted_usec_list_t *sul)
{
	struct lws_context *context = lws_container_of(sul, struct lws_context,
						       sul_api_amazon_com);

	lws_ss_sys_auth_api_amazon_com(context);
}

static signed char
auth_api_amazon_com_parser_cb(struct lejp_ctx *ctx, char reason)
{
	ss_api_amazon_auth_t *m = (ss_api_amazon_auth_t *)ctx->user;
	struct lws_context *context = (struct lws_context *)m->opaque_data;
	lws_system_blob_t *blob;

	if (!(reason & LEJP_FLAG_CB_IS_VALUE) || !ctx->path_match)
		return 0;

	switch (ctx->path_match - 1) {
	case LSSPPT_ACCESS_TOKEN:
		if (!ctx->npos)
			break;

		blob = lws_system_get_blob(context, LWS_SYSBLOB_TYPE_AUTH,
					   AUTH_IDX_LWA);
		if (!blob)
			return -1;

		if (lws_system_blob_heap_append(blob,
						(const uint8_t *)ctx->buf,
						ctx->npos)) {
			lwsl_err("%s: unable to store auth token\n", __func__);

			return -1;
		}
		break;
	case LSSPPT_EXPIRES_IN:
		m->expires_secs = atoi(ctx->buf);
		lws_sul_schedule(context, 0, &context->sul_api_amazon_com,
				 lws_ss_sys_auth_api_amazon_com_renew,
				 (lws_usec_t)m->expires_secs * LWS_US_PER_SEC);
		break;
	}

	return 0;
}

/* secure streams payload interface */

static lws_ss_state_return_t
ss_api_amazon_auth_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	ss_api_amazon_auth_t *m = (ss_api_amazon_auth_t *)userobj;
	struct lws_context *context = (struct lws_context *)m->opaque_data;
	lws_system_blob_t *ab;
#if !defined(LWS_WITH_NO_LOGS)
	size_t total;
#endif
	int n;

	ab = lws_system_get_blob(context, LWS_SYSBLOB_TYPE_AUTH, AUTH_IDX_LWA);
	/* coverity */
	if (!ab)
		return LWSSSSRET_DISCONNECT_ME;

	if (buf) {
		if (flags & LWSSS_FLAG_SOM) {
			lejp_construct(&m->jctx, auth_api_amazon_com_parser_cb,
				       m, lejp_tokens_lwa,
				       LWS_ARRAY_SIZE(lejp_tokens_lwa));
			lws_system_blob_heap_empty(ab);
		}

		n = lejp_parse(&m->jctx, buf, (int)len);
		if (n < 0) {
			lejp_destruct(&m->jctx);
			lws_system_blob_destroy(
				lws_system_get_blob(context,
						    LWS_SYSBLOB_TYPE_AUTH,
						    AUTH_IDX_LWA));

			return LWSSSSRET_DISCONNECT_ME;
		}
	}
	if (!(flags & LWSSS_FLAG_EOM))
		return LWSSSSRET_OK;

	/* we should have the auth token now */

#if !defined(LWS_WITH_NO_LOGS)
	total = lws_system_blob_get_size(ab);
	lwsl_notice("%s: acquired %u-byte api.amazon.com auth token, exp %ds\n",
			__func__, (unsigned int)total, m->expires_secs);
#endif

	lejp_destruct(&m->jctx);

	/* we move the system state at auth connection close */

	return LWSSSSRET_DISCONNECT_ME;
}

static lws_ss_state_return_t
ss_api_amazon_auth_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf,
		      size_t *len, int *flags)
{
	ss_api_amazon_auth_t *m = (ss_api_amazon_auth_t *)userobj;
	struct lws_context *context = (struct lws_context *)m->opaque_data;
	lws_system_blob_t *ab;
	size_t total;
	int n;

	/*
	 * We send out auth slot AUTH_IDX_ROOT, it's the LWA user / device
	 * identity token
	 */

	ab = lws_system_get_blob(context, LWS_SYSBLOB_TYPE_AUTH, AUTH_IDX_ROOT);
	if (!ab)
		return LWSSSSRET_DESTROY_ME;

	total = lws_system_blob_get_size(ab);

	n = lws_system_blob_get(ab, buf, len, m->pos);
	if (n < 0)
		return LWSSSSRET_TX_DONT_SEND;

	if (!m->pos)
		*flags |= LWSSS_FLAG_SOM;

	m->pos += *len;

	if (m->pos == total) {
		*flags |= LWSSS_FLAG_EOM;
		m->pos = 0; /* for next time */
	}

	return LWSSSSRET_OK;
}

static lws_ss_state_return_t
ss_api_amazon_auth_state(void *userobj, void *sh, lws_ss_constate_t state,
			 lws_ss_tx_ordinal_t ack)
{
	ss_api_amazon_auth_t *m = (ss_api_amazon_auth_t *)userobj;
	struct lws_context *context = (struct lws_context *)m->opaque_data;
	lws_system_blob_t *ab;
	size_t s;

	lwsl_info("%s: %s, ord 0x%x\n", __func__, lws_ss_state_name((int)state),
		  (unsigned int)ack);

	ab = lws_system_get_blob(context, LWS_SYSBLOB_TYPE_AUTH, AUTH_IDX_ROOT);
	/* coverity */
	if (!ab)
		return LWSSSSRET_DESTROY_ME;

	switch (state) {
	case LWSSSCS_CREATING:
		//if (lws_ss_set_metadata(m->ss, "ctype", "application/json", 16))
		//	return LWSSSSRET_DESTROY_ME;
		/* fallthru */
	case LWSSSCS_CONNECTING:
		s = lws_system_blob_get_size(ab);
		if (!s)
			lwsl_debug("%s: no auth blob\n", __func__);
		m->pos = 0;
		return lws_ss_request_tx_len(m->ss, (unsigned long)s);

	case LWSSSCS_DISCONNECTED:
		/*
		 * We defer moving the system state forward until we have
		 * closed our connection + tls for the auth action... this is
		 * because on small systems, we need that memory recovered
		 * before we can make another connection subsequently.
		 *
		 * At this point, we're ultimately being called from within
		 * the wsi close process, the tls tunnel is not freed yet.
		 * Use a sul to actually do it next time around the event loop
		 * when the close process for the auth wsi has completed and
		 * the related tls is already freed.
		 */
		s = lws_system_blob_get_size(ab);

		if (s && context->mgr_system.state != LWS_SYSTATE_OPERATIONAL)
			lws_sul_schedule(context, 0,
					 &context->sul_api_amazon_com_kick,
					 lws_ss_sys_auth_api_amazon_com_kick, 1);

		context->hss_auth = NULL;
		return LWSSSSRET_DESTROY_ME;

	default:
		break;
	}

	return LWSSSSRET_OK;
}

int
lws_ss_sys_auth_api_amazon_com(struct lws_context *context)
{
	lws_ss_info_t ssi;

	if (context->hss_auth) /* already exists */
		return 0;

	/* We're making an outgoing secure stream ourselves */

	memset(&ssi, 0, sizeof(ssi));
	ssi.handle_offset	    = offsetof(ss_api_amazon_auth_t, ss);
	ssi.opaque_user_data_offset = offsetof(ss_api_amazon_auth_t, opaque_data);
	ssi.rx			    = ss_api_amazon_auth_rx;
	ssi.tx			    = ss_api_amazon_auth_tx;
	ssi.state		    = ss_api_amazon_auth_state;
	ssi.user_alloc		    = sizeof(ss_api_amazon_auth_t);
	ssi.streamtype		    = "api_amazon_com_auth";

	if (lws_ss_create(context, 0, &ssi, context, &context->hss_auth,
			  NULL, NULL)) {
		lwsl_info("%s: Create LWA auth ss failed (policy?)\n", __func__);
		return 1;
	}

	return 0;
}
