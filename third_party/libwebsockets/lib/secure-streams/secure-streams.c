/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2019 - 2021 Andy Green <andy@warmcat.com>
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

static const struct ss_pcols *ss_pcols[] = {
#if defined(LWS_ROLE_H1)
	&ss_pcol_h1,		/* LWSSSP_H1 */
#else
	NULL,
#endif
#if defined(LWS_ROLE_H2)
	&ss_pcol_h2,		/* LWSSSP_H2 */
#else
	NULL,
#endif
#if defined(LWS_ROLE_WS)
	&ss_pcol_ws,		/* LWSSSP_WS */
#else
	NULL,
#endif
#if defined(LWS_ROLE_MQTT)
	&ss_pcol_mqtt,		/* LWSSSP_MQTT */
#else
	NULL,
#endif
	&ss_pcol_raw,		/* LWSSSP_RAW */
	NULL,
};

static const char *state_names[] = {
	"(unset)",
	"LWSSSCS_CREATING",
	"LWSSSCS_DISCONNECTED",
	"LWSSSCS_UNREACHABLE",
	"LWSSSCS_AUTH_FAILED",
	"LWSSSCS_CONNECTED",
	"LWSSSCS_CONNECTING",
	"LWSSSCS_DESTROYING",
	"LWSSSCS_POLL",
	"LWSSSCS_ALL_RETRIES_FAILED",
	"LWSSSCS_QOS_ACK_REMOTE",
	"LWSSSCS_QOS_NACK_REMOTE",
	"LWSSSCS_QOS_ACK_LOCAL",
	"LWSSSCS_QOS_NACK_LOCAL",
	"LWSSSCS_TIMEOUT",
	"LWSSSCS_SERVER_TXN",
	"LWSSSCS_SERVER_UPGRADE",
	"LWSSSCS_EVENT_WAIT_CANCELLED",
	"LWSSSCS_UPSTREAM_LINK_RETRY",
};

/*
 * For each "current state", set bit offsets for valid "next states".
 *
 * Since there are complicated ways to arrive at state transitions like proxying
 * and asynchronous destruction etc, so we monitor the state transitions we are
 * giving the ss user code to ensure we never deliver illegal state transitions
 * (because we will assert if we have bugs that do it)
 */

const uint32_t ss_state_txn_validity[] = {

	/* if we was last in this state...  we can legally go to these states */

	[0]				= (1 << LWSSSCS_CREATING) |
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_CREATING]		= (1 << LWSSSCS_CONNECTING) |
					  (1 << LWSSSCS_TIMEOUT) |
					  (1 << LWSSSCS_POLL) |
					  (1 << LWSSSCS_SERVER_UPGRADE) |
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_DISCONNECTED]		= (1 << LWSSSCS_CONNECTING) |
					  (1 << LWSSSCS_TIMEOUT) |
					  (1 << LWSSSCS_POLL) |
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_UNREACHABLE]		= (1 << LWSSSCS_ALL_RETRIES_FAILED) |
					  (1 << LWSSSCS_TIMEOUT) |
					  (1 << LWSSSCS_POLL) |
					  (1 << LWSSSCS_CONNECTING) |
					  /* win conn failure > retry > succ */
					  (1 << LWSSSCS_CONNECTED) |
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_AUTH_FAILED]		= (1 << LWSSSCS_ALL_RETRIES_FAILED) |
					  (1 << LWSSSCS_TIMEOUT) |
					  (1 << LWSSSCS_CONNECTING) |
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_CONNECTED]		= (1 << LWSSSCS_SERVER_UPGRADE) |
					  (1 << LWSSSCS_SERVER_TXN) |
					  (1 << LWSSSCS_AUTH_FAILED) |
					  (1 << LWSSSCS_QOS_ACK_REMOTE) |
					  (1 << LWSSSCS_QOS_NACK_REMOTE) |
					  (1 << LWSSSCS_QOS_ACK_LOCAL) |
					  (1 << LWSSSCS_QOS_NACK_LOCAL) |
					  (1 << LWSSSCS_DISCONNECTED) |
					  (1 << LWSSSCS_TIMEOUT) |
					  (1 << LWSSSCS_POLL) | /* proxy retry */
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_CONNECTING]		= (1 << LWSSSCS_UNREACHABLE) |
					  (1 << LWSSSCS_AUTH_FAILED) |
					  (1 << LWSSSCS_CONNECTING) |
					  (1 << LWSSSCS_CONNECTED) |
					  (1 << LWSSSCS_TIMEOUT) |
					  (1 << LWSSSCS_POLL) |
					  (1 << LWSSSCS_DISCONNECTED) | /* proxy retry */
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_DESTROYING]		= 0,

	[LWSSSCS_POLL]			= (1 << LWSSSCS_CONNECTING) |
					  (1 << LWSSSCS_TIMEOUT) |
					  (1 << LWSSSCS_ALL_RETRIES_FAILED) |
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_ALL_RETRIES_FAILED]	= (1 << LWSSSCS_CONNECTING) |
					  (1 << LWSSSCS_TIMEOUT) |
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_QOS_ACK_REMOTE]	= (1 << LWSSSCS_DISCONNECTED) |
					  (1 << LWSSSCS_TIMEOUT) |
#if defined(LWS_ROLE_MQTT)
					  (1 << LWSSSCS_QOS_ACK_REMOTE) |
					  (1 << LWSSSCS_QOS_NACK_REMOTE) |
#endif
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_QOS_NACK_REMOTE]	= (1 << LWSSSCS_DISCONNECTED) |
					  (1 << LWSSSCS_TIMEOUT) |
#if defined(LWS_ROLE_MQTT)
					  (1 << LWSSSCS_QOS_ACK_REMOTE) |
					  (1 << LWSSSCS_QOS_NACK_REMOTE) |
#endif
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_QOS_ACK_LOCAL]		= (1 << LWSSSCS_DISCONNECTED) |
					  (1 << LWSSSCS_TIMEOUT) |
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_QOS_NACK_LOCAL]	= (1 << LWSSSCS_DESTROYING) |
					  (1 << LWSSSCS_TIMEOUT),

	/* he can get the timeout at any point and take no action... */
	[LWSSSCS_TIMEOUT]		= (1 << LWSSSCS_CONNECTING) |
					  (1 << LWSSSCS_CONNECTED) |
					  (1 << LWSSSCS_QOS_ACK_REMOTE) |
					  (1 << LWSSSCS_QOS_NACK_REMOTE) |
					  (1 << LWSSSCS_POLL) |
					  (1 << LWSSSCS_TIMEOUT) |
					  (1 << LWSSSCS_DISCONNECTED) |
					  (1 << LWSSSCS_UNREACHABLE) |
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_SERVER_TXN]		= (1 << LWSSSCS_DISCONNECTED) |
					  (1 << LWSSSCS_TIMEOUT) |
					  (1 << LWSSSCS_DESTROYING),

	[LWSSSCS_SERVER_UPGRADE]	= (1 << LWSSSCS_SERVER_TXN) |
					  (1 << LWSSSCS_TIMEOUT) |
					  (1 << LWSSSCS_DISCONNECTED) |
					  (1 << LWSSSCS_DESTROYING),
};

#if defined(LWS_WITH_CONMON)

/*
 * Convert any conmon data to JSON and attach to the ss handle.
 */

