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

typedef uint32_t lws_wsi_state_t;

/*
 * The wsi->role_ops pointer decides almost everything about what role the wsi
 * will play, h2, raw, ws, etc.
 *
 * However there are a few additional flags needed that vary, such as if the
 * role is a client or server side, if it has that concept.  And the connection
 * fulfilling the role, has a separate dynamic state.
 *
 *   31           16 15      0
 *   [  role flags ] [ state ]
 *
 * The role flags part is generally invariant for the lifetime of the wsi,
 * although it can change if the connection role itself does, eg, if the
 * connection upgrades from H1 -> WS1 the role flags may be changed at that
 * point.
 *
 * The state part reflects the dynamic connection state, and the states are
 * reused between roles.
 *
 * None of the internal role or state representations are made available outside
 * of lws internals.  Even for lws internals, if you add stuff here, please keep
 * the constants inside this header only by adding necessary helpers here and
 * use the helpers in the actual code.  This is to ease any future refactors.
 *
 * Notice LWSIFR_ENCAP means we have a parent wsi that actually carries our
 * data as a stream inside a different protocol.
 */

#define _RS 16

#define LWSIFR_CLIENT		(0x1000 << _RS) /* client side */
#define LWSIFR_SERVER		(0x2000 << _RS) /* server side */

#define LWSIFR_P_ENCAP_H2	(0x0100 << _RS) /* we are encapsulated by h2 */

enum lwsi_role {
	LWSI_ROLE_MASK		=			     (0xffff << _RS),
	LWSI_ROLE_ENCAP_MASK	=			     (0x0f00 << _RS),
};

#define lwsi_role(wsi) (wsi->wsistate & (unsigned int)LWSI_ROLE_MASK)
#if !defined (_DEBUG)
#define lwsi_set_role(wsi, role) wsi->wsistate = \
				(wsi->wsistate & (~LWSI_ROLE_MASK)) | role
#else
void lwsi_set_role(struct lws *wsi, lws_wsi_state_t role);
#endif

#define lwsi_role_client(wsi) (!!(wsi->wsistate & LWSIFR_CLIENT))
#define lwsi_role_server(wsi) (!!(wsi->wsistate & LWSIFR_SERVER))
#define lwsi_role_h2_ENCAPSULATION(wsi) \
		((wsi->wsistate & LWSI_ROLE_ENCAP_MASK) == LWSIFR_P_ENCAP_H2)

/* Pollout wants a callback in this state */
#define LWSIFS_POCB		(0x100)
/* Before any protocol connection was established */
#define LWSIFS_NOT_EST		(0x200)

enum lwsi_state {

	/* Phase 1: pre-transport */

	LRS_UNCONNECTED				= LWSIFS_NOT_EST | 0,
	LRS_WAITING_DNS				= LWSIFS_NOT_EST | 1,
	LRS_WAITING_CONNECT			= LWSIFS_NOT_EST | 2,

	/* Phase 2: establishing intermediaries on top of transport */

	LRS_WAITING_PROXY_REPLY			= LWSIFS_NOT_EST | 3,
	LRS_WAITING_SSL				= LWSIFS_NOT_EST | 4,
	LRS_WAITING_SOCKS_GREETING_REPLY	= LWSIFS_NOT_EST | 5,
	LRS_WAITING_SOCKS_CONNECT_REPLY		= LWSIFS_NOT_EST | 6,
	LRS_WAITING_SOCKS_AUTH_REPLY		= LWSIFS_NOT_EST | 7,

	/* Phase 3: establishing tls tunnel */

	LRS_SSL_INIT				= LWSIFS_NOT_EST | 8,
	LRS_SSL_ACK_PENDING			= LWSIFS_NOT_EST | 9,
	LRS_PRE_WS_SERVING_ACCEPT		= LWSIFS_NOT_EST | 10,

	/* Phase 4: connected */

	LRS_WAITING_SERVER_REPLY		= LWSIFS_NOT_EST | 11,
	LRS_H2_AWAIT_PREFACE			= LWSIFS_NOT_EST | 12,
	LRS_H2_AWAIT_SETTINGS			= LWSIFS_NOT_EST |
						  LWSIFS_POCB | 13,
	LRS_MQTTC_IDLE				= LWSIFS_POCB | 33,
	LRS_MQTTC_AWAIT_CONNACK			= 34,

	/* Phase 5: protocol logically established */

