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

typedef enum {
	LDHC_INIT_REBOOT,
	LDHC_REBOOTING,		/* jitterwait */
	LDHC_INIT,		/* issue DHCPDISCOVER */
	LDHC_SELECTING,
	LDHC_REQUESTING,
	LDHC_REBINDING,
	LDHC_BOUND,
	LDHC_RENEWING
} lws_dhcpc_state_t;

enum {
	LWSDHC4PDISCOVER		= 1,
	LWSDHC4POFFER,
	LWSDHC4PREQUEST,
	LWSDHC4PDECLINE,
	LWSDHC4PACK,
	LWSDHC4PNACK,
	LWSDHC4PRELEASE,

	LWSDHC4POPT_PAD			= 0,
	LWSDHC4POPT_SUBNET_MASK		= 1,
	LWSDHC4POPT_TIME_OFFSET		= 2,
	LWSDHC4POPT_ROUTER		= 3,
	LWSDHC4POPT_TIME_SERVER		= 4,
	LWSDHC4POPT_NAME_SERVER		= 5,
	LWSDHC4POPT_DNSERVER		= 6,
	LWSDHC4POPT_LOG_SERVER		= 7,
	LWSDHC4POPT_COOKIE_SERVER	= 8,
	LWSDHC4POPT_LPR_SERVER		= 9,
	LWSDHC4POPT_IMPRESS_SERVER	= 10,
	LWSDHC4POPT_RESLOC_SERVER	= 11,
	LWSDHC4POPT_HOST_NAME		= 12,
	LWSDHC4POPT_BOOTFILE_SIZE	= 13,
	LWSDHC4POPT_MERIT_DUMP_FILE	= 14,
	LWSDHC4POPT_DOMAIN_NAME		= 15,
	LWSDHC4POPT_SWAP_SERVER		= 16,
	LWSDHC4POPT_ROOT_PATH		= 17,
	LWSDHC4POPT_EXTENSIONS_PATH	= 18,
	LWSDHC4POPT_BROADCAST_ADS	= 28,

	LWSDHC4POPT_REQUESTED_ADS	= 50,
	LWSDHC4POPT_LEASE_TIME		= 51,
	LWSDHC4POPT_OPTION_OVERLOAD	= 52,
	LWSDHC4POPT_MESSAGE_TYPE		= 53,
	LWSDHC4POPT_SERVER_ID		= 54,
	LWSDHC4POPT_PARAM_REQ_LIST	= 55,
	LWSDHC4POPT_MESSAGE		= 56,
	LWSDHC4POPT_MAX_DHCP_MSG_SIZE	= 57,
	LWSDHC4POPT_RENEWAL_TIME		= 58, /* AKA T1 */
	LWSDHC4POPT_REBINDING_TIME	= 59, /* AKA T2 */
	LWSDHC4POPT_VENDOR_CLASS_ID	= 60,
	LWSDHC4POPT_CLIENT_ID		= 61,

	LWSDHC4POPT_END_OPTIONS		= 255
};

typedef struct lws_dhcpc_req {
	lws_dll2_t		list;

	struct lws_context	*context;
	lws_sorted_usec_list_t	sul_renew;
	lws_sorted_usec_list_t 	sul_conn;
	lws_sorted_usec_list_t 	sul_write;
	dhcpc_cb_t		cb;	    /* cb on completion / failure */
	void			*opaque;    /* ignored by lws, give to cb */

	/* these are separated so we can close the bcast one asynchronously */
	struct lws		*wsi_raw;   /* for broadcast */
	lws_dhcpc_state_t	state;

	lws_dhcpc_ifstate_t	is;

	uint16_t		retry_count_conn;
	uint16_t		retry_count_write;
	uint8_t			xid[4];
	uint8_t			af;	    /* address family */
} lws_dhcpc_req_t;
/* interface name is overallocated here */

void
lws_dhcpc4_retry_conn(struct lws_sorted_usec_list *sul);

int
lws_dhcpc4_parse(lws_dhcpc_req_t *r, void *in, size_t len);

void
lws_dhcpc_retry_write(struct lws_sorted_usec_list *sul);