lws_ss_state_return_t
lws_conmon_ss_json(lws_ss_handle_t *h)
{
	char ads[48], *end, *buf, *obuf;
	const struct addrinfo *ai;
	lws_ss_state_return_t ret = LWSSSSRET_OK;
	struct lws_conmon cm;
	size_t len = 500;

	if (!h->policy || !(h->policy->flags & LWSSSPOLF_PERF) || !h->wsi ||
	    h->wsi->perf_done)
		return LWSSSSRET_OK;

	if (h->conmon_json)
		lws_free_set_NULL(h->conmon_json);

	h->conmon_json = lws_malloc(len, __func__);
	if (!h->conmon_json)
		return LWSSSSRET_OK;

	obuf = buf = h->conmon_json;
	end = buf + len - 1;

	lws_conmon_wsi_take(h->wsi, &cm);

	lws_sa46_write_numeric_address(&cm.peer46, ads, sizeof(ads));
	buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf),
		     "{\"peer\":\"%s\","
		      "\"dns_us\":%u,"
		      "\"dns_disp\":%u,"
		      "\"sockconn_us\":%u,"
		      "\"tls_us\":%u,"
		      "\"txn_resp_us\":%u,"
		      "\"dns\":[",
		    ads,
		    (unsigned int)cm.ciu_dns,
		    (unsigned int)cm.dns_disposition,
		    (unsigned int)cm.ciu_sockconn,
		    (unsigned int)cm.ciu_tls,
		    (unsigned int)cm.ciu_txn_resp);

	ai = cm.dns_results_copy;
	while (ai) {
		lws_sa46_write_numeric_address((lws_sockaddr46 *)ai->ai_addr, ads, sizeof(ads));
		buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf), "\"%s\"", ads);
		if (ai->ai_next && buf < end - 2)
			*buf++ = ',';
		ai = ai->ai_next;
	}

	buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf), "]");

	switch (cm.pcol) {
	case LWSCONMON_PCOL_HTTP:
		buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf),
			   ",\"prot_specific\":{\"protocol\":\"http\",\"resp\":%u}",
			   (unsigned int)cm.protocol_specific.http.response);
		break;
	default:
		break;
	}

	buf += lws_snprintf(buf, lws_ptr_diff_size_t(end, buf), "}");

	/*
	 * This destroys the DNS list in the lws_conmon that we took
	 * responsibility for when we used lws_conmon_wsi_take()
	 */

	lws_conmon_release(&cm);

	h->conmon_len = (uint16_t)lws_ptr_diff(buf, obuf);

#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
	if (h->proxy_onward) {

		/*
		 * ask to forward it on the proxy link
		 */

		ss_proxy_onward_link_req_writeable(h);
		return LWSSSSRET_OK;
	}
#endif

	/*
	 * We can deliver it directly
	 */

	if (h->info.rx)
		ret = h->info.rx(ss_to_userobj(h), (uint8_t *)h->conmon_json,
				 (unsigned int)h->conmon_len,
				 (int)(LWSSS_FLAG_SOM | LWSSS_FLAG_EOM |
						 LWSSS_FLAG_PERF_JSON));

	lws_free_set_NULL(h->conmon_json);

	return ret;
}
#endif

int
lws_ss_check_next_state(lws_lifecycle_t *lc, uint8_t *prevstate,
			lws_ss_constate_t cs)
{
	if (cs >= LWSSSCS_USER_BASE ||
	    cs == LWSSSCS_EVENT_WAIT_CANCELLED ||
	    cs == LWSSSCS_SERVER_TXN ||
	    cs == LWSSSCS_UPSTREAM_LINK_RETRY)
		/*
		 * we can't judge user or transient states, leave the old state
		 * and just wave them through
		 */
		return 0;

	if (cs >= LWS_ARRAY_SIZE(ss_state_txn_validity)) {
		/* we don't recognize this state as usable */
		lwsl_err("%s: %s: bad new state %u\n", __func__, lc->gutag, cs);
		assert(0);
		return 1;
	}

	if (*prevstate >= LWS_ARRAY_SIZE(ss_state_txn_validity)) {
		/* existing state is broken */
		lwsl_err("%s: %s: bad existing state %u\n", __func__,
			 lc->gutag, (unsigned int)*prevstate);
		assert(0);
		return 1;
	}

	if (ss_state_txn_validity[*prevstate] & (1u << cs)) {

		lwsl_notice("%s: %s: %s -> %s\n", __func__, lc->gutag,
			    lws_ss_state_name((int)*prevstate),
			    lws_ss_state_name((int)cs));

		/* this is explicitly allowed, update old state to new */
		*prevstate = (uint8_t)cs;

		return 0;
	}

	lwsl_err("%s: %s: transition from %s -> %s is illegal\n", __func__,
		 lc->gutag, lws_ss_state_name((int)*prevstate),
		 lws_ss_state_name((int)cs));

	assert(0);

	return 1;
}

int
lws_ss_check_next_state_ss(lws_ss_handle_t *ss, uint8_t *prevstate,
			   lws_ss_constate_t cs)
{
	if (cs >= LWSSSCS_USER_BASE ||
	    cs == LWSSSCS_EVENT_WAIT_CANCELLED ||
	    cs == LWSSSCS_UPSTREAM_LINK_RETRY)
		/*
		 * we can't judge user or transient states, leave the old state
		 * and just wave them through
		 */
		return 0;

	if (cs >= LWS_ARRAY_SIZE(ss_state_txn_validity)) {
		/* we don't recognize this state as usable */
		lwsl_ss_err(ss, "bad new state %u", cs);
		assert(0);
		return 1;
	}

	if (*prevstate >= LWS_ARRAY_SIZE(ss_state_txn_validity)) {
		/* existing state is broken */
		lwsl_ss_err(ss, "bad existing state %u",
				(unsigned int)*prevstate);
		assert(0);
		return 1;
	}

	if (ss_state_txn_validity[*prevstate] & (1u << cs)) {

		lwsl_ss_notice(ss, "%s -> %s",
			       lws_ss_state_name((int)*prevstate),
			       lws_ss_state_name((int)cs));

		/* this is explicitly allowed, update old state to new */
		*prevstate = (uint8_t)cs;

		return 0;
	}

	lwsl_ss_err(ss, "transition from %s -> %s is illegal",
		    lws_ss_state_name((int)*prevstate),
		    lws_ss_state_name((int)cs));

	assert(0);

	return 1;
}

const char *
lws_ss_state_name(int state)
{
	if (state >= LWSSSCS_USER_BASE)
		return "user state";

	if (state >= (int)LWS_ARRAY_SIZE(state_names))
		return "unknown";

	return state_names[state];
}

lws_ss_state_return_t
lws_ss_event_helper(lws_ss_handle_t *h, lws_ss_constate_t cs)
{
	lws_ss_state_return_t r;

	if (!h)
		return LWSSSSRET_OK;

	if (lws_ss_check_next_state_ss(h, &h->prev_ss_state, cs))
		return LWSSSSRET_DESTROY_ME;

	if (cs == LWSSSCS_CONNECTED)
		h->ss_dangling_connected = 1;
	if (cs == LWSSSCS_DISCONNECTED)
		h->ss_dangling_connected = 0;

#if defined(LWS_WITH_SEQUENCER)
	/*
	 * A parent sequencer for the ss is optional, if we have one, keep it
	 * informed of state changes on the ss connection
	 */
	if (h->seq && cs != LWSSSCS_DESTROYING)
		lws_seq_queue_event(h->seq, LWSSEQ_SS_STATE_BASE + cs,
				    (void *)h, NULL);
#endif

	if (h->info.state) {
		h->h_in_svc = h;
		r = h->info.state(ss_to_userobj(h), NULL, cs,
			cs == LWSSSCS_UNREACHABLE &&
			h->wsi && h->wsi->dns_reachability);
		h->h_in_svc = NULL;
#if defined(LWS_WITH_SERVER)
		if ((h->info.flags & LWSSSINFLAGS_ACCEPTED) &&
		    cs == LWSSSCS_DISCONNECTED)
			r = LWSSSSRET_DESTROY_ME;
#endif
		return r;
	}

	return LWSSSSRET_OK;
}