	LRS_H2_CLIENT_SEND_SETTINGS		= LWSIFS_POCB | 14,
	LRS_H2_WAITING_TO_SEND_HEADERS		= LWSIFS_POCB | 15,
	LRS_DEFERRING_ACTION			= LWSIFS_POCB | 16,
	LRS_IDLING				= 17,
	LRS_H1C_ISSUE_HANDSHAKE			= 18,
	LRS_H1C_ISSUE_HANDSHAKE2		= 19,
	LRS_ISSUE_HTTP_BODY			= 20,
	LRS_ISSUING_FILE			= 21,
	LRS_HEADERS				= 22,
	LRS_BODY				= 23,
	LRS_DISCARD_BODY			= 24,
	LRS_ESTABLISHED				= LWSIFS_POCB | 25,

	/* we are established, but we have embarked on serving a single
	 * transaction.  Other transaction input may be pending, but we will
	 * not service it while we are busy dealing with the current
	 * transaction.
	 *
	 * When we complete the current transaction, we would reset our state
	 * back to ESTABLISHED and start to process the next transaction.
	 */
	LRS_DOING_TRANSACTION			= LWSIFS_POCB | 26,

	/* Phase 6: finishing */

	LRS_WAITING_TO_SEND_CLOSE		= LWSIFS_POCB | 27,
	LRS_RETURNED_CLOSE			= LWSIFS_POCB | 28,
	LRS_AWAITING_CLOSE_ACK			= LWSIFS_POCB | 29,
	LRS_FLUSHING_BEFORE_CLOSE		= LWSIFS_POCB | 30,
	LRS_SHUTDOWN				= 31,

	/* Phase 7: dead */

	LRS_DEAD_SOCKET				= 32,

	LRS_MASK				= 0xffff
};

#define lwsi_state(wsi) ((enum lwsi_state)(wsi->wsistate & LRS_MASK))
#define lwsi_state_PRE_CLOSE(wsi) \
		((enum lwsi_state)(wsi->wsistate_pre_close & LRS_MASK))
#define lwsi_state_est(wsi) (!(wsi->wsistate & LWSIFS_NOT_EST))
#define lwsi_state_est_PRE_CLOSE(wsi) \
		(!(wsi->wsistate_pre_close & LWSIFS_NOT_EST))
#define lwsi_state_can_handle_POLLOUT(wsi) (wsi->wsistate & LWSIFS_POCB)
#if !defined (_DEBUG)
#define lwsi_set_state(wsi, lrs) wsi->wsistate = \
			  (wsi->wsistate & (lws_wsi_state_t)(~LRS_MASK)) | lrs
#else
void lwsi_set_state(struct lws *wsi, lws_wsi_state_t lrs);
#endif

#define _LWS_ADOPT_FINISH (1 << 24)

/*
 * Internal role-specific ops
 *
 * Many roles are sparsely filled with callbacks, rather than has 20 x
 * function pointers in the ops struct, let's have a 20 nybble array telling us
 * if the pointer doesn't exist, or its offset in a smaller "just pointers that
 * exist" array.
 *
 * We can support up to 15 valid pointers in the role that way and only have to
 * provide pointers that exist for that role, at the cost of a 10-byte nybble
 * table.
 *
 * For x86_64, a set 196 byte allocation becomes 60 + 8 bytes per defined ptr,
 * where the ops table is sparse this is a considable .rodata saving, for 32-bit
 * 52 + 4 bytes per defined ptr accounting for padding.
 */

/*
 * After http headers have parsed, this is the last chance for a role
 * to upgrade the connection to something else using the headers.
 * ws-over-h2 is upgraded from h2 like this.
 */
typedef int (*lws_rops_check_upgrades_t)(struct lws *wsi);
/* role-specific context init during context creation */
typedef int (*lws_rops_pt_init_destroy_t)(struct lws_context *context,
				const struct lws_context_creation_info *info,
				struct lws_context_per_thread *pt, int destroy);
/* role-specific per-vhost init during vhost creation */
typedef int (*lws_rops_init_vhost_t)(struct lws_vhost *vh,
				  const struct lws_context_creation_info *info);
/* role-specific per-vhost destructor during vhost destroy */
typedef int (*lws_rops_destroy_vhost_t)(struct lws_vhost *vh);
/* chance for the role to force POLLIN without network activity */
typedef int (*lws_rops_service_flag_pending_t)(struct lws_context *context,
					       int tsi);
/* an fd using this role has POLLIN signalled */
typedef int (*lws_rops_handle_POLLIN_t)(struct lws_context_per_thread *pt,
					struct lws *wsi,
					struct lws_pollfd *pollfd);
