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

#include "libwebsockets.h"
#include "lws-ssh.h"

#include <string.h>

struct per_vhost_data__telnet {
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;
	struct per_session_data__telnet *live_pss_list;
	const struct lws_ssh_ops *ops;
};

struct per_session_data__telnet {
	struct per_session_data__telnet *next;
	struct per_vhost_data__telnet *vhd;
	uint32_t rx_tail;
	void *priv;

	uint32_t initial:1;

	char state;
	uint8_t cmd;
};

enum {
	LTS_BINARY_XMIT,
	LTS_ECHO,
	LTS_SUPPRESS_GA,


	LTSC_SUBOPT_END		= 240,
	LTSC_BREAK		= 243,
	LTSC_SUBOPT_START	= 250,
	LTSC_WILL		= 251,
	LTSC_WONT,
	LTSC_DO,
	LTSC_DONT,
	LTSC_IAC,

	LTST_WAIT_IAC		= 0,
	LTST_GOT_IAC,
	LTST_WAIT_OPT,
};

static int
telnet_ld(struct per_session_data__telnet *pss, uint8_t c)
{
	switch (pss->state) {
	case LTST_WAIT_IAC:
		if (c == LTSC_IAC) {
			pss->state = LTST_GOT_IAC;
			return 0;
		}
		return 1;

	case LTST_GOT_IAC:
		pss->state = LTST_WAIT_IAC;

		switch (c) {
		case LTSC_BREAK:
			return 0;
		case LTSC_WILL:
		case LTSC_WONT:
		case LTSC_DO:
		case LTSC_DONT:
			pss->cmd = c;
			pss->state = LTST_WAIT_OPT;
			return 0;
		case LTSC_IAC:
			return 1; /* double IAC */
		}
		return 0; /* ignore unknown */

	case LTST_WAIT_OPT:
		lwsl_notice(" tld: cmd %d: opt %d\n", pss->cmd, c);
		pss->state = LTST_WAIT_IAC;
		return 0;	
	}

	return 0;
}

static uint8_t init[] = {
	LTSC_IAC, LTSC_WILL, 3,
	LTSC_IAC, LTSC_WILL, 1,
	LTSC_IAC, LTSC_DONT, 1,
	LTSC_IAC, LTSC_DO,   0
};

static int
lws_callback_raw_telnet(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct per_session_data__telnet *pss =
			(struct per_session_data__telnet *)user, **p;
	struct per_vhost_data__telnet *vhd =
			(struct per_vhost_data__telnet *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
					lws_get_protocol(wsi));
	const struct lws_protocol_vhost_options *pvo =
			(const struct lws_protocol_vhost_options *)in;
	int n, m;
	uint8_t buf[LWS_PRE + 800], *pu = in;

	switch ((int)reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct per_vhost_data__telnet));
		vhd->context = lws_get_context(wsi);
		vhd->protocol = lws_get_protocol(wsi);
		vhd->vhost = lws_get_vhost(wsi);

		while (pvo) {
			if (!strcmp(pvo->name, "ops"))
				vhd->ops = (const struct lws_ssh_ops *)pvo->value;

			pvo = pvo->next;
		}

		if (!vhd->ops) {
			lwsl_err("telnet pvo \"ops\" is mandatory\n");
			return -1;
		}
		break;

        case LWS_CALLBACK_RAW_ADOPT:
		pss->next = vhd->live_pss_list;
		vhd->live_pss_list = pss;
		pss->vhd = vhd;
		pss->state = LTST_WAIT_IAC;
		pss->initial = 0;
		if (vhd->ops->channel_create)
			vhd->ops->channel_create(wsi, &pss->priv);
		lws_callback_on_writable(wsi);
                break;

	case LWS_CALLBACK_RAW_CLOSE:
		p = &vhd->live_pss_list;

		while (*p) {
			if ((*p) == pss) {
				if (vhd->ops->channel_destroy)
					vhd->ops->channel_destroy(pss->priv);
				*p = pss->next;
				continue;
			}
			p = &((*p)->next);
		}
		break;

	case LWS_CALLBACK_RAW_RX:
		n = 0;

		/* this stuff is coming in telnet line discipline, we
		 * have to strip IACs and process IAC repeats */

		while (len--) {
			if (telnet_ld(pss, *pu))
				buf[n++] = *pu++;
			else
				pu++;

			if (n > 100 || !len)
				pss->vhd->ops->rx(pss->priv, wsi, buf, (uint32_t)n);
		}
		break;

        case LWS_CALLBACK_RAW_WRITEABLE:
		n = 0;
		if (!pss->initial) {
			memcpy(buf + LWS_PRE, init, sizeof(init));

			n = sizeof(init);
			pss->initial = 1;
		} else {
			/* bring any waiting tx into second half of buffer
			 * restrict how much we can send to 1/4 of the buffer,
			 * because we have to apply telnet line discipline...
			 * in the worst case of all 0xff, doubling the size
			 */
			pu = buf + LWS_PRE + 400;
			m = (int)pss->vhd->ops->tx(pss->priv, LWS_STDOUT, pu,
					(size_t)((int)sizeof(buf) - LWS_PRE - n - 401) / 2);

			/*
			 * apply telnet line discipline and copy into place
			 * in output buffer
			 */
			while (m--) {
				if (*pu == 0xff)
					buf[LWS_PRE + n++] = 0xff;
				buf[LWS_PRE + n++] = *pu++;
			}
		}
		if (n > 0) {
			m = lws_write(wsi, (unsigned char *)buf + LWS_PRE, (unsigned int)n,
				      LWS_WRITE_HTTP);
	                if (m < 0) {
	                        lwsl_err("ERROR %d writing to di socket\n", m);
	                        return -1;
	                }
		}

		if (vhd->ops->tx_waiting(&pss->priv))
		       lws_callback_on_writable(wsi);
		break;

        case LWS_CALLBACK_SSH_UART_SET_RXFLOW:
        	/*
        	 * this is sent to set rxflow state on any connections that
        	 * sink on a particular uart.  The uart index affected is in len
        	 *
        	 * More than one protocol may sink to the same uart, and the
        	 * protocol may select the uart itself, eg, in the URL used
        	 * to set up the connection.
        	 */
        	lws_rx_flow_control(wsi, len & 1);
        	break;

	default:
		break;
	}

	return 0;
}

const struct lws_protocols protocols_telnet[] = {
	{
		"lws-telnetd-base",
		lws_callback_raw_telnet,
		sizeof(struct per_session_data__telnet),
		1024, 0, NULL, 900
	},
	{ NULL, NULL, 0, 0, 0, NULL, 0 } /* terminator */
};


