/*
 * Policy fetching for Secure Streams
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

typedef struct ss_fetch_policy {
	struct lws_ss_handle 	*ss;
	void			*opaque_data;
	/* ... application specific state ... */

	lws_sorted_usec_list_t	sul;

	uint8_t			partway;
} ss_fetch_policy_t;

/* secure streams payload interface */

static lws_ss_state_return_t
ss_fetch_policy_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	ss_fetch_policy_t *m = (ss_fetch_policy_t *)userobj;
	struct lws_context *context = (struct lws_context *)m->opaque_data;

	if (flags & LWSSS_FLAG_SOM) {
		if (lws_ss_policy_parse_begin(context, 0))
			return LWSSSSRET_OK;
		m->partway = 1;
	}

	if (len && lws_ss_policy_parse(context, buf, len) < 0)
		return LWSSSSRET_OK;

	if (flags & LWSSS_FLAG_EOM)
		m->partway = 2;

	return LWSSSSRET_OK;
}

static lws_ss_state_return_t
ss_fetch_policy_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf,
		   size_t *len, int *flags)
{
	return LWSSSSRET_TX_DONT_SEND;
}

static void
policy_set(lws_sorted_usec_list_t *sul)
{
	ss_fetch_policy_t *m = lws_container_of(sul, ss_fetch_policy_t, sul);
	struct lws_context *context = (struct lws_context *)m->opaque_data;

	/*
	 * We get called if the policy parse was successful, just after the
	 * ss connection close that was using the vhost from the old policy
	 */

	lws_ss_destroy(&m->ss);

	if (lws_ss_policy_set(context, "updated"))
		lwsl_err("%s: policy set failed\n", __func__);
	else {
		context->policy_updated = 1;
#if defined(LWS_WITH_SYS_STATE)
		lws_state_transition_steps(&context->mgr_system,
					   LWS_SYSTATE_OPERATIONAL);
#endif
	}
}

static lws_ss_state_return_t
ss_fetch_policy_state(void *userobj, void *sh, lws_ss_constate_t state,
		      lws_ss_tx_ordinal_t ack)
{
	ss_fetch_policy_t *m = (ss_fetch_policy_t *)userobj;
	struct lws_context *context = (struct lws_context *)m->opaque_data;

	lwsl_info("%s: %s, ord 0x%x\n", __func__, lws_ss_state_name((int)state),
		  (unsigned int)ack);

	switch (state) {
	case LWSSSCS_CREATING:
		return lws_ss_request_tx(m->ss);

	case LWSSSCS_CONNECTING:
		break;

	case LWSSSCS_QOS_ACK_REMOTE:
		switch (m->partway) {
		case 2:
			lws_sul_schedule(context, 0, &m->sul, policy_set, 1);
			m->partway = 0;
			break;
		}
		break;

	case LWSSSCS_DISCONNECTED:
		if (m->partway == 1) {
			lws_ss_policy_parse_abandon(context);
			break;
		}
		m->partway = 0;
		break;

	default:
		break;
	}

	return LWSSSSRET_OK;
}

int
lws_ss_sys_fetch_policy(struct lws_context *context)
{
	lws_ss_info_t ssi;

	if (context->hss_fetch_policy) /* already exists */
		return 0;

	/* We're making an outgoing secure stream ourselves */

	memset(&ssi, 0, sizeof(ssi));
	ssi.handle_offset	    = offsetof(ss_fetch_policy_t, ss);
	ssi.opaque_user_data_offset = offsetof(ss_fetch_policy_t, opaque_data);
	ssi.rx			    = ss_fetch_policy_rx;
	ssi.tx			    = ss_fetch_policy_tx;
	ssi.state		    = ss_fetch_policy_state;
	ssi.user_alloc		    = sizeof(ss_fetch_policy_t);
	ssi.streamtype		    = "fetch_policy";

	if (lws_ss_create(context, 0, &ssi, context, &context->hss_fetch_policy,
			  NULL, NULL)) {
		/*
		 * If there's no fetch_policy streamtype, it can just be we're
		 * running on a proxied client with no policy of its own,
		 * it's OK.
		 */
		lwsl_info("%s: Policy fetch ss failed (stub policy?)\n", __func__);

		return 0;
	}

	lwsl_info("%s: policy fetching ongoing\n", __func__);

	/* fetching it is ongoing */

	return 1;
}