/* an fd using the role wanted a POLLOUT callback and now has it */
typedef int (*lws_rops_handle_POLLOUT_t)(struct lws *wsi);
/* perform user pollout */
typedef int (*lws_rops_perform_user_POLLOUT_t)(struct lws *wsi);
/* do effective callback on writeable */
typedef int (*lws_rops_callback_on_writable_t)(struct lws *wsi);
/* connection-specific tx credit in bytes */
typedef int (*lws_rops_tx_credit_t)(struct lws *wsi, char peer_to_us, int add);
/* role-specific write formatting */
typedef int (*lws_rops_write_role_protocol_t)(struct lws *wsi,
					      unsigned char *buf, size_t len,
					      enum lws_write_protocol *wp);

/* get encapsulation parent */
typedef struct lws * (*lws_rops_encapsulation_parent_t)(struct lws *wsi);

/* role-specific destructor */
typedef int (*lws_rops_alpn_negotiated_t)(struct lws *wsi, const char *alpn);

/* chance for the role to handle close in the protocol */
typedef int (*lws_rops_close_via_role_protocol_t)(struct lws *wsi,
						  enum lws_close_status reason);
/* role-specific close processing */
typedef int (*lws_rops_close_role_t)(struct lws_context_per_thread *pt,
				     struct lws *wsi);
/* role-specific connection close processing */
typedef int (*lws_rops_close_kill_connection_t)(struct lws *wsi,
						enum lws_close_status reason);
/* role-specific destructor */
typedef int (*lws_rops_destroy_role_t)(struct lws *wsi);

/* role-specific socket-adopt */
typedef int (*lws_rops_adoption_bind_t)(struct lws *wsi, int type,
					const char *prot);
/* role-specific client-bind:
 * ret 1 = bound, 0 = not bound, -1 = fail out
 * i may be NULL, indicating client_bind is being called after
 * a successful bind earlier, to finalize the binding.  In that
 * case ret 0 = OK, 1 = fail, wsi needs freeing, -1 = fail, wsi freed */
typedef int (*lws_rops_client_bind_t)(struct lws *wsi,
				      const struct lws_client_connect_info *i);
/* isvalid = 0: request a role-specific keepalive (PING etc)
 *         = 1: reset any related validity timer */
typedef int (*lws_rops_issue_keepalive_t)(struct lws *wsi, int isvalid);

#define LWS_COUNT_ROLE_OPS			20

typedef union lws_rops {
	lws_rops_check_upgrades_t		check_upgrades;
	lws_rops_pt_init_destroy_t		pt_init_destroy;
	lws_rops_init_vhost_t			init_vhost;
	lws_rops_destroy_vhost_t		destroy_vhost;
	lws_rops_service_flag_pending_t		service_flag_pending;
	lws_rops_handle_POLLIN_t		handle_POLLIN;
	lws_rops_handle_POLLOUT_t		handle_POLLOUT;
	lws_rops_perform_user_POLLOUT_t		perform_user_POLLOUT;
	lws_rops_callback_on_writable_t		callback_on_writable;
	lws_rops_tx_credit_t			tx_credit;
	lws_rops_write_role_protocol_t		write_role_protocol;
	lws_rops_encapsulation_parent_t		encapsulation_parent;
	lws_rops_alpn_negotiated_t		alpn_negotiated;
	lws_rops_close_via_role_protocol_t	close_via_role_protocol;
	lws_rops_close_role_t			close_role;
	lws_rops_close_kill_connection_t	close_kill_connection;
	lws_rops_destroy_role_t			destroy_role;
	lws_rops_adoption_bind_t		adoption_bind;
	lws_rops_client_bind_t			client_bind;
	lws_rops_issue_keepalive_t		issue_keepalive;
} lws_rops_t;

typedef enum {
	LWS_ROPS_check_upgrades,
	LWS_ROPS_pt_init_destroy,
	LWS_ROPS_init_vhost,
	LWS_ROPS_destroy_vhost,
	LWS_ROPS_service_flag_pending,
	LWS_ROPS_handle_POLLIN,
	LWS_ROPS_handle_POLLOUT,
	LWS_ROPS_perform_user_POLLOUT,
	LWS_ROPS_callback_on_writable,
	LWS_ROPS_tx_credit,
	LWS_ROPS_write_role_protocol,
	LWS_ROPS_encapsulation_parent,
	LWS_ROPS_alpn_negotiated,
	LWS_ROPS_close_via_role_protocol,
	LWS_ROPS_close_role,
	LWS_ROPS_close_kill_connection,
	LWS_ROPS_destroy_role,
	LWS_ROPS_adoption_bind,
	LWS_ROPS_client_bind,
	LWS_ROPS_issue_keepalive,
} lws_rops_func_idx_t;

