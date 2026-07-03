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
 *
 *
 * In the case Secure Streams protocol needs to pass through a buffer,
 * or a streamed connection, the protocol metadata must be serialized.  This
 * file provides internal apis to perform the serialization and deserialization
 * in and out of an lws_dsh fifo-type buffer.
 */

#include <private-lib-core.h>

typedef enum {
	RPAR_TYPE,
	RPAR_LEN_MSB,
	RPAR_LEN_LSB,

	RPAR_FLAG_B3,
	RPAR_FLAG_B2,
	RPAR_FLAG_B1,
	RPAR_FLAG_B0,

	RPAR_LATA3,
	RPAR_LATA2,
	RPAR_LATA1,
	RPAR_LATA0,

	RPAR_LATB7,
	RPAR_LATB6,
	RPAR_LATB5,
	RPAR_LATB4,
	RPAR_LATB3,
	RPAR_LATB2,
	RPAR_LATB1,
	RPAR_LATB0,

	RPAR_RIDESHARE_LEN,
	RPAR_RIDESHARE,

	RPAR_PERF,

	RPAR_RESULT_CREATION_DSH,
	RPAR_RESULT_CREATION_RIDESHARE,

	RPAR_METADATA_NAMELEN,
	RPAR_METADATA_NAME,
	RPAR_METADATA_VALUE,

	RPAR_PAYLOAD,

	RPAR_RX_TXCR_UPDATE,

	RPAR_STREAMTYPE,
	RPAR_INIT_PROVERS,
	RPAR_INIT_PID,
	RPAR_INITTXC0,

	RPAR_TXCR0,

	RPAR_TIMEOUT0,

	RPAR_PAYLEN0,

	RPAR_RESULT_CREATION,

	RPAR_STATEINDEX,
	RPAR_ORD3,
	RPAR_ORD2,
	RPAR_ORD1,
	RPAR_ORD0,
} rx_parser_t;

#if defined(_DEBUG) && !defined(LWS_WITH_NO_LOGS)
static const char *sn[] = {
	"unset",

	"LPCSPROX_WAIT_INITIAL_TX",
	"LPCSPROX_REPORTING_FAIL",
	"LPCSPROX_REPORTING_OK",
	"LPCSPROX_OPERATIONAL",
	"LPCSPROX_DESTROYED",

	"LPCSCLI_SENDING_INITIAL_TX",
	"LPCSCLI_WAITING_CREATE_RESULT",
	"LPCSCLI_LOCAL_CONNECTED",
	"LPCSCLI_ONWARD_CONNECT",
	"LPCSCLI_OPERATIONAL",
};
#endif

struct lws_log_cx *
lwsl_sspc_get_cx(struct lws_sspc_handle *sspc)
{
	if (!sspc)
		return NULL;

	return sspc->lc.log_cx;
}


void
lws_log_prepend_sspc(struct lws_log_cx *cx, void *obj, char **p, char *e)
{
	struct lws_sspc_handle *h = (struct lws_sspc_handle *)obj;

	*p += lws_snprintf(*p, lws_ptr_diff_size_t(e, (*p)), "%s: ",
			lws_sspc_tag(h));
}

static void
lws_ss_serialize_state_transition(lws_sspc_handle_t *h,
				  lws_ss_conn_states_t *state, int new_state)
{
#if defined(_DEBUG)
	lwsl_sspc_info(h, "%s -> %s", sn[*state], sn[new_state]);
#endif
	*state = (lws_ss_conn_states_t)new_state;
}


/*
 * event loop received something and is queueing it for the foreign side of
 * the dsh to consume later as serialized rx
 */

int
lws_ss_serialize_rx_payload(struct lws_dsh *dsh, const uint8_t *buf,
			    size_t len, int flags, const char *rsp)
{
	lws_usec_t us = lws_now_usecs();
	uint8_t pre[128];
	int est = 19, l = 0;

	if (flags & LWSSS_FLAG_RIDESHARE) {
		/*
		 * We should have the rideshare name if we have been told it's
		 * on a non-default rideshare
		 */
		assert(rsp);
		if (!rsp)
			return 1;
		l = (int)strlen(rsp);
		est += 1 + l;
	} else
		assert(!rsp);

	// lwsl_user("%s: len %d, flags: %d\n", __func__, (int)len, flags);
	// lwsl_hexdump_info(buf, len);

	pre[0] = LWSSS_SER_RXPRE_RX_PAYLOAD;
	lws_ser_wu16be(&pre[1], (uint16_t)(len + (size_t)est - 3));
	lws_ser_wu32be(&pre[3], (uint32_t)flags);
	lws_ser_wu32be(&pre[7], 0);	/* write will compute latency here... */
	lws_ser_wu64be(&pre[11], (uint64_t)us);	/* ... and set this to the write time */

	/*
	 * If we are on a non-default rideshare, append the non-default name to
	 * the headers of the payload part, 1-byte length first
	 */

	if (flags & LWSSS_FLAG_RIDESHARE) {
		pre[19] = (uint8_t)l;
		memcpy(&pre[20], rsp, (unsigned int)l);
	}

	if (lws_dsh_alloc_tail(dsh, KIND_SS_TO_P, pre, (unsigned int)est, buf, len)) {
		lwsl_err("%s: unable to alloc in dsh 1\n", __func__);

		return 1;
	}

	return 0;
}

/*
 * event loop is consuming dsh-buffered, already-serialized tx from the
 * foreign side
 */

