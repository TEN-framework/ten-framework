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

#if !defined (LWS_PLUGIN_STATIC)
#if !defined(LWS_DLL)
#define LWS_DLL
#endif
#if !defined(LWS_INTERNAL)
#define LWS_INTERNAL
#endif
#include <libwebsockets.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <fcntl.h>

#define RING_DEPTH 8

struct packet {
	void *payload;
	uint32_t len;
	uint32_t ticket;
};

enum {
	ACC,
	ONW
};

/*
 * Because both sides of the connection want to share this, we allocate it
 * during accepted adoption and both sides have a pss that is just a wrapper
 * pointing to this.
 *
 * The last one of the accepted side and the onward side to close frees it.
 * This removes any chance of one side or the other having an invalidated
 * pointer to the pss.
 */

struct conn {
	struct lws *wsi[2];

	/* rings containing unsent rx from accepted and onward sides */
	struct lws_ring *r[2];
	uint32_t t[2]; /* ring tail */

	uint32_t ticket_next;
	uint32_t ticket_retired;

	char rx_enabled[2];
	char closed[2];
	char established[2];
};

struct raw_pss {
	struct conn *conn;
};

/* one of these is created for each vhost our protocol is used with */

struct raw_vhd {
	char addr[128];
	uint16_t port;
	char ipv6;
};

static void
__destroy_packet(void *_pkt)
{
	struct packet *pkt = _pkt;

	free(pkt->payload);
	pkt->payload = NULL;
	pkt->len = 0;
}

static void
destroy_conn(struct raw_vhd *vhd, struct raw_pss *pss)
{
	struct conn *conn = pss->conn;

	if (conn->r[ACC])
		lws_ring_destroy(conn->r[ACC]);
	if (conn->r[ONW])
		lws_ring_destroy(conn->r[ONW]);

	pss->conn = NULL;

	free(conn);
}

static int
connect_client(struct raw_vhd *vhd, struct raw_pss *pss)
{
	struct lws_client_connect_info i;
	char host[128];
	struct lws *cwsi;

	lws_snprintf(host, sizeof(host), "%s:%u", vhd->addr, vhd->port);

	memset(&i, 0, sizeof(i));

	i.method = "RAW";
	i.context = lws_get_context(pss->conn->wsi[ACC]);
	i.port = vhd->port;
	i.address = vhd->addr;
	i.host = host;
	i.origin = host;
	i.ssl_connection = 0;
	i.vhost = lws_get_vhost(pss->conn->wsi[ACC]);
	i.local_protocol_name = "raw-proxy";
	i.protocol = "raw-proxy";
	i.path = "/";
	/*
	 * The "onward" client wsi has its own pss but shares the "conn"
	 * created when the inbound connection was accepted.  We need to stash
	 * the address of the shared conn and apply it to the client psss
	 * when the client connection completes.
	 */
	i.opaque_user_data = pss->conn;
	i.pwsi = &pss->conn->wsi[ONW];

	lwsl_info("%s: onward: %s:%d%s\n", __func__, i.address, i.port, i.path);

	cwsi = lws_client_connect_via_info(&i);
	if (!cwsi)
		lwsl_err("%s: client connect failed early\n", __func__);

	return !cwsi;
}

static int
flow_control(struct conn *conn, int side, int enable)
{
	if (conn->closed[side] ||
	    enable == conn->rx_enabled[side] ||
	    !conn->established[side])
		return 0;

	if (lws_rx_flow_control(conn->wsi[side], enable))
		return 1;

	conn->rx_enabled[side] = (char)enable;
	lwsl_info("%s: %s side: %s\n", __func__, side ? "ONW" : "ACC",
		  enable ? "rx enabled" : "rx flow controlled");

	return 0;
}