int
_lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(lws_ss_state_return_t r, struct lws *wsi,
			 lws_ss_handle_t **ph)
{
	if (r == LWSSSSRET_DESTROY_ME) {
		lwsl_info("%s: DESTROY ME: %s, %s\n", __func__,
				lws_wsi_tag(wsi), lws_ss_tag(*ph));
		if (wsi) {
			lws_set_opaque_user_data(wsi, NULL);
			lws_set_timeout(wsi, 1, LWS_TO_KILL_ASYNC);
		} else {
			if ((*ph)->wsi) {
				lws_set_opaque_user_data((*ph)->wsi, NULL);
				lws_set_timeout((*ph)->wsi, 1, LWS_TO_KILL_ASYNC);
			}
		}
		(*ph)->wsi = NULL;
		lws_ss_destroy(ph);
	}

	return -1; /* close connection */
}

static void
lws_ss_timeout_sul_check_cb(lws_sorted_usec_list_t *sul)
{
	lws_ss_state_return_t r;
	lws_ss_handle_t *h = lws_container_of(sul, lws_ss_handle_t, sul);

	lwsl_info("%s: retrying %s after backoff\n", __func__, lws_ss_tag(h));
	/* we want to retry... */
	h->seqstate = SSSEQ_DO_RETRY;

	r = _lws_ss_request_tx(h);
	_lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, NULL, &h);
}

int
lws_ss_exp_cb_metadata(void *priv, const char *name, char *out, size_t *pos,
			size_t olen, size_t *exp_ofs)
{
	lws_ss_handle_t *h = (lws_ss_handle_t *)priv;
	const char *replace = NULL;
	size_t total, budget;
	lws_ss_metadata_t *md = lws_ss_policy_metadata(h->policy, name),
			  *hmd = lws_ss_get_handle_metadata(h, name);

	if (!md) {
		lwsl_err("%s: Unknown metadata %s\n", __func__, name);

		return LSTRX_FATAL_NAME_UNKNOWN;
	}

	if (!hmd)
		return LSTRX_FILLED_OUT;

	replace = hmd->value__may_own_heap;

	if (!replace)
		return LSTRX_DONE;

	total = hmd->length;

	budget = olen - *pos;
	total -= *exp_ofs;
	if (total < budget)
		budget = total;

	if (out)
		memcpy(out + *pos, replace + (*exp_ofs), budget);
	*exp_ofs += budget;
	*pos += budget;

	if (budget == total)
		return LSTRX_DONE;

	return LSTRX_FILLED_OUT;
}

int
lws_ss_set_timeout_us(lws_ss_handle_t *h, lws_usec_t us)
{
	struct lws_context_per_thread *pt = &h->context->pt[h->tsi];

	h->sul.cb = lws_ss_timeout_sul_check_cb;
	__lws_sul_insert_us(&pt->pt_sul_owner[
	            !!(h->policy->flags & LWSSSPOLF_WAKE_SUSPEND__VALIDITY)],
		    &h->sul, us);

	return 0;
}

lws_ss_state_return_t
_lws_ss_backoff(lws_ss_handle_t *h, lws_usec_t us_override)
{
	uint64_t ms;
	char conceal;

	lws_service_assert_loop_thread(h->context, h->tsi);

	if (h->seqstate == SSSEQ_RECONNECT_WAIT)
		return LWSSSSRET_OK;

	/* figure out what we should do about another retry */

	lwsl_info("%s: %s: retry backoff after failure\n", __func__, lws_ss_tag(h));
	ms = lws_retry_get_delay_ms(h->context, h->policy->retry_bo,
				    &h->retry, &conceal);
	if (!conceal) {
		lwsl_info("%s: %s: abandon conn attempt \n",__func__, lws_ss_tag(h));

		if (h->seqstate == SSSEQ_IDLE) /* been here? */
			return LWSSSSRET_OK;

		h->seqstate = SSSEQ_IDLE;

		return lws_ss_event_helper(h, LWSSSCS_ALL_RETRIES_FAILED);
	}

	/* Only increase our planned backoff, or go with it */

	if (us_override < (lws_usec_t)ms * LWS_US_PER_MS)
		us_override = (lws_usec_t)(ms * LWS_US_PER_MS);

	h->seqstate = SSSEQ_RECONNECT_WAIT;
	lws_ss_set_timeout_us(h, us_override);

	lwsl_info("%s: %s: retry wait %dms\n", __func__, lws_ss_tag(h),
						  (int)(us_override / 1000));

	return LWSSSSRET_OK;
}

lws_ss_state_return_t
lws_ss_backoff(lws_ss_handle_t *h)
{
	return _lws_ss_backoff(h, 0);
}

#if defined(LWS_WITH_SYS_SMD)

/*
 * Local SMD <-> SS
 *
 * We pass received messages through to the SS handler synchronously, using the
 * lws service thread context.
 *
 * After the SS is created and registered, still nothing is going to come here
 * until the peer sends us his rx_class_mask and we update his registration with
 * it, because from SS creation his rx_class_mask defaults to 0.
 */

static int
lws_smd_ss_cb(void *opaque, lws_smd_class_t _class,
	      lws_usec_t timestamp, void *buf, size_t len)
{
	lws_ss_handle_t *h = (lws_ss_handle_t *)opaque;
	uint8_t *p = (uint8_t *)buf - LWS_SMD_SS_RX_HEADER_LEN;

	lws_service_assert_loop_thread(h->context, h->tsi);

	/*
	 * When configured with SS enabled, lws over-allocates
	 * LWS_SMD_SS_RX_HEADER_LEN bytes behind the payload of the queued
	 * message, for prepending serialized class and timestamp data in-band
	 * with the payload.
	 */

	lws_ser_wu64be(p, _class);
	lws_ser_wu64be(p + 8, (uint64_t)timestamp);

	if (h->info.rx)
		h->info.rx((void *)(h + 1), p, len + LWS_SMD_SS_RX_HEADER_LEN,
		      LWSSS_FLAG_SOM | LWSSS_FLAG_EOM);

	return 0;
}

static void
lws_ss_smd_tx_cb(lws_sorted_usec_list_t *sul)
{
	lws_ss_handle_t *h = lws_container_of(sul, lws_ss_handle_t, u.smd.sul_write);
	uint8_t buf[LWS_SMD_SS_RX_HEADER_LEN + LWS_SMD_MAX_PAYLOAD], *p;
	size_t len = sizeof(buf);
	lws_smd_class_t _class;
	int flags = 0, n;

	lws_service_assert_loop_thread(h->context, h->tsi);

	if (!h->info.tx)
		return;

	n = h->info.tx((h + 1), h->txord++, buf, &len, &flags);
	if (n)
		/* nonzero return means don't want to send anything */
		return;

	// lwsl_notice("%s: (SS %p bound to _lws_smd creates message) tx len %d\n", __func__, h, (int)len);
	// lwsl_hexdump_notice(buf, len);

	assert(len >= LWS_SMD_SS_RX_HEADER_LEN);
	_class = (lws_smd_class_t)lws_ser_ru64be(buf);
	p = lws_smd_msg_alloc(h->context, _class, len - LWS_SMD_SS_RX_HEADER_LEN);
	if (!p) {
		// this can be rejected if nobody listening for this class
		//lwsl_notice("%s: failed to alloc\n", __func__);
		return;
	}

	memcpy(p, buf + LWS_SMD_SS_RX_HEADER_LEN, len - LWS_SMD_SS_RX_HEADER_LEN);
	if (lws_smd_msg_send(h->context, p)) {
		lwsl_notice("%s: failed to queue\n", __func__);
		return;
	}
}