int
lws_ss_deserialize_tx_payload(struct lws_dsh *dsh, struct lws *wsi,
			      lws_ss_tx_ordinal_t ord, uint8_t *buf,
			      size_t *len, int *flags)
{
	uint8_t *p;
	size_t si;

	if (lws_dsh_get_head(dsh, KIND_C_TO_P, (void **)&p, &si)) {
		*len = 0;
		return 0;
	}

	/*
	 * The packet in the dsh has a proxying serialization header, process
	 * and strip it so we just forward the payload
	 */

	if (*len <= si - 23 || si < 23) {
		/*
		 * What comes out of the dsh needs to fit in the tx buffer...
		 * we have arrangements at the proxy rx of the client UDS to
		 * chop chunks larger than 1380 into seuqential lumps of 1380
		 */
		lwsl_err("%s: *len = %d, si = %d\n", __func__, (int)*len, (int)si);
		assert(0);
		return 1;
	}
	if (p[0] != LWSSS_SER_TXPRE_TX_PAYLOAD) {
		assert(0);
		return 1;
	}

	*len = (size_t)(lws_ser_ru16be(&p[1]) - (23 - 3));
	if (*len != si - 23) {
		/*
		 * We cannot accept any length that doesn't reflect the actual
		 * length of what came in from the dsh, either something nasty
		 * happened with truncation or we are being attacked
		 */
		assert(0);

		return 1;
	}

	memcpy(buf, p + 23, si - 23);

	*flags = (int)lws_ser_ru32be(&p[3]);

	lws_dsh_free((void **)&p);

	return 0;
}

/*
 * event loop side is issuing state, serialize and put it in the dbuf for
 * the foreign side to consume later
 */

int
lws_ss_serialize_state(struct lws *wsi, struct lws_dsh *dsh, lws_ss_constate_t state,
		       lws_ss_tx_ordinal_t ack)
{
	uint8_t pre[12];
	int n = 4;

	if (state == LWSSSCS_EVENT_WAIT_CANCELLED)
		return 0;

	lwsl_info("%s: %s, ord 0x%x\n", __func__, lws_ss_state_name((int)state),
		  (unsigned int)ack);

	pre[0] = LWSSS_SER_RXPRE_CONNSTATE;
	pre[1] = 0;

	if (state > 255) {
		pre[2] = 8;
		lws_ser_wu32be(&pre[3], state);
		n = 7;
	} else {
		pre[2] = 5;
		pre[3] = (uint8_t)state;
	}

	lws_ser_wu32be(&pre[n], ack);

	if (lws_dsh_alloc_tail(dsh, KIND_SS_TO_P, pre, (unsigned int)n + 4, NULL, 0) ||
	    (wsi && lws_fi(&wsi->fic, "sspc_dsh_ss2p_oom"))) {
		lwsl_err("%s: unable to alloc in dsh 2\n", __func__);

		return 1;
	}

	return 0;
}

/*
 * event loop side was told about remote peer tx credit window update, serialize
 * and put it in the dbuf for the foreign side to consume later
 */

int
lws_ss_serialize_txcr(struct lws_dsh *dsh, int txcr)
{
	uint8_t pre[7];

	lwsl_info("%s: %d\n", __func__, txcr);

	pre[0] = LWSSS_SER_RXPRE_TXCR_UPDATE;
	pre[1] = 0;
	pre[2] = 4;
	lws_ser_wu32be(&pre[3], (uint32_t)txcr);

	if (lws_dsh_alloc_tail(dsh, KIND_SS_TO_P, pre, 7, NULL, 0)) {
		lwsl_err("%s: unable to alloc in dsh 2\n", __func__);

		return 1;
	}

	return 0;
}

/*
 * event loop side is consuming serialized data from the client via dsh, parse
 * it using a bytewise parser for the serialization header(s)...
 * it's possibly coalesced
 *
 * client: pss is pointing to the start of userdata.  We can use
 *         pss_to_sspc_h(_pss, _ssi) to convert that to a pointer to the sspc
 *         handle
 *
 * proxy: pss is pointing to &conn->ss, a pointer to the ss handle
 *
 * Returns one of
 *
 * 	LWSSSSRET_OK
 *	LWSSSSRET_DISCONNECT_ME
 *	LWSSSSRET_DESTROY_ME
 */

/* convert userdata ptr _pss to handle pointer, allowing for any layout in
 * userdata */
#define client_pss_to_sspc_h(_pss, _ssi) (*((lws_sspc_handle_t **) \
				     ((uint8_t *)_pss) + _ssi->handle_offset))
/* client pss to sspc userdata */
#define client_pss_to_userdata(_pss) ((void *)_pss)
/* proxy convert pss to ss handle */
#define proxy_pss_to_ss_h(_pss) (*_pss)

/* convert userdata ptr _pss to handle pointer, allowing for any layout in
 * userdata */
#define client_pss_to_sspc_h(_pss, _ssi) (*((lws_sspc_handle_t **) \
				     ((uint8_t *)_pss) + _ssi->handle_offset))
/* client pss to sspc userdata */
#define client_pss_to_userdata(_pss) ((void *)_pss)
/* proxy convert pss to ss handle */
#define proxy_pss_to_ss_h(_pss) (*_pss)