static int
callback_raw_proxy(struct lws *wsi, enum lws_callback_reasons reason,
		   void *user, void *in, size_t len)
{
	struct raw_pss *pss = (struct raw_pss *)user;
	struct raw_vhd *vhd = (struct raw_vhd *)lws_protocol_vh_priv_get(
				     lws_get_vhost(wsi), lws_get_protocol(wsi));
	const struct packet *ppkt;
	struct conn *conn = NULL;
	struct lws_tokenize ts;
	lws_tokenize_elem e;
	struct packet pkt;
	const char *cp;
	int n;

	if (pss)
		conn = pss->conn;

	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi), sizeof(struct raw_vhd));
		if (!vhd)
			return 0;
		if (lws_pvo_get_str(in, "onward", &cp)) {
			lwsl_warn("%s: vh %s: pvo 'onward' required\n", __func__,
				 lws_get_vhost_name(lws_get_vhost(wsi)));

			return 0;
		}
		lws_tokenize_init(&ts, cp, LWS_TOKENIZE_F_DOT_NONTERM |
					   LWS_TOKENIZE_F_MINUS_NONTERM |
					   LWS_TOKENIZE_F_NO_FLOATS);
		ts.len = strlen(cp);

		if (lws_tokenize(&ts) != LWS_TOKZE_TOKEN)
			goto bad_onward;
		if (!strncmp(ts.token, "ipv6", ts.token_len))
			vhd->ipv6 = 1;
		else
			if (strncmp(ts.token, "ipv4", ts.token_len))
				goto bad_onward;

		/* then the colon */
		if (lws_tokenize(&ts) != LWS_TOKZE_DELIMITER)
			goto bad_onward;

		e = lws_tokenize(&ts);
		if (!vhd->ipv6) {
			if (e != LWS_TOKZE_TOKEN ||
			    ts.token_len + 1 >= (int)sizeof(vhd->addr))
				goto bad_onward;

			lws_strncpy(vhd->addr, ts.token, ts.token_len + 1);
			e = lws_tokenize(&ts);
			if (e == LWS_TOKZE_DELIMITER) {
				/* there should be a port then */
				e = lws_tokenize(&ts);
				if (e != LWS_TOKZE_INTEGER)
					goto bad_onward;
				vhd->port = (uint16_t)atoi(ts.token);
				e = lws_tokenize(&ts);
			}
			if (e != LWS_TOKZE_ENDED)
				goto bad_onward;
		} else
			lws_strncpy(vhd->addr, ts.token, sizeof(vhd->addr));

		lwsl_notice("%s: vh %s: onward %s:%s:%d\n", __func__,
			    lws_get_vhost_name(lws_get_vhost(wsi)),
			    vhd->ipv6 ? "ipv6": "ipv4", vhd->addr, vhd->port);
		break;