#endif

lws_ss_state_return_t
_lws_ss_client_connect(lws_ss_handle_t *h, int is_retry, void *conn_if_sspc_onw)
{
	const char *prot, *_prot, *ipath, *_ipath, *ads, *_ads;
	struct lws_client_connect_info i;
	const struct ss_pcols *ssp;
	size_t used_in, used_out;
	union lws_ss_contemp ct;
	lws_ss_state_return_t r;
	int port, _port, tls;
	char *path, ep[96];
	lws_strexp_t exp;
	struct lws *wsi;

	lws_service_assert_loop_thread(h->context, h->tsi);

	if (!h->policy) {
		lwsl_err("%s: ss with no policy\n", __func__);

		return LWSSSSRET_OK;
	}

	/*
	 * We are already bound to a sink?
	 */

//	if (h->h_sink)
//		return 0;

	if (!is_retry)
		h->retry = 0;

#if defined(LWS_WITH_SYS_SMD)
	if (h->policy == &pol_smd) {

		if (h->u.smd.smd_peer)
			return LWSSSSRET_OK;

		// lwsl_notice("%s: received connect for _lws_smd, registering for class mask 0x%x\n",
		//		__func__, h->info.manual_initial_tx_credit);

		h->u.smd.smd_peer = lws_smd_register(h->context, h,
					(h->info.flags & LWSSSINFLAGS_PROXIED) ?
						LWSSMDREG_FLAG_PROXIED_SS : 0,
					(lws_smd_class_t)h->info.manual_initial_tx_credit,
					lws_smd_ss_cb);
		if (!h->u.smd.smd_peer)
			return LWSSSSRET_TX_DONT_SEND;

		if (lws_ss_event_helper(h, LWSSSCS_CONNECTING))
			return LWSSSSRET_TX_DONT_SEND;

		if (lws_ss_event_helper(h, LWSSSCS_CONNECTED))
			return LWSSSSRET_TX_DONT_SEND;
		return LWSSSSRET_OK;
	}
#endif

	/*
	 * We're going to substitute ${metadata} in the endpoint at connection-
	 * time, so this can be set dynamically...
	 */

	lws_strexp_init(&exp, (void *)h, lws_ss_exp_cb_metadata, ep, sizeof(ep));

	if (lws_strexp_expand(&exp, h->policy->endpoint,
			      strlen(h->policy->endpoint),
			      &used_in, &used_out) != LSTRX_DONE) {
		lwsl_err("%s: address strexp failed\n", __func__);

		return LWSSSSRET_TX_DONT_SEND;
	}

	/*
	 * ... in some cases, we might want the user to be able to override
	 * some policy settings by what he provided in there.  For example,
	 * if he set the endpoint to "https://myendpoint.com:4443/mypath" it
	 * might be quite convenient to override the policy to follow the info
	 * that was given for at least server, port and the url path.
	 */

	_port = port = h->policy->port;
	_prot = prot = NULL;
	_ipath = ipath = "";
	_ads = ads = ep;

	if (strchr(ep, ':') &&
	    !lws_parse_uri(ep, &_prot, &_ads, &_port, &_ipath)) {
		lwsl_debug("%s: using uri parse results '%s' '%s' %d '%s'\n",
				__func__, _prot, _ads, _port, _ipath);
		prot = _prot;
		ads = _ads;
		port = _port;
		ipath = _ipath;
	}

	memset(&i, 0, sizeof i); /* otherwise uninitialized garbage */
	i.context = h->context;
	tls = !!(h->policy->flags & LWSSSPOLF_TLS);

	if (prot && (!strcmp(prot, "http") || !strcmp(prot, "ws") ||
		     !strcmp(prot, "mqtt")))
		tls = 0;

	if (tls) {
		lwsl_info("%s: using tls\n", __func__);
		i.ssl_connection = LCCSCF_USE_SSL;

		if (!h->policy->trust.store)
			lwsl_info("%s: using platform trust store\n", __func__);
		else {

			i.vhost = lws_get_vhost_by_name(h->context,
					h->policy->trust.store->name);
			if (!i.vhost) {
				lwsl_err("%s: missing vh for policy %s\n",
					 __func__,
					 h->policy->trust.store->name);

				return -1;
			}
		}
	}

	if (h->policy->flags & LWSSSPOLF_WAKE_SUSPEND__VALIDITY)
		i.ssl_connection |= LCCSCF_WAKE_SUSPEND__VALIDITY;

	/* translate policy attributes to IP ToS flags */

	if (h->policy->flags & LWSSSPOLF_ATTR_LOW_LATENCY)
		i.ssl_connection |= LCCSCF_IP_LOW_LATENCY;
	if (h->policy->flags & LWSSSPOLF_ATTR_HIGH_THROUGHPUT)
		i.ssl_connection |= LCCSCF_IP_HIGH_THROUGHPUT;
	if (h->policy->flags & LWSSSPOLF_ATTR_HIGH_RELIABILITY)
		i.ssl_connection |= LCCSCF_IP_HIGH_RELIABILITY;
	if (h->policy->flags & LWSSSPOLF_ATTR_LOW_COST)
		i.ssl_connection |= LCCSCF_IP_LOW_COST;
	if (h->policy->flags & LWSSSPOLF_PERF) /* collect conmon stats on this */
		i.ssl_connection |= LCCSCF_CONMON;

	/* mark the connection with the streamtype priority from the policy */

	i.priority = h->policy->priority;

	i.ssl_connection |= LCCSCF_SECSTREAM_CLIENT;

	if (conn_if_sspc_onw) {
		i.ssl_connection |= LCCSCF_SECSTREAM_PROXY_ONWARD;
		h->conn_if_sspc_onw = conn_if_sspc_onw;
	}


	i.address		= ads;
	i.port			= port;
	i.host			= i.address;
	i.origin		= i.address;
	i.opaque_user_data	= h;
	i.seq			= h->seq;
	i.retry_and_idle_policy	= h->policy->retry_bo;
	i.sys_tls_client_cert	= h->policy->client_cert;

	i.path			= ipath;
		/* if this is not "", munge should use it instead of policy
		 * url path
		 */

	ssp = ss_pcols[(int)h->policy->protocol];
	if (!ssp) {
		lwsl_err("%s: unsupported protocol\n", __func__);

		return LWSSSSRET_TX_DONT_SEND;
	}
	i.alpn = ssp->alpn;

	/*
	 * For http, we can get the method from the http object, override in
	 * the protocol-specific munge callback below if not http
	 */
	i.method = h->policy->u.http.method;
	i.protocol = ssp->protocol->name; /* lws protocol name */
	i.local_protocol_name = i.protocol;

	path = lws_malloc(h->context->max_http_header_data, __func__);
	if (!path) {
		lwsl_warn("%s: OOM on path prealloc\n", __func__);
		return LWSSSSRET_TX_DONT_SEND;
	}

	if (ssp->munge) /* eg, raw doesn't use; endpoint strexp already done */
		ssp->munge(h, path, h->context->max_http_header_data, &i, &ct);

	i.pwsi = &h->wsi;

#if defined(LWS_WITH_SSPLUGINS)
	if (h->policy->plugins[0] && h->policy->plugins[0]->munge)
		h->policy->plugins[0]->munge(h, path, h->context->max_http_header_data);
#endif

	lwsl_info("%s: connecting %s, '%s' '%s' %s\n", __func__, i.method,
			i.alpn, i.address, i.path);

#if defined(LWS_WITH_SYS_METRICS)
	/* possibly already hanging connect retry... */
	if (!h->cal_txn.mt)
		lws_metrics_caliper_bind(h->cal_txn, h->context->mth_ss_conn);

	if (h->policy->streamtype)
		lws_metrics_tag_add(&h->cal_txn.mtags_owner, "ss",
				    h->policy->streamtype);
#endif

	h->txn_ok = 0;
	r = lws_ss_event_helper(h, LWSSSCS_CONNECTING);
	if (r) {
		lws_free(path);
		return r;
	}

	h->inside_connect = 1;
	h->pending_ret = LWSSSSRET_OK;
	wsi = lws_client_connect_via_info(&i);
	h->inside_connect = 0;
	lws_free(path);
	if (!wsi) {
		/*
		 * We already found that we could not connect, without even
		 * having to go around the event loop
		 */

		if (h->pending_ret)
			return h->pending_ret;

		if (h->prev_ss_state != LWSSSCS_UNREACHABLE &&
		    h->prev_ss_state != LWSSSCS_ALL_RETRIES_FAILED) {
			/*
			 * blocking DNS failure can get to unreachable via
			 * CCE, and unreachable can get to ALL_RETRIES_FAILED
			 */
			r = lws_ss_event_helper(h, LWSSSCS_UNREACHABLE);
			if (r)
				return r;

			r = lws_ss_backoff(h);
			if (r)
				return r;
		}

		return LWSSSSRET_TX_DONT_SEND;
	}

	return LWSSSSRET_OK;
}