struct lws_context_per_thread;

struct lws_role_ops {
	const char		*name;
	const char		*alpn;

	const lws_rops_t	*rops_table;
	/**< the occupied role ops func ptrs */
	uint8_t			rops_idx[(LWS_COUNT_ROLE_OPS + 1) / 2];
	/**< translates role index into .rops[] offset */

	/*
	 * the callback reasons for adoption for client, server
	 * (just client applies if no concept of client or server)
	 */
	uint8_t			adoption_cb[2];
	/*
	 * the callback reasons for adoption for client, server
	 * (just client applies if no concept of client or server)
	 */
	uint8_t			rx_cb[2];
	/*
	 * the callback reasons for WRITEABLE for client, server
	 * (just client applies if no concept of client or server)
	 */
	uint8_t			writeable_cb[2];
	/*
	 * the callback reasons for CLOSE for client, server
	 * (just client applies if no concept of client or server)
	 */
	uint8_t			close_cb[2];
	/*
	 * the callback reasons for protocol bind for client, server
	 * (just client applies if no concept of client or server)
	 */
	uint8_t			protocol_bind_cb[2];
	/*
	 * the callback reasons for protocol unbind for client, server
	 * (just client applies if no concept of client or server)
	 */
	uint8_t			protocol_unbind_cb[2];

	uint8_t			file_handle:1;
	/* role operates on files not sockets */
};

#define lws_rops_fidx(_rops, fidx) \
		((fidx & 1) ? (_rops)->rops_idx[fidx / 2] & 0xf : \
			      (_rops)->rops_idx[fidx / 2] >> 4)

#define lws_rops_func_fidx(_rops, fidx) \
		((_rops)->rops_table[lws_rops_fidx(_rops, fidx) - 1])

/* core roles */
extern const struct lws_role_ops role_ops_raw_skt, role_ops_raw_file,
				 role_ops_listen, role_ops_pipe,
				 role_ops_netlink;

/* bring in role private declarations */

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
 #include "private-lib-roles-http.h"
#else
 #define lwsi_role_http(wsi) (0)
#endif

#if defined(LWS_ROLE_H1)
 #include "private-lib-roles-h1.h"
#else
 #define lwsi_role_h1(wsi) (0)
#endif

#if defined(LWS_ROLE_H2)
 #include "private-lib-roles-h2.h"
#else
 #define lwsi_role_h2(wsi) (0)
#endif

#if defined(LWS_ROLE_WS)
 #include "private-lib-roles-ws.h"
#else
 #define lwsi_role_ws(wsi) (0)
#endif

#if defined(LWS_ROLE_CGI)
 #include "private-lib-roles-cgi.h"
#else
 #define lwsi_role_cgi(wsi) (0)
#endif

#if defined(LWS_ROLE_DBUS)
 #include "private-lib-roles-dbus.h"
#else
 #define lwsi_role_dbus(wsi) (0)
#endif

#if defined(LWS_ROLE_RAW_PROXY)
 #include "private-lib-roles-raw-proxy.h"
#else
 #define lwsi_role_raw_proxy(wsi) (0)
#endif

#if defined(LWS_ROLE_MQTT)
 #include "mqtt/private-lib-roles-mqtt.h"
#else
 #define lwsi_role_mqtt(wsi) (0)
#endif

enum {
	LWS_HP_RET_BAIL_OK,
	LWS_HP_RET_BAIL_DIE,
	LWS_HP_RET_USER_SERVICE,
	LWS_HP_RET_DROP_POLLOUT,

	LWS_HPI_RET_WSI_ALREADY_DIED,	/* we closed it */
	LWS_HPI_RET_HANDLED,		/* no probs */
	LWS_HPI_RET_PLEASE_CLOSE_ME,	/* close it for us */

	LWS_UPG_RET_DONE,
	LWS_UPG_RET_CONTINUE,
	LWS_UPG_RET_BAIL
};

#define LWS_CONNECT_COMPLETION_GOOD (-99)

int
lws_role_call_adoption_bind(struct lws *wsi, int type, const char *prot);

struct lws *
lws_client_connect_4_established(struct lws *wsi, struct lws *wsi_piggyback,
				 ssize_t plen);

struct lws *
lws_client_connect_3_connect(struct lws *wsi, const char *ads,
			     const struct addrinfo *result, int n, void *opaque);