int
lws_ss_deserialize_parse(struct lws_ss_serialization_parser *par,
			 struct lws_context *context,
			 struct lws_dsh *dsh, const uint8_t *cp, size_t len,
			 lws_ss_conn_states_t *state, void *parconn,
			 lws_ss_handle_t **pss, lws_ss_info_t *ssi, char client)
{
	lws_ss_state_return_t r;
	lws_ss_metadata_t *pm;
	lws_sspc_handle_t *h;
	uint8_t pre[23];
	uint32_t flags;
	lws_usec_t us;
	uint8_t *p;
	int n;

	while (len--) {

		switch (par->ps) {
		case RPAR_TYPE:
			par->type = *cp++;
			par->ps++;
			break;

		case RPAR_LEN_MSB: /* this is remaining frame length */
			par->rem = (uint16_t)((*cp++) << 8);
			par->ps++;
			break;

		case RPAR_LEN_LSB:
			par->rem = (uint16_t)(par->rem | *cp++);
			switch (par->type) {

			/* event loop side */

			case LWSSS_SER_TXPRE_TX_PAYLOAD:
				if (client)
					goto hangup;
				if (*state != LPCSPROX_OPERATIONAL)
					goto hangup;

				par->ps = RPAR_FLAG_B3;
				break;

			case LWSSS_SER_TXPRE_DESTROYING:
				if (client)
					goto hangup;
				par->ps = RPAR_TYPE;
				lwsl_cx_notice(context, "DESTROYING");
				goto hangup;

			case LWSSS_SER_TXPRE_ONWARD_CONNECT:
				if (client)
					goto hangup;

				if (*state != LPCSPROX_OPERATIONAL)
					goto hangup;

				par->ps = RPAR_TYPE;
				lwsl_cx_notice(context, "ONWARD_CONNECT");

				/*
				 * Shrug it off if we are already connecting or
				 * connected
				 */

				if (!proxy_pss_to_ss_h(pss) ||
				    proxy_pss_to_ss_h(pss)->wsi)
					break;

				/*
				 * We're going to try to do the onward connect
				 */

				if ((proxy_pss_to_ss_h(pss) &&
				     lws_fi(&proxy_pss_to_ss_h(pss)->fic, "ssproxy_onward_conn_fail")) ||
				    _lws_ss_client_connect(proxy_pss_to_ss_h(pss),
							   0, parconn) ==
							   LWSSSSRET_DESTROY_ME)
					goto hangup;
				break;

			case LWSSS_SER_TXPRE_STREAMTYPE:
				if (client)
					goto hangup;
				if (*state != LPCSPROX_WAIT_INITIAL_TX)
					goto hangup;
				if (par->rem < 1 + 4 + 1)
					goto hangup;
				par->ps = RPAR_INIT_PROVERS;
				break;

			case LWSSS_SER_TXPRE_METADATA:
				if (client)
					goto hangup;
				if (par->rem < 3)
					goto hangup;
				par->ctr = 0;
				par->ps = RPAR_METADATA_NAMELEN;
				break;

			case LWSSS_SER_TXPRE_TXCR_UPDATE:
				par->ps = RPAR_TXCR0;
				par->ctr = 0;
				break;

			case LWSSS_SER_TXPRE_TIMEOUT_UPDATE:
				if (client)
					goto hangup;
				if (par->rem != 4)
					goto hangup;
				par->ps = RPAR_TIMEOUT0;
				par->ctr = 0;
				break;

			case LWSSS_SER_TXPRE_PAYLOAD_LENGTH_HINT:
				if (client)
					goto hangup;
				if (par->rem != 4)
					goto hangup;
				par->ps = RPAR_PAYLEN0;
				par->ctr = 0;
				break;

			/* client side */

			case LWSSS_SER_RXPRE_RX_PAYLOAD:
				if (!client)
					goto hangup;
				if (*state != LPCSCLI_OPERATIONAL &&
				    *state != LPCSCLI_LOCAL_CONNECTED)
					goto hangup;

				par->rideshare[0] = '\0';
				par->ps = RPAR_FLAG_B3;
				break;

			case LWSSS_SER_RXPRE_CREATE_RESULT:
				if (!client)
					goto hangup;
				if (*state != LPCSCLI_WAITING_CREATE_RESULT)
					goto hangup;

				if (par->rem < 1)
					goto hangup;

				par->ps = RPAR_RESULT_CREATION;
				break;

			case LWSSS_SER_RXPRE_CONNSTATE:
				if (!client)
					goto hangup;
				if (*state != LPCSCLI_LOCAL_CONNECTED &&
				    *state != LPCSCLI_OPERATIONAL)
					goto hangup;

				if (par->rem < 5 || par->rem > 8)
					goto hangup;

				par->ps = RPAR_STATEINDEX;
				par->ctr = 0;
				break;

			case LWSSS_SER_RXPRE_METADATA:
				if (!client)
					goto hangup;
				if (par->rem < 3)
					goto hangup;
				par->ctr = 0;
				par->ps = RPAR_METADATA_NAMELEN;
				break;

			case LWSSS_SER_RXPRE_TXCR_UPDATE:
				par->ctr = 0;
				par->ps = RPAR_RX_TXCR_UPDATE;
				break;

			case LWSSS_SER_RXPRE_PERF:
				par->ctr = 0;
				if (!par->rem)
					goto hangup;
				par->ps = RPAR_PERF;
				break;

			default:
				lwsl_cx_notice(context, "bad type 0x%x",
					       par->type);
				goto hangup;
			}
			break;

			case RPAR_FLAG_B3:
			case RPAR_FLAG_B2:
			case RPAR_FLAG_B1:
			case RPAR_FLAG_B0:
				par->flags <<= 8;
				par->flags |= *cp++;
				par->ps++;
				if (!par->rem--)
					goto hangup;
				break;

			case RPAR_LATA3:
			case RPAR_LATA2:
			case RPAR_LATA1:
			case RPAR_LATA0:
				par->usd_phandling <<= 8;
				par->usd_phandling |= *cp++;
				par->ps++;
				if (!par->rem--)
					goto hangup;
				break;

			case RPAR_LATB7:
			case RPAR_LATB6:
			case RPAR_LATB5:
			case RPAR_LATB4:
			case RPAR_LATB3:
			case RPAR_LATB2:
			case RPAR_LATB1:
			case RPAR_LATB0:
				par->ust_pwait <<= 8;
				par->ust_pwait |= *cp++;
				par->ps++;
				par->frag1 = 1;
				if (!par->rem--)
					goto hangup;

				if (par->ps == RPAR_RIDESHARE_LEN &&
				    !(par->flags & LWSSS_FLAG_RIDESHARE))
					par->ps = RPAR_PAYLOAD;

				if (par->rem)
					break;

				/* fallthru - handle 0-length payload */

				if (!(par->flags & LWSSS_FLAG_RIDESHARE))
					goto payload_ff;
				goto hangup;

			/*
			 * Inbound rideshare info is provided on the RX packet
			 * itself
			 */

		case RPAR_RIDESHARE_LEN:
			par->slen = *cp++;
			par->ctr = 0;
			par->ps++;
			if (par->rem-- < par->slen)
				goto hangup;
			break;

		case RPAR_PERF:
			n = (int)len + 1;
			if (n > par->rem)
				n = par->rem;

			if (client &&
			    client_pss_to_sspc_h(pss, ssi) &&
			    ssi->rx) {
				int ret;

				/* we still have an sspc handle */
				ret = ssi->rx(client_pss_to_userdata(pss),
					(uint8_t *)cp, (unsigned int)n,
					(int)(LWSSS_FLAG_SOM | LWSSS_FLAG_EOM |
							LWSSS_FLAG_PERF_JSON));

				if (lws_fi(&client_pss_to_sspc_h(pss, ssi)->fic,
						    "sspc_perf_rx_fake_destroy_me"))
					ret = LWSSSSRET_DESTROY_ME;

				switch (ret) {
				case LWSSSSRET_OK:
					break;
				case LWSSSSRET_DISCONNECT_ME:
					goto hangup;
				case LWSSSSRET_DESTROY_ME:
					return LWSSSSRET_DESTROY_ME;
				}
			}
			if (n) {
				cp += n;
				par->rem = (uint16_t)(par->rem - (uint16_t)(unsigned int)n);
				len = (len + 1) - (unsigned int)n;
			}
			if (!par->rem)
				par->ps = RPAR_TYPE;
			break;

		case RPAR_RIDESHARE:
			par->rideshare[par->ctr++] = (char)*cp++;
			if (!par->rem--)
				goto hangup;
			if (par->ctr != par->slen)
				break;
			par->ps = RPAR_PAYLOAD;
			if (par->rem)
				break;

			/* fallthru - handle 0-length payload */

		case RPAR_PAYLOAD:
payload_ff:
			n = (int)len + 1;
			if (n > par->rem)
				n = par->rem;
			/*
			 * We get called with a serialized buffer of a size
			 * chosen by the client.  We can only create dsh entries
			 * with up to 1380 payload, to guarantee we can emit
			 * them on the onward connection atomically.
			 *
			 * If 1380 isn't enough to cover what was handed to us,
			 * we'll stop at 1380 and go around again and create
			 * more dsh entries for the rest, with their own
			 * headers.
			 */

			if (n > 1380)
				n = 1380;

			/*
			 * Since we're in the business of fragmenting client
			 * serialized payloads at 1380, we have to deal with
			 * refragmenting the SOM / EOM flags that covered the
			 * whole client serialized packet, so they apply to
			 * each dsh entry we split it into correctly
			 */

			flags = par->flags & LWSSS_FLAG_RELATED_START;
			if (par->frag1)
				/*
				 * Only set the first time we came to this
				 * state after deserialization of the header
				 */
				flags |= par->flags &
				    (LWSSS_FLAG_SOM | LWSSS_FLAG_POLL);

			if (par->rem == n)
				/*
				 * We are going to complete the advertised
				 * payload length from the client on this dsh,
				 * so give him the EOM type flags if any
				 */
				flags |= par->flags & (LWSSS_FLAG_EOM |
						LWSSS_FLAG_RELATED_END);

			par->frag1 = 0;
			us = lws_now_usecs();

			if (!client) {
				lws_ss_handle_t *hss;

				/*
				 * Proxy - we received some serialized tx from
				 * the client.
				 *
				 * The header for buffering private to the
				 * proxy is 23 bytes vs 19, so we can hold the
				 * current time when it was buffered
				 * additionally
				 */

				hss = proxy_pss_to_ss_h(pss);
				if (hss)
					lwsl_ss_info(hss, "C2P RX: len %d", (int)n);

				p = pre;
				pre[0] = LWSSS_SER_TXPRE_TX_PAYLOAD;
				lws_ser_wu16be(&p[1], (uint16_t)((unsigned int)n + 23 - 3));
				lws_ser_wu32be(&p[3], flags);
				/* us held at client before written */
				lws_ser_wu32be(&p[7], par->usd_phandling);
				/* us taken for transit to proxy */
				lws_ser_wu32be(&p[11], (uint32_t)(us - (lws_usec_t)par->ust_pwait));
				/* time used later to find proxy hold time */
				lws_ser_wu64be(&p[15], (uint64_t)us);

				if ((hss &&
				    lws_fi(&hss->fic, "ssproxy_dsh_c2p_pay_oom")) ||
				    lws_dsh_alloc_tail(dsh, KIND_C_TO_P, pre,
						       23, cp, (unsigned int)n)) {
					lwsl_ss_err(hss, "unable to alloc in dsh 3");

					return LWSSSSRET_DISCONNECT_ME;
				}

				if (hss)
					_lws_ss_request_tx(hss);
			} else {

				/*
				 * Client receives some RX from proxy
				 *
				 * Pass whatever payload we have to ss user
				 */

				h = lws_container_of(par, lws_sspc_handle_t,
						     parser);
				h->txc.peer_tx_cr_est -= n;

				lwsl_sspc_info(h, "P2C RX: len %d", (int)n);

				if (ssi->rx && client_pss_to_sspc_h(pss, ssi)) {
					/* we still have an sspc handle */
					int ret;

					ret = ssi->rx(client_pss_to_userdata(pss),
						(uint8_t *)cp, (unsigned int)n, (int)flags);

					if (client_pss_to_sspc_h(pss, ssi) &&
					    lws_fi(&client_pss_to_sspc_h(pss, ssi)->fic, "sspc_rx_fake_destroy_me"))
						ret = LWSSSSRET_DESTROY_ME;

					switch (ret) {
					case LWSSSSRET_OK:
						break;
					case LWSSSSRET_DISCONNECT_ME:
						goto hangup;
					case LWSSSSRET_DESTROY_ME:
						return LWSSSSRET_DESTROY_ME;
					}
				}

#if 0
				if (lws_det_lat_active(context)) {
					lws_detlat_t d;

					d.type = LDLT_READ;
					d.acc_size = d.req_size = n;
					d.latencies[LAT_DUR_USERCB] =
							lws_now_usecs() - us;
					d.latencies[LAT_DUR_PROXY_CLIENT_REQ_TO_WRITE] =
							par->usd_phandling;
					d.latencies[LAT_DUR_PROXY_CLIENT_WRITE_TO_PROXY_RX] =
						us - par->ust_pwait;

					lws_det_lat_cb(context, &d);
				}
#endif
			}

			if (n) {
				cp += n;
				par->rem = (uint16_t)(par->rem - (uint16_t)(unsigned int)n);
				len = (len + 1) - (unsigned int)n;
				/*
				 * if we didn't consume it all, we'll come
				 * around again and produce more dsh entries up
				 * to 1380 each until it is gone
				 */
			}
			if (!par->rem)
				par->ps = RPAR_TYPE;
			break;

		case RPAR_RX_TXCR_UPDATE:
			if (!--par->rem && par->ctr != 3)
				goto hangup;

			par->temp32 = (par->temp32 << 8) | *cp++;
			if (++par->ctr < 4)
				break;

			/*
			 * Proxy is telling us remote endpoint is allowing us
			 * par->temp32 more bytes tx credit to write to it
			 */

			h = lws_container_of(par, lws_sspc_handle_t, parser);
			h->txc.tx_cr += par->temp32;
			lwsl_info("%s: RX_PEER_TXCR: %d\n", __func__, par->temp32);
			lws_sspc_request_tx(h); /* in case something waiting */
			par->ctr = 0;
			par->ps = RPAR_TYPE;
			break;

		case RPAR_INIT_PROVERS:
			/* Protocol version byte for this connection */
			par->protocol_version = *cp++;

			/*
			 * So we have to know what versions of the serialization
			 * protocol we can support at the proxy side, and
			 * reject anythng we don't know how to deal with
			 * noisily in the logs.
			 */

			if (par->protocol_version != 1) {
				lwsl_err("%s: Rejecting client with "
					 "unsupported SSv%d protocol\n",
					 __func__, par->protocol_version);

				goto hangup;
			}

			if (!--par->rem)
				goto hangup;
			par->ctr = 0;
			par->ps = RPAR_INIT_PID;
			break;


		case RPAR_INIT_PID:
			if (!--par->rem)
				goto hangup;

			par->temp32 = (par->temp32 << 8) | *cp++;
			if (++par->ctr < 4)
				break;

			par->client_pid = (uint32_t)par->temp32;
			par->ctr = 0;
			par->ps = RPAR_INITTXC0;
			break;

		case RPAR_INITTXC0:
			if (!--par->rem)
				goto hangup;

			par->temp32 = (par->temp32 << 8) | *cp++;
			if (++par->ctr < 4)
				break;

			par->txcr_out = par->temp32;
			par->ctr = 0;
			par->ps = RPAR_STREAMTYPE;
			break;

		/*
		 * These are the client adjusting our / the remote peer ability
		 * to send back to him. He's sending a signed u32 BE
		 */

		case RPAR_TXCR0:

			par->temp32 = (par->temp32 << 8) | *cp++;
			if (++par->ctr < 4) {
				if (!--par->rem)
					goto hangup;
				break;
			}

			if (--par->rem)
				goto hangup;

			if (!client) {
				/*
				 * We're the proxy, being told by the client
				 * that it wants to allow more tx from the peer
				 * on the onward connection towards it.
				 */
#if defined(LWS_ROLE_H2) || defined(LWS_ROLE_MQTT)
				if (proxy_pss_to_ss_h(pss) &&
				    proxy_pss_to_ss_h(pss)->wsi) {
					lws_wsi_tx_credit(
						proxy_pss_to_ss_h(pss)->wsi,
							  LWSTXCR_PEER_TO_US,
							  par->temp32);
					lwsl_notice("%s: proxy RX_PEER_TXCR: +%d (est %d)\n",
						 __func__, par->temp32,
						 proxy_pss_to_ss_h(pss)->wsi->
							 txc.peer_tx_cr_est);
					_lws_ss_request_tx(proxy_pss_to_ss_h(pss));
				} else
#endif
					lwsl_info("%s: dropping TXCR\n", __func__);
			} else {
				/*
				 * We're the client, being told by the proxy
				 * about tx credit being given to us from the
				 * remote peer, allowing the client to write to
				 * it.
				 */
				h = lws_container_of(par, lws_sspc_handle_t,
						     parser);
				h->txc.tx_cr += par->temp32;
				lwsl_info("%s: client RX_PEER_TXCR: %d\n",
							__func__, par->temp32);
				lws_sspc_request_tx(h); /* in case something waiting */
			}
			par->ps = RPAR_TYPE;
			break;

		case RPAR_TIMEOUT0:

			par->temp32 = (par->temp32 << 8) | *cp++;
			if (++par->ctr < 4) {
				if (!--par->rem)
					goto hangup;
				break;
			}

			if (--par->rem)
				goto hangup;

			/*
			 * Proxy...
			 *
			 * *pss may have gone away asynchronously inbetweentimes
			 */

			if (proxy_pss_to_ss_h(pss)) {

				if ((unsigned int)par->temp32 == 0xffffffff) {
					lwsl_notice("%s: cancel ss timeout\n",
							__func__);
					lws_ss_cancel_timeout(
						proxy_pss_to_ss_h(pss));
				} else {

					if (!par->temp32)
						par->temp32 = (int)
						   proxy_pss_to_ss_h(pss)->
							   policy->timeout_ms;

					lwsl_notice("%s: set ss timeout for +%ums\n",
						__func__, par->temp32);

					lws_ss_start_timeout(
						proxy_pss_to_ss_h(pss), (unsigned int)
								par->temp32);
				}
			}

			par->ps = RPAR_TYPE;
			break;

		case RPAR_PAYLEN0:
			/*
			 * It's the length from lws_ss_request_tx_len() being
			 * passed up to the proxy
			 */
			par->temp32 = (par->temp32 << 8) | *cp++;
			if (++par->ctr < 4) {
				if (!--par->rem)
					goto hangup;
				break;
			}

			if (--par->rem)
				goto hangup;

			lwsl_notice("%s: set payload len %u\n", __func__,
				    par->temp32);

			par->ps = RPAR_TYPE;

			if (proxy_pss_to_ss_h(pss)) {
				r = lws_ss_request_tx_len(proxy_pss_to_ss_h(pss),
							(unsigned long)par->temp32);
				if (r == LWSSSSRET_DESTROY_ME)
					goto hangup;
			}
			break;

		case RPAR_METADATA_NAMELEN:
			/* both client and proxy */
			if (!--par->rem)
				goto hangup;
			par->slen = *cp++;
			if (par->slen >= sizeof(par->metadata_name) - 1)
				goto hangup;
			par->ctr = 0;
			par->ps++;
			break;

		case RPAR_METADATA_NAME:
			/* both client and proxy */
			if (!--par->rem)
				goto hangup;
			par->metadata_name[par->ctr++] = (char)*cp++;
			if (par->ctr != par->slen)
				break;
			par->metadata_name[par->ctr] = '\0';
			par->ps = RPAR_METADATA_VALUE;

			if (client) {
				lws_sspc_metadata_t *md;
				lws_sspc_handle_t *h =
						client_pss_to_sspc_h(pss, ssi);

				/*
				 * client side does not have access to policy
				 * and any metadata are new to it each time,
				 * we allocate them, removing any existing with
				 * the same name first
				 */

				lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
						lws_dll2_get_head(
							&h->metadata_owner_rx)) {
					md = lws_container_of(d,
						   lws_sspc_metadata_t, list);

					if (!strcmp(md->name,
						    par->metadata_name)) {
						lws_dll2_remove(&md->list);
						lws_free(md);
					}

				} lws_end_foreach_dll_safe(d, d1);

				/*
				 * Create the client's rx metadata entry
				 */

				if (h && lws_fi(&h->fic, "sspc_rx_metadata_oom"))
					md = NULL;
				else
					md = lws_malloc(sizeof(lws_sspc_metadata_t) +
						par->rem + 1, "rxmeta");
				if (!md) {
					lwsl_err("%s: OOM\n", __func__);
					goto hangup;
				}

				if (!h)
					/* coverity */
					goto hangup;

				memset(md, 0, sizeof(lws_sspc_metadata_t));

				lws_strncpy(md->name, par->metadata_name,
						sizeof(md->name));
				md->len = par->rem;
				par->rxmetaval = (uint8_t *)&md[1];
				/*
				 * Overallocate by 1 and put a NUL just beyond
				 * the official md->len, so value can be easily
				 * dereferenced safely for NUL-terminated string
				 * apis that's the most common usage
				 */
				par->rxmetaval[md->len] = '\0';
				lws_dll2_add_tail(&md->list,
						  &h->metadata_owner_rx);
				par->ctr = 0;
				break;
			}

			/* proxy side is receiving it */

			if (!proxy_pss_to_ss_h(pss))
				goto hangup;

			if (!proxy_pss_to_ss_h(pss)->policy) {
				lwsl_err("%s: null policy\n", __func__);
				goto hangup;
			}

			/*
			 * This is the policy's metadata list for the given
			 * name
			 */
			pm = lws_ss_policy_metadata(
					proxy_pss_to_ss_h(pss)->policy,
					par->metadata_name);
			if (!pm) {
				lwsl_err("%s: metadata %s not in proxy policy\n",
					 __func__, par->metadata_name);

				goto hangup;
			}

			par->ssmd = lws_ss_get_handle_metadata(
					proxy_pss_to_ss_h(pss),
					par->metadata_name);

			if (par->ssmd) {

				if (par->ssmd->value_on_lws_heap)
					lws_free_set_NULL(par->ssmd->value__may_own_heap);
				par->ssmd->value_on_lws_heap = 0;

				if (proxy_pss_to_ss_h(pss) &&
				    lws_fi(&proxy_pss_to_ss_h(pss)->fic, "ssproxy_rx_metadata_oom"))
					par->ssmd->value__may_own_heap = NULL;
				else
					par->ssmd->value__may_own_heap =
						lws_malloc((unsigned int)par->rem + 1, "metadata");

				if (!par->ssmd->value__may_own_heap) {
					lwsl_err("%s: OOM mdv\n", __func__);
					goto hangup;
				}
				par->ssmd->length = par->rem;
				((uint8_t *)par->ssmd->value__may_own_heap)[par->rem] = '\0';
				/* mark it as needing cleanup */
				par->ssmd->value_on_lws_heap = 1;
			}
			par->ctr = 0;
			break;

		case RPAR_METADATA_VALUE:
			/* both client and proxy */

			if (client) {
				*par->rxmetaval++ = *cp++;
			} else {

				if (!par->ssmd) {
					/* we don't recognize the name */

					cp++;

					if (--par->rem)
						break;

					par->ps = RPAR_TYPE;
					break;
				}

				((uint8_t *)(par->ssmd->value__may_own_heap))[par->ctr++] = *cp++;
			}

			if (--par->rem)
				break;

			/* we think we got all the value */
			if (client) {
				h = lws_container_of(par, lws_sspc_handle_t, parser);
				lwsl_sspc_notice(h, "RX METADATA %s",
							par->metadata_name);
			} else {
				lwsl_ss_info(proxy_pss_to_ss_h(pss),
					     "RPAR_METADATA_VALUE for %s (len %d)",
					     par->ssmd->name,
					     (int)par->ssmd->length);
				lwsl_hexdump_ss_info(proxy_pss_to_ss_h(pss),
						par->ssmd->value__may_own_heap,
						par->ssmd->length);
			}
			par->ps = RPAR_TYPE;
			break;

		case RPAR_STREAMTYPE:

			/* only the proxy can get these */

			if (client)
				goto hangup;
			if (par->ctr == sizeof(par->streamtype) - 1)
				goto hangup;

			/*
			 * We can only expect to get this if we ourselves are
			 * in the state that we're waiting for it.  If it comes
			 * later it's a protocol error.
			 */

			if (*state != LPCSPROX_WAIT_INITIAL_TX)
				goto hangup;

			/*
			 * We're the proxy, creating an SS on behalf of a
			 * client
			 */

			par->streamtype[par->ctr++] = (char)*cp++;
			if (--par->rem)
				break;

			par->ps = RPAR_TYPE;
			par->streamtype[par->ctr] = '\0';
			lwsl_info("%s: proxy ss '%s', sssv%d, txcr %d\n",
				    __func__, par->streamtype,
				    par->protocol_version, par->txcr_out);

			ssi->streamtype = par->streamtype;
			if (par->txcr_out) // !!!
				ssi->manual_initial_tx_credit = par->txcr_out;

			/*
			 * Even for a synthetic SS proxing action like _lws_smd,
			 * we create an actual SS in the proxy representing the
			 * connection
			 */

			ssi->flags |= LWSSSINFLAGS_PROXIED;
			ssi->sss_protocol_version = par->protocol_version;
			ssi->client_pid = par->client_pid;

			if (lws_ss_create(context, 0, ssi, parconn, pss,
					  NULL, NULL)) {
				/*
				 * We're unable to create the onward secure
				 * stream he asked for... schedule a chance to
				 * inform him
				 */
				lwsl_err("%s: create '%s' fail\n", __func__,
					 par->streamtype);
				*state = LPCSPROX_REPORTING_FAIL;
				break;
			} else {
				lwsl_debug("%s: create '%s' OK\n",
					__func__, par->streamtype);
				*state = LPCSPROX_REPORTING_OK;
			}

			if (*pss) {
				(*pss)->being_serialized = 1;
#if defined(LWS_WITH_SYS_SMD)
				if ((*pss)->policy != &pol_smd)
					/*
					 * In SMD case we overloaded the
					 * initial credit to be the class mask
					 */
#endif
				{
					lwsl_info("%s: Created SS initial credit %d\n",
						__func__, par->txcr_out);

					(*pss)->info.manual_initial_tx_credit = par->txcr_out;
				}
			}

			/* parent needs to schedule write on client conn */
			break;

		/* clientside states */

		case RPAR_RESULT_CREATION:
			if (*cp++) {
				lwsl_err("%s: stream creation failed\n",
					 __func__);
				goto hangup;
			}

			if (--par->rem < 4)
				goto hangup;

			par->ps = RPAR_RESULT_CREATION_DSH;
			par->ctr = 0;
			break;

		case RPAR_RESULT_CREATION_DSH:

			par->temp32 = (par->temp32 << 8) | (*cp++);
			if (!par->rem--)
				goto hangup;
			if (++par->ctr < 4)
				break;

			/*
			 * Client (par->temp32 == dsh alloc)
			 */

			h = lws_container_of(par, lws_sspc_handle_t, parser);

			lws_ss_serialize_state_transition(h, state,
							  LPCSCLI_LOCAL_CONNECTED);

			lws_set_timeout(h->cwsi, NO_PENDING_TIMEOUT, 0);

			if (h->dsh)
				goto hangup;

			/*
			 * This is telling us that the streamtype could be (and
			 * was) created at the proxy.  It's not telling us that
			 * the onward peer connection could be connected.
			 *
			 * We'll get a proxied state() coming later that informs
			 * us about the situation with that.
			 *
			 * However at this point, we should choose to inform
			 * the client that his stream was created... we will
			 * later get a proxied CREATING state from the peer
			 * but we should do it now and suppress the later one.
			 *
			 * The reason is he may set metadata in CREATING, and
			 * we will try to do writeables to sync the stream to
			 * proxy and ultimately bring up the onward connection
			 * now we are in LOCAL_CONNECTED.  We need to do the
			 * CREATING now so we'll know the metadata to sync.
			 */

#if defined(LWS_WITH_SYS_METRICS)
			/*
			 * If any hanging caliper measurement, dump it, and free any tags
			 */
			lws_metrics_caliper_report_hist(h->cal_txn, (struct lws *)NULL);
#endif

			if (!h->creating_cb_done) {
				if (lws_ss_check_next_state_sspc(h,
							       &h->prev_ss_state,
							       LWSSSCS_CREATING))
					return LWSSSSRET_DESTROY_ME;
				h->prev_ss_state = (uint8_t)LWSSSCS_CREATING;
				h->creating_cb_done = 1;
			} else
				h->prev_ss_state = LWSSSCS_DISCONNECTED;

			if (ssi->state) {
				n = ssi->state(client_pss_to_userdata(pss),
					       NULL, h->prev_ss_state, 0);
				switch (n) {
				case LWSSSSRET_OK:
					break;
				case LWSSSSRET_DISCONNECT_ME:
					goto hangup;
				case LWSSSSRET_DESTROY_ME:
					return LWSSSSRET_DESTROY_ME;
				}
			}

			h->dsh = lws_dsh_create(NULL, (size_t)(par->temp32 ?
						par->temp32 : 32768), 1);
			if (!h->dsh)
				goto hangup;

			lws_callback_on_writable(h->cwsi);

			par->rsl_pos = 0;
			par->rsl_idx = 0;

			memset(&h->rideshare_ofs[0], 0, sizeof(h->rideshare_ofs[0]));
			h->rideshare_list[0] = '\0';
			h->rsidx = 0;

			/* no rideshare data is OK */
			par->ps = RPAR_TYPE;

			if (par->rem) {
				par->ps = RPAR_RESULT_CREATION_RIDESHARE;
				if (par->rem >= sizeof(h->rideshare_list))
					goto hangup;
			}
			break;

		case RPAR_RESULT_CREATION_RIDESHARE:
			h = lws_container_of(par, lws_sspc_handle_t, parser);
			if (*cp == ',') {
				cp++;
				h->rideshare_list[par->rsl_pos++] = '\0';
				if (par->rsl_idx == LWS_ARRAY_SIZE(h->rideshare_ofs))
					goto hangup;
				h->rideshare_ofs[++par->rsl_idx] = par->rsl_pos;
			} else
				h->rideshare_list[par->rsl_pos++] = (char)*cp++;
			if (!--par->rem)
				par->ps = RPAR_TYPE;
			break;

		case RPAR_STATEINDEX:
			par->ctr = (par->ctr << 8) | (*cp++);
			if (--par->rem == 4)
				par->ps = RPAR_ORD3;
			break;

		case RPAR_ORD3:
			par->flags = (uint32_t)((*cp++) << 24);
			par->ps++;
			break;

		case RPAR_ORD2:
			par->flags |= (uint32_t)((*cp++) << 16);
			par->ps++;
			break;

		case RPAR_ORD1:
			par->flags |= (uint32_t)((*cp++) << 8);
			par->ps++;
			break;

		case RPAR_ORD0:
			par->flags |= (uint32_t)(*cp++);
			par->ps++;
			par->ps = RPAR_TYPE;

			/*
			 * Client received a proxied state change
			 */

			h = client_pss_to_sspc_h(pss, ssi);
			if (!h)
				/*
				 * Since we're being informed we need to have
				 * a stream to inform.  Assume whatever set this
				 * to NULL has started to close it.
				 */
				break;

			switch (par->ctr) {
			case LWSSSCS_DISCONNECTED:
			case LWSSSCS_UNREACHABLE:
			case LWSSSCS_AUTH_FAILED:
				lws_ss_serialize_state_transition(h, state,
						LPCSCLI_LOCAL_CONNECTED);
				h->conn_req_state = LWSSSPC_ONW_NONE;
				break;

			case LWSSSCS_CONNECTED:
				lwsl_sspc_info(h, "CONNECTED %s",
							ssi->streamtype);
				if (*state == LPCSCLI_OPERATIONAL)
					/*
					 * Don't allow to see connected more
					 * than once for one connection
					 */
					goto swallow;

				lws_ss_serialize_state_transition(h, state,
							LPCSCLI_OPERATIONAL);

				h->conn_req_state = LWSSSPC_ONW_CONN;
				break;
			case LWSSSCS_TIMEOUT:
				break;
			default:
				break;
			}

			if (par->ctr < 0)
				goto hangup;

#if defined(_DEBUG)
			lwsl_sspc_info(h, "forwarding proxied state %s",
					lws_ss_state_name(par->ctr));
#endif

			if (par->ctr == LWSSSCS_CREATING) {
				h = lws_container_of(par, lws_sspc_handle_t, parser);
				if (h->creating_cb_done)
					/*
					 * We have told him he's CREATING when
					 * we heard we had linked up to the
					 * proxy, so suppress the remote
					 * CREATING so that he only sees it once
					 */
				break;

				h->creating_cb_done = 1;
			}

			if (ssi->state) {
				h = lws_container_of(par, lws_sspc_handle_t, parser);
				lws_ss_constate_t cs = (lws_ss_constate_t)par->ctr;

				if (cs == LWSSSCS_CONNECTED)
					h->ss_dangling_connected = 1;
				if (cs == LWSSSCS_DISCONNECTED)
					h->ss_dangling_connected = 0;

				if (lws_ss_check_next_state_sspc(h,
							    &h->prev_ss_state, cs))
					return LWSSSSRET_DESTROY_ME;

				if (cs < LWSSSCS_USER_BASE)
					h->prev_ss_state = (uint8_t)cs;

				h->h_in_svc = h;
				n = ssi->state(client_pss_to_userdata(pss),
					NULL, cs, par->flags);
				h->h_in_svc = NULL;
				switch (n) {
				case LWSSSSRET_OK:
					break;
				case LWSSSSRET_DISCONNECT_ME:
					goto hangup;
				case LWSSSSRET_DESTROY_ME:
					return LWSSSSRET_DESTROY_ME;
				}
			}

swallow:
			break;

		default:
			goto hangup;
		}
	}

	return LWSSSSRET_OK;

hangup:

	lwsl_cx_notice(context, "hangup");

	return LWSSSSRET_DISCONNECT_ME;
}