lws_ss_state_return_t
lws_ss_client_connect(lws_ss_handle_t *h)
{
	lws_ss_state_return_t r;

	lws_service_assert_loop_thread(h->context, h->tsi);

	r = _lws_ss_client_connect(h, 0, 0);

	return r;
}

/*
 * Public API
 */

/*
 * Create either a stream or a sink
 */

int
lws_ss_create(struct lws_context *context, int tsi, const lws_ss_info_t *ssi,
	      void *opaque_user_data, lws_ss_handle_t **ppss,
	      struct lws_sequencer *seq_owner, const char **ppayload_fmt)
{
	struct lws_context_per_thread *pt = &context->pt[tsi];
	const lws_ss_policy_t *pol;
	lws_ss_state_return_t r;
	lws_ss_metadata_t *smd;
	lws_ss_handle_t *h;
	size_t size;
	void **v;
	char *p;
	int n;

	lws_service_assert_loop_thread(context, tsi);

#if defined(LWS_WITH_SECURE_STREAMS_CPP)
	pol = ssi->policy;
	if (!pol) {
#endif

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
		lws_fi_ctx_t temp_fic;

		/*
		 * We have to do a temp inherit from context to find out
		 * early if we are supposed to inject a fault concealing
		 * the policy
		 */

		memset(&temp_fic, 0, sizeof(temp_fic));
		lws_xos_init(&temp_fic.xos, lws_xos(&context->fic.xos));
		lws_fi_inherit_copy(&temp_fic, &context->fic, "ss", ssi->streamtype);

		if (lws_fi(&temp_fic, "ss_no_streamtype_policy"))
			pol = NULL;
		else
			pol = lws_ss_policy_lookup(context, ssi->streamtype);

		lws_fi_destroy(&temp_fic);
#else
		pol = lws_ss_policy_lookup(context, ssi->streamtype);
#endif
		if (!pol) {
			lwsl_cx_info(context, "unknown stream type %s",
				  ssi->streamtype);
			return 1;
		}
#if defined(LWS_WITH_SECURE_STREAMS_CPP)
	}
#endif

#if 0
	if (ssi->flags & LWSSSINFLAGS_REGISTER_SINK) {
		/*
		 * This can register a secure streams sink as well as normal
		 * secure streams connections.  If that's what's happening,
		 * confirm the policy agrees that this streamtype should be
		 * directed to a sink.
		 */
		if (!(pol->flags & LWSSSPOLF_LOCAL_SINK)) {
			/*
			 * Caller wanted to create a sink for this streamtype,
			 * but the policy does not agree the streamtype should
			 * be routed to a local sink.
			 */
			lwsl_err("%s: %s policy does not allow local sink\n",
				 __func__, ssi->streamtype);

			return 1;
		}
	} else {

		if (!(pol->flags & LWSSSPOLF_LOCAL_SINK)) {

		}
//		lws_dll2_foreach_safe(&pt->ss_owner, NULL, lws_ss_destroy_dll);
	}
#endif

	/*
	 * We overallocate and point to things in the overallocation...
	 *
	 * 1) the user_alloc from the stream info
	 * 2) network auth plugin instantiation data
	 * 3) stream auth plugin instantiation data
	 * 4) as many metadata pointer structs as the policy tells
	 * 5) the streamtype name (length is not aligned)
	 *
	 * ... when we come to destroy it, just one free to do.
	 */

	size = sizeof(*h) + ssi->user_alloc +
			(ssi->streamtype ? strlen(ssi->streamtype): 0) + 1;
#if defined(LWS_WITH_SSPLUGINS)
	if (pol->plugins[0])
		size += pol->plugins[0]->alloc;
	if (pol->plugins[1])
		size += pol->plugins[1]->alloc;
#endif
	size += pol->metadata_count * sizeof(lws_ss_metadata_t);

	h = lws_zalloc(size, __func__);
	if (!h)
		return 2;

	h->lc.log_cx = context->log_cx;

	if (ssi->sss_protocol_version)
		__lws_lc_tag(context, &context->lcg[LWSLCG_WSI_SS_CLIENT],
			     &h->lc, "%s|v%u|%u",
			     ssi->streamtype ? ssi->streamtype : "nostreamtype",
			     (unsigned int)ssi->sss_protocol_version,
			     (unsigned int)ssi->client_pid);
	else
		__lws_lc_tag(context, &context->lcg[LWSLCG_WSI_SS_CLIENT],
			     &h->lc, "%s",
			     ssi->streamtype ? ssi->streamtype : "nostreamtype");

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	h->fic.name = "ss";
	lws_xos_init(&h->fic.xos, lws_xos(&context->fic.xos));
	if (ssi->fic.fi_owner.count)
		lws_fi_import(&h->fic, &ssi->fic);

	lws_fi_inherit_copy(&h->fic, &context->fic, "ss", ssi->streamtype);
#endif

	h->info = *ssi;
	h->policy = pol;
	h->context = context;
	h->tsi = (uint8_t)tsi;
	h->seq = seq_owner;

	if (h->info.flags & LWSSSINFLAGS_PROXIED)
		h->proxy_onward = 1;

	/* start of overallocated area */
	p = (char *)(h + 1);

	/* set the handle pointer in the user data struct */
	v = (void **)(p + ssi->handle_offset);
	*v = h;

	/* set the opaque user data in the user data struct */
	v = (void **)(p + ssi->opaque_user_data_offset);
	*v = opaque_user_data;

	p += ssi->user_alloc;

#if defined(LWS_WITH_SSPLUGINS)
	if (pol->plugins[0]) {
		h->nauthi = p;
		p += pol->plugins[0]->alloc;
	}
	if (pol->plugins[1]) {
		h->sauthi = p;
		p += pol->plugins[1]->alloc;
	}
#endif