bad_onward:
		lwsl_err("%s: onward pvo format must be ipv4:addr[:port] "
			 " or ipv6:addr, not '%s'\n", __func__, cp);
		return -1;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		break;

	/* callbacks related to client "onward side" */

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		break;

        case LWS_CALLBACK_RAW_PROXY_CLI_ADOPT:
		lwsl_debug("%s: %p: LWS_CALLBACK_RAW_CLI_ADOPT: pss %p\n", __func__, wsi, pss);
		if (conn || !pss)
			break;
		conn = pss->conn = lws_get_opaque_user_data(wsi);
		if (!conn)
			break;
		conn->established[ONW] = 1;
		/* they start enabled */
		conn->rx_enabled[ACC] = 1;
		conn->rx_enabled[ONW] = 1;

		/* he disabled his rx while waiting for use to be established */
		flow_control(conn, ACC, 1);

		lws_callback_on_writable(wsi);
		lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);
		break;

	case LWS_CALLBACK_RAW_PROXY_CLI_CLOSE:
		lwsl_debug("LWS_CALLBACK_RAW_PROXY_CLI_CLOSE\n");
		if (!conn)
			break;

		conn->closed[ONW] = 1;

		if (conn->closed[ACC])
			destroy_conn(vhd, pss);

		break;

	case LWS_CALLBACK_RAW_PROXY_CLI_RX:
		lwsl_debug("LWS_CALLBACK_RAW_PROXY_CLI_RX: %d\n", (int)len);

		if (!conn)
			return 0;

		if (!pss || !conn->wsi[ACC] || conn->closed[ACC]) {
			lwsl_info(" pss %p, wsi[ACC] %p, closed[ACC] %d\n",
				  pss, conn->wsi[ACC], conn->closed[ACC]);
			return -1;
		}
		pkt.payload = malloc(len);
		if (!pkt.payload) {
			lwsl_notice("OOM: dropping\n");
			return -1;
		}
		pkt.len = (uint32_t)len;
		pkt.ticket = conn->ticket_next++;

		memcpy(pkt.payload, in, len);
		if (!lws_ring_insert(conn->r[ONW], &pkt, 1)) {
			__destroy_packet(&pkt);
			lwsl_notice("dropping!\n");
			return -1;
		}

		lwsl_debug("After onward RX: acc free: %d...\n",
			   (int)lws_ring_get_count_free_elements(conn->r[ONW]));

		if (conn->rx_enabled[ONW] &&
		    lws_ring_get_count_free_elements(conn->r[ONW]) < 2)
			flow_control(conn, ONW, 0);

		if (!conn->closed[ACC])
			lws_callback_on_writable(conn->wsi[ACC]);
		break;

	case LWS_CALLBACK_RAW_PROXY_CLI_WRITEABLE:
		lwsl_debug("LWS_CALLBACK_RAW_PROXY_CLI_WRITEABLE\n");

		if (!conn)
			break;

		ppkt = lws_ring_get_element(conn->r[ACC], &conn->t[ACC]);
		if (!ppkt) {
			lwsl_info("%s: CLI_WRITABLE had nothing in acc ring\n",
				  __func__);
			break;
		}

		if (ppkt->ticket != conn->ticket_retired + 1) {
			lwsl_info("%s: acc ring has %d but next %d\n", __func__,
				  ppkt->ticket, conn->ticket_retired + 1);
			lws_callback_on_writable(conn->wsi[ACC]);
			break;
		}

		n = lws_write(wsi, ppkt->payload, ppkt->len, LWS_WRITE_RAW);
		if (n < 0) {
			lwsl_info("%s: WRITEABLE: %d\n", __func__, n);

			return -1;
		}

		conn->ticket_retired = ppkt->ticket;
		lws_ring_consume(conn->r[ACC], &conn->t[ACC], NULL, 1);
		lws_ring_update_oldest_tail(conn->r[ACC], conn->t[ACC]);

		lwsl_debug("acc free: %d...\n",
			  (int)lws_ring_get_count_free_elements(conn->r[ACC]));

		if (!conn->rx_enabled[ACC] &&
		    lws_ring_get_count_free_elements(conn->r[ACC]) > 2)
			flow_control(conn, ACC, 1);

		ppkt = lws_ring_get_element(conn->r[ACC], &conn->t[ACC]);
		lwsl_debug("%s: CLI_WRITABLE: next acc pkt %p idx %d vs %d\n",
			   __func__, ppkt, ppkt ? ppkt->ticket : 0,
					   conn->ticket_retired + 1);

		if (ppkt && ppkt->ticket == conn->ticket_retired + 1)
			lws_callback_on_writable(wsi);
		else {
			/*
			 * defer checking for accepted side closing until we
			 * sent everything in the ring to onward
			 */
			if (conn->closed[ACC])
				/*
				 * there is never going to be any more... but
				 * we may have some tx still in tx buflist /
				 * partial
				 */
				return lws_raw_transaction_completed(wsi);

			if (lws_ring_get_element(conn->r[ONW], &conn->t[ONW]))
				lws_callback_on_writable(conn->wsi[ACC]);
		}
		break;

	/* callbacks related to raw socket descriptor "accepted side" */

        case LWS_CALLBACK_RAW_PROXY_SRV_ADOPT:
		lwsl_debug("LWS_CALLBACK_RAW_SRV_ADOPT\n");
		if (!pss)
			return -1;
		conn = pss->conn = malloc(sizeof(struct conn));
		if (!pss->conn)
			return -1;
		memset(conn, 0, sizeof(*conn));

		conn->wsi[ACC] = wsi;
		conn->ticket_next = 1;

		conn->r[ACC] = lws_ring_create(sizeof(struct packet),
					       RING_DEPTH, __destroy_packet);
		if (!conn->r[ACC]) {
			lwsl_err("%s: OOM\n", __func__);
			return -1;
		}
		conn->r[ONW] = lws_ring_create(sizeof(struct packet),
					       RING_DEPTH, __destroy_packet);
		if (!conn->r[ONW]) {
			lws_ring_destroy(conn->r[ACC]);
			conn->r[ACC] = NULL;
			lwsl_err("%s: OOM\n", __func__);

			return -1;
		}

		conn->established[ACC] = 1;

		/* disable any rx until the client side is up */
		flow_control(conn, ACC, 0);

		lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);

		/* try to create the onward client connection */
		connect_client(vhd, pss);
                break;

	case LWS_CALLBACK_RAW_PROXY_SRV_CLOSE:
		lwsl_debug("LWS_CALLBACK_RAW_PROXY_SRV_CLOSE:\n");

		if (!conn)
			break;

		conn->closed[ACC] = 1;
		if (conn->closed[ONW])
			destroy_conn(vhd, pss);
		break;

	case LWS_CALLBACK_RAW_PROXY_SRV_RX:
		lwsl_debug("LWS_CALLBACK_RAW_PROXY_SRV_RX: rx %d\n", (int)len);

		if (!conn || !conn->wsi[ONW]) {
			lwsl_err("%s: LWS_CALLBACK_RAW_PROXY_SRV_RX: "
				 "conn->wsi[ONW] NULL\n", __func__);
			return -1;
		}
		if (conn->closed[ONW]) {
			lwsl_info(" closed[ONW] %d\n", conn->closed[ONW]);
			return -1;
		}

		if (!len)
			return 0;

		pkt.payload = malloc(len);
		if (!pkt.payload) {
			lwsl_notice("OOM: dropping\n");
			return -1;
		}
		pkt.len = (uint32_t)len;
		pkt.ticket = conn->ticket_next++;

		memcpy(pkt.payload, in, len);
		if (!lws_ring_insert(conn->r[ACC], &pkt, 1)) {
			__destroy_packet(&pkt);
			lwsl_notice("dropping!\n");
			return -1;
		}

		lwsl_debug("After acc RX: acc free: %d...\n",
			   (int)lws_ring_get_count_free_elements(conn->r[ACC]));

		if (conn->rx_enabled[ACC] &&
		    lws_ring_get_count_free_elements(conn->r[ACC]) <= 2)
			flow_control(conn, ACC, 0);

		if (conn->established[ONW] && !conn->closed[ONW])
			lws_callback_on_writable(conn->wsi[ONW]);
		break;

	case LWS_CALLBACK_RAW_PROXY_SRV_WRITEABLE:
		lwsl_debug("LWS_CALLBACK_RAW_PROXY_SRV_WRITEABLE\n");

		if (!conn || !conn->established[ONW] || conn->closed[ONW])
			break;

		ppkt = lws_ring_get_element(conn->r[ONW], &conn->t[ONW]);
		if (!ppkt) {
			lwsl_info("%s: SRV_WRITABLE nothing in onw ring\n",
				  __func__);
			break;
		}

		if (ppkt->ticket != conn->ticket_retired + 1) {
			lwsl_info("%s: onw ring has %d but next %d\n", __func__,
				  ppkt->ticket, conn->ticket_retired + 1);
			lws_callback_on_writable(conn->wsi[ONW]);
			break;
		}

		n = lws_write(wsi, ppkt->payload, ppkt->len, LWS_WRITE_RAW);
		if (n < 0) {
			lwsl_info("%s: WRITEABLE: %d\n", __func__, n);

			return -1;
		}

		conn->ticket_retired = ppkt->ticket;
		lws_ring_consume(conn->r[ONW], &conn->t[ONW], NULL, 1);
		lws_ring_update_oldest_tail(conn->r[ONW], conn->t[ONW]);

		lwsl_debug("onward free: %d... waiting %d\n",
			  (int)lws_ring_get_count_free_elements(conn->r[ONW]),
			  (int)lws_ring_get_count_waiting_elements(conn->r[ONW],
								&conn->t[ONW]));

		if (!conn->rx_enabled[ONW] &&
		    lws_ring_get_count_free_elements(conn->r[ONW]) > 2)
			flow_control(conn, ONW, 1);

		ppkt = lws_ring_get_element(conn->r[ONW], &conn->t[ONW]);
		lwsl_debug("%s: SRV_WRITABLE: next onw pkt %p idx %d vs %d\n",
			   __func__, ppkt, ppkt ? ppkt->ticket : 0,
					   conn->ticket_retired + 1);

		if (ppkt && ppkt->ticket == conn->ticket_retired + 1)
			lws_callback_on_writable(wsi);
		else {
			/*
			 * defer checking for onward side closing until we
			 * sent everything in the ring to accepted side
			 */
			if (conn->closed[ONW])
				/*
				 * there is never going to be any more... but
				 * we may have some tx still in tx buflist /
				 * partial
				 */
				return lws_raw_transaction_completed(wsi);

		if (lws_ring_get_element(conn->r[ACC], &conn->t[ACC]))
			lws_callback_on_writable(conn->wsi[ONW]);
		}
		break;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

#define LWS_PLUGIN_PROTOCOL_RAW_PROXY { \
		"raw-proxy", \
		callback_raw_proxy, \
		sizeof(struct raw_pss), \
		8192, \
		8192, NULL, 0 \
	}

#if !defined (LWS_PLUGIN_STATIC)

LWS_VISIBLE const struct lws_protocols lws_raw_proxy_protocols[] = {
	LWS_PLUGIN_PROTOCOL_RAW_PROXY
};

LWS_VISIBLE const lws_plugin_protocol_t lws_raw_proxy = {
	.hdr = {
		"raw proxy",
		"lws_protocol_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.protocols = lws_raw_proxy_protocols,
	.count_protocols = LWS_ARRAY_SIZE(lws_raw_proxy_protocols),
	.extensions = NULL,
	.count_extensions = 0,
};
#endif