	if (pol->metadata_count) {
		h->metadata = (lws_ss_metadata_t *)p;
		p += pol->metadata_count * sizeof(lws_ss_metadata_t);

		lwsl_cx_info(context, "%s metadata count %d",
			  pol->streamtype, pol->metadata_count);
	}

	smd = pol->metadata;
	for (n = 0; n < pol->metadata_count; n++) {
		h->metadata[n].name = smd->name;
		if (n + 1 == pol->metadata_count)
			h->metadata[n].next = NULL;
		else
			h->metadata[n].next = &h->metadata[n + 1];
		smd = smd->next;
	}

	if (ssi->streamtype)
		memcpy(p, ssi->streamtype, strlen(ssi->streamtype) + 1);
	/* don't mark accepted ss as being the server */
	if (ssi->flags & LWSSSINFLAGS_SERVER)
		h->info.flags &= (uint8_t)~LWSSSINFLAGS_SERVER;
	h->info.streamtype = p;

	lws_pt_lock(pt, __func__);
	lws_dll2_add_head(&h->list, &pt->ss_owner);
	lws_pt_unlock(pt);

	if (ppss)
		*ppss = h;

	if (ppayload_fmt)
		*ppayload_fmt = pol->payload_fmt;

	if (ssi->flags & LWSSSINFLAGS_SERVER)
		/*
		 * return early for accepted connection flow
		 */
		return 0;

#if defined(LWS_WITH_SYS_SMD)
	/*
	 * For a local Secure Streams connection
	 */
	if (!(ssi->flags & LWSSSINFLAGS_PROXIED) &&
	    pol == &pol_smd) {

		/*
		 * So he has asked to be wired up to SMD over a SS link.
		 * Register him as an smd participant in his own right.
		 *
		 * Just for this case, ssi->manual_initial_tx_credit is used
		 * to set the rx class mask (this is part of the SS serialization
		 * format as well)
		 */
		h->u.smd.smd_peer = lws_smd_register(context, h, 0,
						     (lws_smd_class_t)ssi->manual_initial_tx_credit,
						     lws_smd_ss_cb);
		if (!h->u.smd.smd_peer || lws_fi(&h->fic, "ss_create_smd"))
			goto fail_creation;
		lwsl_cx_info(context, "registered SS SMD");
	}
#endif

#if defined(LWS_WITH_SERVER)
	if (h->policy->flags & LWSSSPOLF_SERVER) {
		const struct lws_protocols *pprot[3], **ppp = &pprot[0];
		struct lws_context_creation_info i;
		struct lws_vhost *vho = NULL;

		lwsl_cx_info(context, "creating server");

		if (h->policy->endpoint &&
		    h->policy->endpoint[0] == '!') {
			/*
			 * There's already a vhost existing that we want to
			 * bind to, we don't have to specify and create one.
			 *
			 * The vhost must enable any protocols that we want.
			 */

			vho = lws_get_vhost_by_name(context,
						    &h->policy->endpoint[1]);
			if (!vho || lws_fi(&h->fic, "ss_create_vhost")) {
				lwsl_err("%s: no vhost %s\n", __func__,
						&h->policy->endpoint[1]);
				goto fail_creation;
			}

			goto extant;
		}

		/*
		 * This streamtype represents a server, we're being asked to
		 * instantiate a corresponding vhost for it
		 */

		memset(&i, 0, sizeof i);

		i.iface		= h->policy->endpoint;
		i.vhost_name	= h->policy->streamtype;
		i.port		= h->policy->port;

		if (i.iface && i.iface[0] == '+') {
			i.iface++;
			i.options |= LWS_SERVER_OPTION_UNIX_SOCK;
		}

		if (!ss_pcols[h->policy->protocol] ||
		    lws_fi(&h->fic, "ss_create_pcol")) {
			lwsl_err("%s: unsupp protocol", __func__);
			goto fail_creation;
		}

		*ppp++ = ss_pcols[h->policy->protocol]->protocol;
#if defined(LWS_ROLE_WS)
		if (h->policy->u.http.u.ws.subprotocol)
			/*
			 * He names a ws subprotocol, ie, we want to support
			 * ss-ws protocol in this vhost
			 */
			*ppp++ = &protocol_secstream_ws;
#endif
		*ppp = NULL;
		i.pprotocols = pprot;

#if defined(LWS_WITH_TLS)
		if (h->policy->flags & LWSSSPOLF_TLS) {
			i.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
			i.server_ssl_cert_mem =
				h->policy->trust.server.cert->ca_der;
			i.server_ssl_cert_mem_len = (unsigned int)
				h->policy->trust.server.cert->ca_der_len;
			i.server_ssl_private_key_mem =
				h->policy->trust.server.key->ca_der;
			i.server_ssl_private_key_mem_len = (unsigned int)
				h->policy->trust.server.key->ca_der_len;
		}
#endif

		if (!lws_fi(&h->fic, "ss_srv_vh_fail"))
			vho = lws_create_vhost(context, &i);
		else
			vho = NULL;
		if (!vho) {
			lwsl_cx_err(context, "failed to create vh");
			goto fail_creation;
		}

extant:

		/*
		 * Mark this vhost as having to apply ss server semantics to
		 * any incoming accepted connection
		 */
		vho->ss_handle = h;

		r = lws_ss_event_helper(h, LWSSSCS_CREATING);
		lwsl_cx_info(context, "CREATING returned status %d", (int)r);
		if (r == LWSSSSRET_DESTROY_ME ||
		    lws_fi(&h->fic, "ss_create_destroy_me"))
			goto fail_creation;

		lwsl_cx_notice(context, "created server %s",
				h->policy->streamtype);

		return 0;
	}
#endif

#if defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)

	/*
	 * For static policy case, dynamically ref / instantiate the related
	 * trust store and vhost.  We do it by logical ss rather than connection
	 * because we don't want to expose the latency of creating the x.509
	 * trust store at the first connection.
	 *
	 * But it might be given the tls linkup takes time anyway, it can move
	 * to the ss connect code instead.
	 */

	if (!lws_ss_policy_ref_trust_store(context, h->policy, 1 /* do the ref */) ||
	    lws_fi(&h->fic, "ss_create_no_ts")) {
		lwsl_err("%s: unable to get vhost / trust store\n", __func__);
		goto fail_creation;
	}
#else
#if defined(LWS_WITH_SECURE_STREAMS_CPP)
        if (!ssi->streamtype &&
	    !lws_ss_policy_ref_trust_store(context, h->policy, 1 /* do the ref */)) {
		lwsl_err("%s: unable to get vhost / trust store\n", __func__);
		goto fail_creation;
	}
#endif
#endif

	r = lws_ss_event_helper(h, LWSSSCS_CREATING);
	lwsl_ss_info(h, "CREATING returned status %d", (int)r);
	if (r == LWSSSSRET_DESTROY_ME ||
	    lws_fi(&h->fic, "ss_create_destroy_me"))
		goto fail_creation;

#if defined(LWS_WITH_SYS_SMD)
	if (!(ssi->flags & LWSSSINFLAGS_PROXIED) &&
	    pol == &pol_smd) {
		r = lws_ss_event_helper(h, LWSSSCS_CONNECTING);
		if (r || lws_fi(&h->fic, "ss_create_smd_1"))
			goto fail_creation;
		r = lws_ss_event_helper(h, LWSSSCS_CONNECTED);
		if (r || lws_fi(&h->fic, "ss_create_smd_2"))
			goto fail_creation;
	}
#endif

	if (!(ssi->flags & LWSSSINFLAGS_REGISTER_SINK) &&
	    ((h->policy->flags & LWSSSPOLF_NAILED_UP)
#if defined(LWS_WITH_SYS_SMD)
		|| ((h->policy == &pol_smd) //&&
		    //(ssi->flags & LWSSSINFLAGS_PROXIED))
				)
#endif
			    )) {
		r = _lws_ss_client_connect(h, 0, 0);
		if (lws_fi(&h->fic, "ss_create_conn"))
			r = LWSSSSRET_DESTROY_ME;
		switch (r) {
		case LWSSSSRET_OK:
			break;
		case LWSSSSRET_TX_DONT_SEND:
		case LWSSSSRET_DISCONNECT_ME:
			if (lws_ss_backoff(h) == LWSSSSRET_DESTROY_ME)
				goto fail_creation;
			break;
		case LWSSSSRET_DESTROY_ME:
			goto fail_creation;
		}
	}

	return 0;

fail_creation:

	if (ppss)
		*ppss = NULL;

	lws_ss_destroy(&h);

	return 1;
}

void *
lws_ss_to_user_object(struct lws_ss_handle *h)
{
	return (void *)(h + 1);
}

void
lws_ss_destroy(lws_ss_handle_t **ppss)
{
	struct lws_context_per_thread *pt;
#if defined(LWS_WITH_SERVER)
	struct lws_vhost *v = NULL;
#endif
	lws_ss_handle_t *h = *ppss;
	lws_ss_metadata_t *pmd;

	if (!h)
		return;

	lws_service_assert_loop_thread(h->context, h->tsi);

	if (h == h->h_in_svc) {
		lwsl_err("%s: illegal destroy, return LWSSSSRET_DESTROY_ME instead\n",
				__func__);
		assert(0);
		return;
	}

	if (h->destroying) {
		lwsl_info("%s: reentrant destroy\n", __func__);
		return;
	}
	h->destroying = 1;

#if defined(LWS_WITH_CONMON)
	if (h->conmon_json)
		lws_free_set_NULL(h->conmon_json);
#endif

	if (h->wsi) {
		/*
		 * Don't let the wsi point to us any more,
		 * we (the ss object bound to the wsi) are going away now
		 */
		lws_set_opaque_user_data(h->wsi, NULL);
		lws_set_timeout(h->wsi, 1, LWS_TO_KILL_SYNC);
	}

	/*
	 * if we bound an smd registration to the SS, unregister it
	 */

#if defined(LWS_WITH_SYS_SMD)
	if (h->policy == &pol_smd) {
		lws_sul_cancel(&h->u.smd.sul_write);

		if (h->u.smd.smd_peer) {
			lws_smd_unregister(h->u.smd.smd_peer);
			h->u.smd.smd_peer = NULL;
		}
	}
#endif

	pt = &h->context->pt[h->tsi];

	lws_pt_lock(pt, __func__);
	*ppss = NULL;
	lws_dll2_remove(&h->list);
#if defined(LWS_WITH_SERVER)
		lws_dll2_remove(&h->cli_list);
#endif
	lws_dll2_remove(&h->to_list);
	lws_sul_cancel(&h->sul_timeout);

	/*
	 * for lss, DESTROYING deletes the C++ lss object, making the
	 * self-defined h->policy radioactive
	 */

#if defined(LWS_WITH_SERVER)
	if (h->policy && (h->policy->flags & LWSSSPOLF_SERVER))
		v = lws_get_vhost_by_name(h->context, h->policy->streamtype);
#endif

	/*
	 * Since we also come here to unpick create, it's possible we failed
	 * the creation before issuing any states, even CREATING.  We should
	 * only issue cleanup states on destroy if we previously got as far as
	 * issuing CREATING.
	 */

	if (h->prev_ss_state) {
		if (h->ss_dangling_connected)
			(void)lws_ss_event_helper(h, LWSSSCS_DISCONNECTED);

		(void)lws_ss_event_helper(h, LWSSSCS_DESTROYING);
	}

	lws_pt_unlock(pt);

	/* in proxy case, metadata value on heap may need cleaning up */

	pmd = h->metadata;
	while (pmd) {
		lwsl_info("%s: pmd %p\n", __func__, pmd);
		if (pmd->value_on_lws_heap)
			lws_free_set_NULL(pmd->value__may_own_heap);

		pmd = pmd->next;
	}

#if defined(LWS_WITH_SS_DIRECT_PROTOCOL_STR)
	{

		lws_ss_metadata_t *imd;
	       
		pmd = h->instant_metadata;

		while (pmd) {
			imd = pmd;
			pmd = pmd->next;

			lwsl_info("%s: instant md %p\n", __func__, imd);
			lws_free(imd);
		}
		h->instant_metadata = NULL;

		if (h->imd_ac)
			lwsac_free(&h->imd_ac);
	}
#endif

	lws_sul_cancel(&h->sul);

#if defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)

	/*
	 * For static policy case, dynamically ref / instantiate the related
	 * trust store and vhost.  We do it by logical ss rather than connection
	 * because we don't want to expose the latency of creating the x.509
	 * trust store at the first connection.
	 *
	 * But it might be given the tls linkup takes time anyway, it can move
	 * to the ss connect code instead.
	 */

	if (h->policy)
		lws_ss_policy_unref_trust_store(h->context, h->policy);
#else
#if defined(LWS_WITH_SECURE_STREAMS_CPP)
	if (!h->info.streamtype || !*(h->info.streamtype))
		lws_ss_policy_unref_trust_store(h->context, h->policy);
#endif
#endif

#if defined(LWS_WITH_SERVER)
	if (v)
		/*
		 * For server, the policy describes a vhost that implements the
		 * server, when we take down the ss, we take down the related
		 * vhost (if it got that far)
		 */
		lws_vhost_destroy(v);
#endif

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	lws_fi_destroy(&h->fic);
#endif

#if defined(LWS_WITH_SYS_METRICS)
	/*
	 * If any hanging caliper measurement, dump it, and free any tags
	 */
	lws_metrics_caliper_report_hist(h->cal_txn, (struct lws *)NULL);
#endif

	lws_sul_cancel(&h->sul_timeout);

	/* confirm no sul left scheduled in handle or user allocation object */
	lws_sul_debug_zombies(h->context, h, sizeof(*h) + h->info.user_alloc,
			      __func__);

	__lws_lc_untag(h->context, &h->lc);

	lws_explicit_bzero((void *)h, sizeof(*h) + h->info.user_alloc);

	lws_free_set_NULL(h);
}

#if defined(LWS_WITH_SERVER)
void
lws_ss_server_ack(struct lws_ss_handle *h, int nack)
{
	h->txn_resp = nack;
	h->txn_resp_set = 1;
}

void
lws_ss_server_foreach_client(struct lws_ss_handle *h, lws_sssfec_cb cb,
			     void *arg)
{
	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1, h->src_list.head) {
		struct lws_ss_handle *h =
			lws_container_of(d, struct lws_ss_handle, cli_list);

		cb(h, arg);

	} lws_end_foreach_dll_safe(d, d1);
}
#endif

lws_ss_state_return_t
lws_ss_request_tx(lws_ss_handle_t *h)
{
	lws_ss_state_return_t r;

	r = _lws_ss_request_tx(h);

	return r;
}

lws_ss_state_return_t
_lws_ss_request_tx(lws_ss_handle_t *h)
{
	lws_ss_state_return_t r;

	// lwsl_notice("%s: h %p, wsi %p\n", __func__, h, h->wsi);

	lws_service_assert_loop_thread(h->context, h->tsi);

	if (h->wsi) {
		lws_callback_on_writable(h->wsi);

		return LWSSSSRET_OK;
	}

	if (!h->policy) {
		/* avoid crash */
		lwsl_err("%s: null policy\n", __func__);
		return LWSSSSRET_OK;
	}

	if (h->policy->flags & LWSSSPOLF_SERVER)
		return LWSSSSRET_OK;

	/*
	 * there's currently no wsi / connection associated with the ss handle
	 */

#if defined(LWS_WITH_SYS_SMD)
	if (h->policy == &pol_smd) {
		/*
		 * He's an _lws_smd... and no wsi... since we're just going
		 * to queue it, we could call his tx() right here, but rather
		 * than surprise him let's set a sul to do it next time around
		 * the event loop
		 */

		lws_sul_schedule(h->context, 0, &h->u.smd.sul_write,
				 lws_ss_smd_tx_cb, 1);

		return LWSSSSRET_OK;
	}
#endif

	if (h->seqstate != SSSEQ_IDLE &&
	    h->seqstate != SSSEQ_DO_RETRY)
		return LWSSSSRET_OK;

	h->seqstate = SSSEQ_TRY_CONNECT;
	if (h->prev_ss_state != LWSSSCS_POLL) { /* possible if we were created
						 * before we could action it */
		r = lws_ss_event_helper(h, LWSSSCS_POLL);
		if (r)
			return r;
	}

	/*
	 * Retries operate via lws_ss_request_tx(), explicitly ask for a
	 * reconnection to clear the retry limit
	 */
	r = _lws_ss_client_connect(h, 1, 0);
	if (r == LWSSSSRET_DESTROY_ME)
		return r;

	if (r)
		return lws_ss_backoff(h);

	return LWSSSSRET_OK;
}

lws_ss_state_return_t
lws_ss_request_tx_len(lws_ss_handle_t *h, unsigned long len)
{
	lws_service_assert_loop_thread(h->context, h->tsi);

	if (h->wsi && h->policy &&
	    (h->policy->protocol == LWSSSP_H1 ||
	     h->policy->protocol == LWSSSP_H2 ||
	     h->policy->protocol == LWSSSP_WS))
		h->wsi->http.writeable_len = len;
	else
		h->writeable_len = len;

	return lws_ss_request_tx(h);
}

/*
 * private helpers
 */

/* used on context destroy when iterating listed lws_ss on a pt */

int
lws_ss_destroy_dll(struct lws_dll2 *d, void *user)
{
	lws_ss_handle_t *h = lws_container_of(d, lws_ss_handle_t, list);

	lws_ss_destroy(&h);

	return 0;
}

int
lws_ss_cancel_notify_dll(struct lws_dll2 *d, void *user)
{
	lws_ss_handle_t *h = lws_container_of(d, lws_ss_handle_t, list);

	if (lws_ss_event_helper(h, LWSSSCS_EVENT_WAIT_CANCELLED))
		lwsl_warn("%s: cancel event ignores return\n", __func__);

	return 0;
}

struct lws_sequencer *
lws_ss_get_sequencer(lws_ss_handle_t *h)
{
	return h->seq;
}

struct lws_context *
lws_ss_get_context(struct lws_ss_handle *h)
{
	return h->context;
}

const char *
lws_ss_rideshare(struct lws_ss_handle *h)
{
	if (!h->rideshare)
		return h->policy->streamtype;

	return h->rideshare->streamtype;
}

int
lws_ss_add_peer_tx_credit(struct lws_ss_handle *h, int32_t bump)
{
	const struct ss_pcols *ssp;

	lws_service_assert_loop_thread(h->context, h->tsi);

	ssp = ss_pcols[(int)h->policy->protocol];

	if (h->wsi && ssp && ssp->tx_cr_add)
		return ssp->tx_cr_add(h, bump);

	return 0;
}

int
lws_ss_get_est_peer_tx_credit(struct lws_ss_handle *h)
{
	const struct ss_pcols *ssp;

	lws_service_assert_loop_thread(h->context, h->tsi);

	ssp = ss_pcols[(int)h->policy->protocol];

	if (h->wsi && ssp && ssp->tx_cr_add)
		return ssp->tx_cr_est(h);

	return 0;
}

/*
 * protocol-independent handler for ss timeout
 */

static void
lws_ss_to_cb(lws_sorted_usec_list_t *sul)
{
	lws_ss_handle_t *h = lws_container_of(sul, lws_ss_handle_t, sul_timeout);
	lws_ss_state_return_t r;

	lwsl_info("%s: %s timeout fired\n", __func__, lws_ss_tag(h));

	r = lws_ss_event_helper(h, LWSSSCS_TIMEOUT);
	if (r != LWSSSSRET_DISCONNECT_ME && r != LWSSSSRET_DESTROY_ME)
		return;

	if (h->wsi)
		lws_set_timeout(h->wsi, 1, LWS_TO_KILL_ASYNC);

	_lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, h->wsi, &h);
}

void
lws_ss_start_timeout(struct lws_ss_handle *h, unsigned int timeout_ms)
{
	lws_service_assert_loop_thread(h->context, h->tsi);

	if (!timeout_ms && !h->policy->timeout_ms)
		return;

	lws_sul_schedule(h->context, 0, &h->sul_timeout, lws_ss_to_cb,
			 (timeout_ms ? timeout_ms : h->policy->timeout_ms) *
			 LWS_US_PER_MS);
}

void
lws_ss_cancel_timeout(struct lws_ss_handle *h)
{
	lws_service_assert_loop_thread(h->context, h->tsi);
	lws_sul_cancel(&h->sul_timeout);
}

void
lws_ss_change_handlers(struct lws_ss_handle *h,
	lws_ss_state_return_t (*rx)(void *userobj, const uint8_t *buf,
				    size_t len, int flags),
	lws_ss_state_return_t (*tx)(void *userobj, lws_ss_tx_ordinal_t ord,
				    uint8_t *buf, size_t *len, int *flags),
	lws_ss_state_return_t (*state)(void *userobj, void *h_src /* ss handle type */,
				       lws_ss_constate_t state,
				       lws_ss_tx_ordinal_t ack))
{
	if (rx)
		h->info.rx = rx;
	if (tx)
		h->info.tx = tx;
	if (state)
		h->info.state = state;
}

const char *
lws_ss_tag(struct lws_ss_handle *h)
{
	if (!h)
		return "[null ss]";
	return lws_lc_tag(&h->lc);
}

struct lws_log_cx *
lwsl_ss_get_cx(struct lws_ss_handle *ss)
{
	if (!ss)
		return NULL;

	return ss->lc.log_cx;
}

void
lws_log_prepend_ss(struct lws_log_cx *cx, void *obj, char **p, char *e)
{
	struct lws_ss_handle *h = (struct lws_ss_handle *)obj;

	*p += lws_snprintf(*p, lws_ptr_diff_size_t(e, (*p)), "%s: ",
			lws_ss_tag(h));
}

#if defined(_DEBUG)
void
lws_ss_assert_extant(struct lws_context *cx, int tsi, struct lws_ss_handle *h)
{
	struct lws_context_per_thread *pt = &cx->pt[tsi];

	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1, pt->ss_owner.head) {
		struct lws_ss_handle *h1 = lws_container_of(d,
						struct lws_ss_handle, list);

		if (h == h1)
			return; /* okay */

	} lws_end_foreach_dll_safe(d, d1);

	/*
	 * The ss handle is not listed in the pt ss handle owner...
	 */

	assert(0);
}
#endif
