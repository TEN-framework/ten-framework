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

#if !defined(__LWS_CORE_NET_PRIVATE_H__)
#define __LWS_CORE_NET_PRIVATE_H__

#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L
#endif

/*
 * Generic pieces needed to manage muxable stream protocols like h2
 */

struct lws_muxable {
	struct lws	*parent_wsi;
	struct lws	*child_list;
	struct lws	*sibling_list;

	unsigned int	my_sid;
	unsigned int	child_count;

	uint32_t	highest_sid;

	uint8_t		requested_POLLOUT;
};

#include "private-lib-roles.h"

#ifdef __cplusplus
extern "C" {
#endif

#define __lws_sul_insert_us(owner, sul, _us) \
		(sul)->us = lws_now_usecs() + (lws_usec_t)(_us); \
		__lws_sul_insert(owner, sul)


/*
 *
 *  ------ roles ------
 *
 */

/* null-terminated array of pointers to roles lws built with */
extern const struct lws_role_ops *available_roles[];

#define LWS_FOR_EVERY_AVAILABLE_ROLE_START(xx) { \
		const struct lws_role_ops **ppxx = available_roles; \
		while (*ppxx) { \
			const struct lws_role_ops *xx = *ppxx++;

#define LWS_FOR_EVERY_AVAILABLE_ROLE_END }}

/*
 *
 *  ------ event_loop ops ------
 *
 */

/* enums of socks version */
enum socks_version {
	SOCKS_VERSION_4 = 4,
	SOCKS_VERSION_5 = 5
};

/* enums of subnegotiation version */
enum socks_subnegotiation_version {
	SOCKS_SUBNEGOTIATION_VERSION_1 = 1,
};

/* enums of socks commands */
enum socks_command {
	SOCKS_COMMAND_CONNECT = 1,
	SOCKS_COMMAND_BIND = 2,
	SOCKS_COMMAND_UDP_ASSOCIATE = 3
};

/* enums of socks address type */
enum socks_atyp {
	SOCKS_ATYP_IPV4 = 1,
	SOCKS_ATYP_DOMAINNAME = 3,
	SOCKS_ATYP_IPV6 = 4
};

/* enums of socks authentication methods */
enum socks_auth_method {
	SOCKS_AUTH_NO_AUTH = 0,
	SOCKS_AUTH_GSSAPI = 1,
	SOCKS_AUTH_USERNAME_PASSWORD = 2
};

/* enums of subnegotiation status */
enum socks_subnegotiation_status {
	SOCKS_SUBNEGOTIATION_STATUS_SUCCESS = 0,
};

/* enums of socks request reply */
enum socks_request_reply {
	SOCKS_REQUEST_REPLY_SUCCESS = 0,
	SOCKS_REQUEST_REPLY_FAILURE_GENERAL = 1,
	SOCKS_REQUEST_REPLY_CONNECTION_NOT_ALLOWED = 2,
	SOCKS_REQUEST_REPLY_NETWORK_UNREACHABLE = 3,
	SOCKS_REQUEST_REPLY_HOST_UNREACHABLE = 4,
	SOCKS_REQUEST_REPLY_CONNECTION_REFUSED = 5,
	SOCKS_REQUEST_REPLY_TTL_EXPIRED = 6,
	SOCKS_REQUEST_REPLY_COMMAND_NOT_SUPPORTED = 7,
	SOCKS_REQUEST_REPLY_ATYP_NOT_SUPPORTED = 8
};

/* enums used to generate socks messages */
enum socks_msg_type {
	/* greeting */
	SOCKS_MSG_GREETING,
	/* credential, user name and password */
	SOCKS_MSG_USERNAME_PASSWORD,
	/* connect command */
	SOCKS_MSG_CONNECT
};

enum {
	LWS_RXFLOW_ALLOW = (1 << 0),
	LWS_RXFLOW_PENDING_CHANGE = (1 << 1),
};

typedef enum lws_parser_return {
	LPR_FORBIDDEN	= -2,
	LPR_FAIL	= -1,
	LPR_OK		= 0,
	LPR_DO_FALLBACK = 2,
} lws_parser_return_t;

enum pmd_return {
	PMDR_UNKNOWN,
	PMDR_DID_NOTHING,
	PMDR_HAS_PENDING,
	PMDR_EMPTY_NONFINAL,
	PMDR_EMPTY_FINAL,
	PMDR_NOTHING_WE_SHOULD_DO,

	PMDR_FAILED = -1
};

#if defined(LWS_WITH_PEER_LIMITS)
struct lws_peer {
	struct lws_peer *next;
	struct lws_peer *peer_wait_list;

	lws_sockaddr46	sa46;

	time_t time_created;
	time_t time_closed_all;

	uint32_t hash;
	uint32_t count_wsi;
	uint32_t total_wsi;

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	struct lws_peer_role_http http;
#endif
};
#endif

#ifdef LWS_WITH_IPV6
#define LWS_IPV6_ENABLED(vh) \
	(!lws_check_opt(vh->context->options, LWS_SERVER_OPTION_DISABLE_IPV6) && \
	 !lws_check_opt(vh->options, LWS_SERVER_OPTION_DISABLE_IPV6))
#else
#define LWS_IPV6_ENABLED(context) (0)
#endif

#ifdef LWS_WITH_UNIX_SOCK
#define LWS_UNIX_SOCK_ENABLED(vhost) \
	(vhost->options & LWS_SERVER_OPTION_UNIX_SOCK)
#else
#define LWS_UNIX_SOCK_ENABLED(vhost) (0)
#endif

enum uri_path_states {
	URIPS_IDLE,
	URIPS_SEEN_SLASH,
	URIPS_SEEN_SLASH_DOT,
	URIPS_SEEN_SLASH_DOT_DOT,
};

enum uri_esc_states {
	URIES_IDLE,
	URIES_SEEN_PERCENT,
	URIES_SEEN_PERCENT_H1,
};

#if defined(LWS_WITH_CLIENT)

enum {
	CIS_ADDRESS,
	CIS_PATH,
	CIS_HOST,
	CIS_ORIGIN,
	CIS_PROTOCOL,
	CIS_METHOD,
	CIS_IFACE,
	CIS_ALPN,


	CIS_COUNT
};

struct client_info_stash {
	char *cis[CIS_COUNT];
	void *opaque_user_data; /* not allocated or freed by lws */
};
#endif

#if defined(LWS_WITH_UDP)
#define lws_wsi_is_udp(___wsi) (!!___wsi->udp)
#endif

#define LWS_H2_FRAME_HEADER_LENGTH 9

lws_usec_t
__lws_sul_service_ripe(lws_dll2_owner_t *own, int num_own, lws_usec_t usnow);

/*
 * lws_async_dns
 */

typedef struct lws_async_dns {
	lws_sockaddr46 		sa46; /* nameserver */
	lws_dll2_owner_t	waiting;
	lws_dll2_owner_t	cached;
	struct lws		*wsi;
	time_t			time_set_server;
	uint8_t			dns_server_set:1;
	uint8_t			dns_server_connected:1;
} lws_async_dns_t;

typedef enum {
	LADNS_CONF_SERVER_UNKNOWN				= -1,
	LADNS_CONF_SERVER_SAME,
	LADNS_CONF_SERVER_CHANGED
} lws_async_dns_server_check_t;

#if defined(LWS_WITH_SYS_ASYNC_DNS)
void
lws_aysnc_dns_completed(struct lws *wsi, void *sa, size_t salen,
			lws_async_dns_retcode_t ret);
#endif
void
lws_async_dns_cancel(struct lws *wsi);

void
lws_async_dns_drop_server(struct lws_context *context);

/*
 * so we can have n connections being serviced simultaneously,
 * these things need to be isolated per-thread.
 */

struct lws_context_per_thread {
#if LWS_MAX_SMP > 1
	pthread_mutex_t lock_stats;
	struct lws_mutex_refcount mr;
	pthread_t self;
#endif
	struct lws_dll2_owner dll_buflist_owner;  /* guys with pending rxflow */
	struct lws_dll2_owner seq_owner;	   /* list of lws_sequencer-s */
	lws_dll2_owner_t      attach_owner;	/* pending lws_attach */

#if defined(LWS_WITH_SECURE_STREAMS)
	lws_dll2_owner_t ss_owner;
#endif
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API) || \
    defined(LWS_WITH_SECURE_STREAMS_THREAD_API)
	lws_dll2_owner_t ss_dsh_owner;
	lws_dll2_owner_t ss_client_owner;
#endif

	struct lws_dll2_owner pt_sul_owner[LWS_COUNT_PT_SUL_OWNERS];

#if defined (LWS_WITH_SEQUENCER)
	lws_sorted_usec_list_t sul_seq_heartbeat;
#endif
#if (defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)) && defined(LWS_WITH_SERVER)
	lws_sorted_usec_list_t sul_ah_lifecheck;
#endif
#if defined(LWS_WITH_TLS) && defined(LWS_WITH_SERVER)
	lws_sorted_usec_list_t sul_tls;
#endif
#if defined(LWS_PLAT_UNIX)
	lws_sorted_usec_list_t sul_plat;
#endif
#if defined(LWS_ROLE_CGI)
	lws_sorted_usec_list_t sul_cgi;
#endif
#if defined(LWS_WITH_PEER_LIMITS)
	lws_sorted_usec_list_t sul_peer_limits;
#endif

#if !defined(LWS_PLAT_FREERTOS)
	struct lws *fake_wsi;   /* used for callbacks where there's no wsi */
#endif

#if defined(WIN32)
	struct sockaddr_in frt_pipe_si;
#endif

#if defined(LWS_WITH_TLS)
	struct lws_pt_tls tls;
#endif
	struct lws_context *context;

	/*
	 * usable by anything in the service code, but only if the scope
	 * does not last longer than the service action (since next service
	 * of any socket can likewise use it and overwrite)
	 */
	unsigned char *serv_buf;

	struct lws_pollfd *fds;
	volatile struct lws_foreign_thread_pollfd * volatile foreign_pfd_list;

	lws_sockfd_type dummy_pipe_fds[2];
	struct lws *pipe_wsi;

	/* --- role based members --- */

#if defined(LWS_ROLE_WS) && !defined(LWS_WITHOUT_EXTENSIONS)
	struct lws_pt_role_ws ws;
#endif
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	struct lws_pt_role_http http;
#endif
#if defined(LWS_ROLE_DBUS)
	struct lws_pt_role_dbus dbus;
#endif
	/* --- event library based members --- */

	void		*evlib_pt; /* overallocated */

	/* --- */

	unsigned long count_conns;
	unsigned int fds_count;

	/*
	 * set to the Thread ID that's doing the service loop just before entry
	 * to poll indicates service thread likely idling in poll()
	 * volatile because other threads may check it as part of processing
	 * for pollfd event change.
	 */
	volatile int service_tid;
	int service_tid_detected;
#if !defined(LWS_PLAT_FREERTOS)
	int count_event_loop_static_asset_handles;
#endif

	volatile unsigned char inside_poll;
	volatile unsigned char foreign_spinlock;

	unsigned char tid;

	unsigned char inside_service:1;
	unsigned char inside_lws_service:1;
	unsigned char event_loop_foreign:1;
	unsigned char event_loop_destroy_processing_done:1;
	unsigned char event_loop_pt_unused:1;
	unsigned char destroy_self:1;
	unsigned char is_destroyed:1;
};

/*
 * virtual host -related context information
 *   vhostwide SSL context
 *   vhostwide proxy
 *
 * hierarchy:
 *
 * context -> vhost -> wsi
 *
 * incoming connection non-SSL vhost binding:
 *
 *    listen socket -> wsi -> select vhost after first headers
 *
 * incoming connection SSL vhost binding:
 *
 *    SSL SNI -> wsi -> bind after SSL negotiation
 */

struct lws_vhost {
#if defined(LWS_WITH_CLIENT) && defined(LWS_CLIENT_HTTP_PROXYING)
	char proxy_basic_auth_token[128];
#endif
#if LWS_MAX_SMP > 1
	struct lws_mutex_refcount		mr;
	char					close_flow_vs_tsi[LWS_MAX_SMP];
#endif

#if defined(LWS_ROLE_H2)
	struct lws_vhost_role_h2 h2;
#endif
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	struct lws_vhost_role_http http;
#endif
#if defined(LWS_ROLE_WS) && !defined(LWS_WITHOUT_EXTENSIONS)
	struct lws_vhost_role_ws ws;
#endif

	lws_lifecycle_t		lc;
	lws_dll2_t		vh_being_destroyed_list;

#if defined(LWS_WITH_SOCKS5)
	char socks_proxy_address[128];
	char socks_user[96];
	char socks_password[96];
#endif

#if defined(LWS_WITH_TLS_SESSIONS)
	lws_dll2_owner_t	tls_sessions; /* vh lock */
#endif

#if defined(LWS_WITH_EVENT_LIBS)
	void		*evlib_vh; /* overallocated */
#endif
#if defined(LWS_WITH_SYS_METRICS)
	lws_metric_t	*mt_traffic_rx;
	lws_metric_t	*mt_traffic_tx;
#endif

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	lws_fi_ctx_t				fic;
	/**< Fault Injection ctx for the vhost, hierarchy vhost->context */
#endif

	uint64_t options;

	struct lws_context *context;
	struct lws_vhost *vhost_next;

	const lws_retry_bo_t *retry_policy;

#if defined(LWS_WITH_TLS_JIT_TRUST)
	lws_sorted_usec_list_t		sul_unref; /* grace period after idle */
#endif

#if defined(LWS_WITH_SERVER) && defined(LWS_WITH_SECURE_STREAMS)
	lws_ss_handle_t		*ss_handle; /* ss handle for the server obj */
#endif

	lws_dll2_owner_t	listen_wsi;

	const char *name;
	const char *iface;
	const char *listen_accept_role;
	const char *listen_accept_protocol;
	const char *unix_socket_perms;

	void (*finalize)(struct lws_vhost *vh, void *arg);
	void *finalize_arg;

	const struct lws_protocols *protocols;
	void **protocol_vh_privs;
	const struct lws_protocol_vhost_options *pvo;
	const struct lws_protocol_vhost_options *headers;
	struct lws_dll2_owner *same_vh_protocol_owner;
	struct lws_vhost *no_listener_vhost_list;
	struct lws_dll2_owner abstract_instances_owner;		/* vh lock */

#if defined(LWS_WITH_CLIENT)
	struct lws_dll2_owner dll_cli_active_conns_owner;
#endif
	struct lws_dll2_owner vh_awaiting_socket_owner;

#if defined(LWS_WITH_TLS)
	struct lws_vhost_tls tls;
#endif

	void *user;

	int listen_port;
#if !defined(LWS_PLAT_FREERTOS) && !defined(OPTEE_TA) && !defined(WIN32)
	int bind_iface;
#endif

#if defined(LWS_WITH_SOCKS5)
	unsigned int socks_proxy_port;
#endif
	int count_protocols;
	int ka_time;
	int ka_probes;
	int ka_interval;
	int keepalive_timeout;
	int timeout_secs_ah_idle;
	int connect_timeout_secs;
	int fo_listen_queue;

	int count_bound_wsi;

#ifdef LWS_WITH_ACCESS_LOG
	int log_fd;
#endif

#if defined(LWS_WITH_TLS_SESSIONS)
	uint32_t		tls_session_cache_max;
#endif

#if defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY) || defined(LWS_WITH_SECURE_STREAMS_CPP)
	int8_t			ss_refcount;
	/**< refcount of number of ss connections with streamtypes using this
	 * trust store */
#endif

	uint8_t allocated_vhost_protocols:1;
	uint8_t created_vhost_protocols:1;
	uint8_t being_destroyed:1;
	uint8_t from_ss_policy:1;
#if defined(LWS_WITH_TLS_JIT_TRUST)
	uint8_t 		grace_after_unref:1;
	/* grace time / autodelete aoplies to us */
#endif

	unsigned char default_protocol_index;
	unsigned char raw_protocol_index;
};

void
__lws_vhost_destroy2(struct lws_vhost *vh);

#define mux_to_wsi(_m) lws_container_of(_m, struct lws, mux)

void
lws_wsi_mux_insert(struct lws *wsi, struct lws *parent_wsi, unsigned int sid);
int
lws_wsi_mux_mark_parents_needing_writeable(struct lws *wsi);
struct lws *
lws_wsi_mux_move_child_to_tail(struct lws **wsi2);
int
lws_wsi_mux_action_pending_writeable_reqs(struct lws *wsi);

void
lws_wsi_mux_dump_children(struct lws *wsi);

void
lws_wsi_mux_close_children(struct lws *wsi, int reason);

void
lws_wsi_mux_sibling_disconnect(struct lws *wsi);

void
lws_wsi_mux_dump_waiting_children(struct lws *wsi);

int
lws_wsi_mux_apply_queue(struct lws *wsi);

/*
 * struct lws
 */

/*
 * These pieces are very commonly used (via accessors) in user protocol handlers
 * and have to be valid, even in the case no real wsi is available for the cb.
 *
 * We put all this category of pointers in there and compose it at the top of
 * struct lws, so a dummy wsi providing these only needs to be this big, while
 * still being castable for being a struct wsi *
 */

struct lws_a {
	struct lws_context		*context;
	struct lws_vhost		*vhost;
	const struct lws_protocols	*protocol;
	void				*opaque_user_data;
};

/*
 * For RTOS-class platforms, their code is relatively new, post-minimal examples
 * and tend to not have legacy user protocol handler baggage touching unexpected
 * things in fakewsi unconditionally... we can use an lws_a on the stack and
 * don't need to define the rest of the wsi content, just cast it, this saves
 * a wsi footprint in heap (typ 800 bytes nowadays even on RTOS).
 *
 * For other platforms that have been around for years and have thousands of
 * different user protocol handler implementations, it's likely some of them
 * will be touching the struct lws content unconditionally in the handler even
 * when we are calling back with a non wsi-specific reason, and may react badly
 * to it being garbage.  So continue to implement those as a full, zero-ed down
 * prepared fakewsi on heap at context creation time.
 */

#if defined(LWS_PLAT_FREERTOS)
#define lws_fakewsi_def_plwsa(pt) struct lws_a lwsa, *plwsa = &lwsa
#else
#define lws_fakewsi_def_plwsa(pt) struct lws_a *plwsa = &(pt)->fake_wsi->a
#endif
/* since we reuse the pt version, also correct to zero down the lws_a part */
#define lws_fakewsi_prep_plwsa_ctx(_c) \
		memset(plwsa, 0, sizeof(*plwsa)); plwsa->context = _c

struct lws {

	struct lws_a			a;

	/* structs */

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	struct _lws_http_mode_related	http;
#endif
#if defined(LWS_ROLE_H2)
	struct _lws_h2_related		h2;
#endif
#if defined(LWS_ROLE_WS)
	struct _lws_websocket_related	*ws; /* allocated if we upgrade to ws */
#endif
#if defined(LWS_ROLE_DBUS)
	struct _lws_dbus_mode_related	dbus;
#endif
#if defined(LWS_ROLE_MQTT)
	struct _lws_mqtt_related	*mqtt;
#endif

#if defined(LWS_ROLE_H2) || defined(LWS_ROLE_MQTT)
	struct lws_muxable		mux;
	struct lws_tx_credit		txc;
#endif

	lws_lifecycle_t			lc;

	/* lifetime members */

#if defined(LWS_WITH_EVENT_LIBS)
	void				*evlib_wsi; /* overallocated */
#endif

	lws_sorted_usec_list_t		sul_timeout;
	lws_sorted_usec_list_t		sul_hrtimer;
	lws_sorted_usec_list_t		sul_validity;
	lws_sorted_usec_list_t		sul_connect_timeout;

	struct lws_dll2			dll_buflist; /* guys with pending rxflow */
	struct lws_dll2			same_vh_protocol;
	struct lws_dll2			vh_awaiting_socket;
#if defined(LWS_WITH_SYS_ASYNC_DNS)
	struct lws_dll2			adns; /* on adns list of guys to tell result */
	lws_async_dns_cb_t		adns_cb; /* callback with result */
#endif
#if defined(LWS_WITH_SERVER)
	struct lws_dll2			listen_list;
#endif
#if defined(LWS_WITH_CLIENT)
	struct lws_dll2			dll_cli_active_conns;
	struct lws_dll2			dll2_cli_txn_queue;
	struct lws_dll2_owner		dll2_cli_txn_queue_owner;

	/**< caliper is reused for tcp, tls and txn conn phases */

	lws_dll2_t			speculative_list;
	lws_dll2_owner_t		speculative_connect_owner;
	/* wsis: additional connection candidates */
	lws_dll2_owner_t		dns_sorted_list;
	/* lws_dns_sort_t: dns results wrapped and sorted in a linked-list...
	 * deleted as they are tried, list empty == everything tried */
#endif

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	lws_fi_ctx_t			fic;
	/**< Fault Injection ctx for the wsi, hierarchy wsi->vhost->context */
	lws_sorted_usec_list_t		sul_fault_timedclose;
	/**< used to inject a fault that closes the wsi after a random time */
#endif

#if defined(LWS_WITH_SYS_METRICS)
	lws_metrics_caliper_compose(cal_conn)
#endif

	lws_sockaddr46			sa46_local;
	lws_sockaddr46			sa46_peer;

	/* pointers */

	struct lws			*parent; /* points to parent, if any */
	struct lws			*child_list; /* points to first child */
	struct lws			*sibling_list; /* subsequent children at same level */
	const struct lws_role_ops	*role_ops;
	struct lws_sequencer		*seq;	/* associated sequencer if any */
	const lws_retry_bo_t		*retry_policy;

	lws_log_cx_t			*log_cx;

#if defined(LWS_WITH_THREADPOOL) && defined(LWS_HAVE_PTHREAD_H)
	lws_dll2_owner_t		tp_task_owner; /* struct lws_threadpool_task */
#endif

#if defined(LWS_WITH_PEER_LIMITS)
	struct lws_peer			*peer;
#endif

#if defined(LWS_WITH_UDP)
	struct lws_udp			*udp;
#endif
#if defined(LWS_WITH_CLIENT)
	struct client_info_stash	*stash;
	char				*cli_hostname_copy;

#if defined(LWS_WITH_CONMON)
	struct lws_conmon		conmon;
	lws_usec_t			conmon_datum;
#endif
#endif /* WITH_CLIENT */
	void				*user_space;
	void				*opaque_parent_data;

	struct lws_buflist		*buflist; /* input-side buflist */
	struct lws_buflist		*buflist_out; /* output-side buflist */

#if defined(LWS_WITH_TLS)
	struct lws_lws_tls		tls;
	char				alpn[24];
#endif

	lws_sock_file_fd_type		desc; /* .filefd / .sockfd */

	lws_wsi_state_t			wsistate;
	lws_wsi_state_t			wsistate_pre_close;

	/* ints */
#define LWS_NO_FDS_POS (-1)
	int				position_in_fds_table;

#if defined(LWS_WITH_CLIENT)
	int				chunk_remaining;
	int				flags;
#endif
	unsigned int			cache_secs;

	short				bugcatcher;

	unsigned int			hdr_parsing_completed:1;
	unsigned int			mux_substream:1;
	unsigned int			upgraded_to_http2:1;
	unsigned int			mux_stream_immortal:1;
	unsigned int			h2_stream_carries_ws:1; /* immortal set as well */
	unsigned int			h2_stream_carries_sse:1; /* immortal set as well */
	unsigned int			h2_acked_settings:1;
	unsigned int			seen_nonpseudoheader:1;
	unsigned int			listener:1;
	unsigned int			pf_packet:1;
	unsigned int			do_broadcast:1;
	unsigned int			user_space_externally_allocated:1;
	unsigned int			socket_is_permanently_unusable:1;
	unsigned int			rxflow_change_to:2;
	unsigned int			conn_stat_done:1;
	unsigned int			cache_reuse:1;
	unsigned int			cache_revalidate:1;
	unsigned int			cache_intermediaries:1;
	unsigned int			favoured_pollin:1;
	unsigned int			sending_chunked:1;
	unsigned int			interpreting:1;
	unsigned int			already_did_cce:1;
	unsigned int			told_user_closed:1;
	unsigned int			told_event_loop_closed:1;
	unsigned int			waiting_to_send_close_frame:1;
	unsigned int			close_needs_ack:1;
	unsigned int			ipv6:1;
	unsigned int			parent_pending_cb_on_writable:1;
	unsigned int			cgi_stdout_zero_length:1;
	unsigned int			seen_zero_length_recv:1;
	unsigned int			rxflow_will_be_applied:1;
	unsigned int			event_pipe:1;
	unsigned int			handling_404:1;
	unsigned int			protocol_bind_balance:1;
	unsigned int			unix_skt:1;
	unsigned int			close_when_buffered_out_drained:1;
	unsigned int			h1_ws_proxied:1;
	unsigned int			proxied_ws_parent:1;
	unsigned int			do_bind:1;
	unsigned int			validity_hup:1;
	unsigned int			skip_fallback:1;
	unsigned int			file_desc:1;
	unsigned int			conn_validity_wakesuspend:1;
	unsigned int			dns_reachability:1;

	unsigned int			could_have_pending:1; /* detect back-to-back writes */
	unsigned int			outer_will_close:1;
	unsigned int			shadow:1; /* we do not control fd lifecycle at all */
#if defined(LWS_WITH_SECURE_STREAMS)
	unsigned int			for_ss:1;
	unsigned int			bound_ss_proxy_conn:1;
	unsigned int			client_bound_sspc:1;
	unsigned int			client_proxy_onward:1;
#endif
	unsigned int                    tls_borrowed:1;
	unsigned int                    tls_borrowed_hs:1;
	unsigned int                    tls_read_wanted_write:1;

#ifdef LWS_WITH_ACCESS_LOG
	unsigned int			access_log_pending:1;
#endif
#if defined(LWS_WITH_CLIENT)
	unsigned int			do_ws:1; /* whether we are doing http or ws flow */
	unsigned int			chunked:1; /* if the clientside connection is chunked */
	unsigned int			client_rx_avail:1;
	unsigned int			client_http_body_pending:1;
	unsigned int			transaction_from_pipeline_queue:1;
	unsigned int			keepalive_active:1;
	unsigned int			keepalive_rejected:1;
	unsigned int			redirected_to_get:1;
	unsigned int			client_pipeline:1;
	unsigned int			client_h2_alpn:1;
	unsigned int			client_mux_substream:1;
	unsigned int			client_mux_migrated:1;
	unsigned int			client_subsequent_mime_part:1;
	unsigned int                    client_no_follow_redirect:1;
	unsigned int                    client_suppress_CONNECTION_ERROR:1;
	/**< because the client connection creation api is still the parent of
	 * this activity, and will report the failure */
	unsigned int			tls_session_reused:1;
	unsigned int			perf_done:1;
	unsigned int			close_is_redirect:1;
	unsigned int			client_mux_substream_was:1;
#endif

#ifdef _WIN32
	unsigned int sock_send_blocking:1;
#endif

	uint16_t			ocport, c_port, conn_port;
	uint16_t			retry;
#if defined(LWS_WITH_CLIENT)
	uint16_t			keep_warm_secs;
#endif

	/* chars */

	char lws_rx_parse_state; /* enum lws_rx_parse_state */
	char rx_frame_type; /* enum lws_write_protocol */
	char pending_timeout; /* enum pending_timeout */
	char tsi; /* thread service index we belong to */
	char protocol_interpret_idx;
	char redirects;
	uint8_t rxflow_bitmap;
	uint8_t bound_vhost_index;
	uint8_t lsp_channel; /* which of stdin/out/err */
#ifdef LWS_WITH_CGI
	char hdr_state;
#endif
#if defined(LWS_WITH_CLIENT)
	char chunk_parser; /* enum lws_chunk_parser */
	uint8_t addrinfo_idx;
	uint8_t sys_tls_client_cert;
	uint8_t c_pri;
#endif
	uint8_t		af;
#if defined(LWS_WITH_CGI) || defined(LWS_WITH_CLIENT)
	char reason_bf; /* internal writeable callback reason bitfield */
#endif
#if defined(LWS_WITH_NETLINK)
	lws_route_uidx_t		peer_route_uidx;
	/**< unique index of the route the connection is estimated to take */
#endif
	uint8_t immortal_substream_count;
	/* volatile to make sure code is aware other thread can change */
	volatile char handling_pollout;
	volatile char leave_pollout_active;
#if LWS_MAX_SMP > 1
	volatile char undergoing_init_from_other_pt;
#endif

};

#define lws_is_flowcontrolled(w) (!!(wsi->rxflow_bitmap))

#if defined(LWS_WITH_SPAWN)

#if defined(WIN32) || defined(_WIN32)
#else
#include <sys/wait.h>
#include <sys/times.h>
#endif

struct lws_spawn_piped {

	struct lws_spawn_piped_info	info;

	struct lws_dll2			dll;
	lws_sorted_usec_list_t		sul;
	lws_sorted_usec_list_t		sul_reap;

	struct lws_context		*context;
	struct lws			*stdwsi[3];
	lws_filefd_type			pipe_fds[3][2];
	int				count_log_lines;

	lws_usec_t			created; /* set by lws_spawn_piped() */
	lws_usec_t			reaped;

	lws_usec_t			accounting[4];

#if defined(WIN32)
	HANDLE				child_pid;
	lws_sorted_usec_list_t		sul_poll;
#else
	pid_t				child_pid;

	siginfo_t			si;
#endif
	int				reap_retry_budget;

	uint8_t				pipes_alive:2;
	uint8_t				we_killed_him_timeout:1;
	uint8_t				we_killed_him_spew:1;
	uint8_t				ungraceful:1;
};

void
lws_spawn_piped_destroy(struct lws_spawn_piped **lsp);

int
lws_spawn_reap(struct lws_spawn_piped *lsp);

#endif

void
lws_service_do_ripe_rxflow(struct lws_context_per_thread *pt);

const struct lws_role_ops *
lws_role_by_name(const char *name);

int
lws_socket_bind(struct lws_vhost *vhost, struct lws *wsi,
		lws_sockfd_type sockfd, int port, const char *iface,
		int ipv6_allowed);

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
void
lws_wsi_fault_timedclose(struct lws *wsi);
#else
#define lws_wsi_fault_timedclose(_w)
#endif

#if defined(LWS_WITH_IPV6)
unsigned long
lws_get_addr_scope(struct lws *wsi, const char *ipaddr);
#endif

void
lws_close_free_wsi(struct lws *wsi, enum lws_close_status, const char *caller);
void
__lws_close_free_wsi(struct lws *wsi, enum lws_close_status, const char *caller);

void
__lws_free_wsi(struct lws *wsi);

void
lws_conmon_addrinfo_destroy(struct addrinfo *ai);

int
lws_conmon_append_copy_new_dns_results(struct lws *wsi,
				       const struct addrinfo *cai);

#if LWS_MAX_SMP > 1

static LWS_INLINE void
lws_pt_mutex_init(struct lws_context_per_thread *pt)
{
	lws_mutex_refcount_init(&pt->mr);
	pthread_mutex_init(&pt->lock_stats, NULL);
}

static LWS_INLINE void
lws_pt_mutex_destroy(struct lws_context_per_thread *pt)
{
	pthread_mutex_destroy(&pt->lock_stats);
	lws_mutex_refcount_destroy(&pt->mr);
}

#define lws_pt_lock(pt, reason) lws_mutex_refcount_lock(&pt->mr, reason)
#define lws_pt_unlock(pt) lws_mutex_refcount_unlock(&pt->mr)
#define lws_pt_assert_lock_held(pt) lws_mutex_refcount_assert_held(&pt->mr)

static LWS_INLINE void
lws_pt_stats_lock(struct lws_context_per_thread *pt)
{
	pthread_mutex_lock(&pt->lock_stats);
}

static LWS_INLINE void
lws_pt_stats_unlock(struct lws_context_per_thread *pt)
{
	pthread_mutex_unlock(&pt->lock_stats);
}
#endif

/*
 * EXTENSIONS
 */

#if defined(LWS_WITHOUT_EXTENSIONS)
#define lws_any_extension_handled(_a, _b, _c, _d) (0)
#define lws_ext_cb_active(_a, _b, _c, _d) (0)
#define lws_ext_cb_all_exts(_a, _b, _c, _d, _e) (0)
#define lws_issue_raw_ext_access lws_issue_raw
#define lws_context_init_extensions(_a, _b)
#endif

int LWS_WARN_UNUSED_RESULT
lws_client_interpret_server_handshake(struct lws *wsi);

int LWS_WARN_UNUSED_RESULT
lws_ws_rx_sm(struct lws *wsi, char already_processed, unsigned char c);

int LWS_WARN_UNUSED_RESULT
lws_issue_raw_ext_access(struct lws *wsi, unsigned char *buf, size_t len);

void
lws_role_transition(struct lws *wsi, enum lwsi_role role, enum lwsi_state state,
		    const struct lws_role_ops *ops);

int
lws_http_to_fallback(struct lws *wsi, unsigned char *buf, size_t len);

int LWS_WARN_UNUSED_RESULT
user_callback_handle_rxflow(lws_callback_function, struct lws *wsi,
			    enum lws_callback_reasons reason, void *user,
			    void *in, size_t len);

int
lws_plat_set_nonblocking(lws_sockfd_type fd);

int
lws_plat_set_socket_options(struct lws_vhost *vhost, lws_sockfd_type fd,
			    int unix_skt);

int
lws_plat_set_socket_options_ip(lws_sockfd_type fd, uint8_t pri, int lws_flags);

int
lws_plat_check_connection_error(struct lws *wsi);

int LWS_WARN_UNUSED_RESULT
lws_header_table_attach(struct lws *wsi, int autoservice);

int
lws_header_table_detach(struct lws *wsi, int autoservice);
int
__lws_header_table_detach(struct lws *wsi, int autoservice);

void
lws_header_table_reset(struct lws *wsi, int autoservice);

void
__lws_header_table_reset(struct lws *wsi, int autoservice);

char * LWS_WARN_UNUSED_RESULT
lws_hdr_simple_ptr(struct lws *wsi, enum lws_token_indexes h);

int LWS_WARN_UNUSED_RESULT
lws_hdr_simple_create(struct lws *wsi, enum lws_token_indexes h, const char *s);

int LWS_WARN_UNUSED_RESULT
lws_ensure_user_space(struct lws *wsi);

int LWS_WARN_UNUSED_RESULT
lws_change_pollfd(struct lws *wsi, int _and, int _or);

#if defined(LWS_WITH_SERVER)
 int _lws_vhost_init_server(const struct lws_context_creation_info *info,
			      struct lws_vhost *vhost);
struct lws_vhost *
 lws_select_vhost(struct lws_context *context, int port, const char *servername);
int LWS_WARN_UNUSED_RESULT
 lws_parse_ws(struct lws *wsi, unsigned char **buf, size_t len);
void
 lws_server_get_canonical_hostname(struct lws_context *context,
				   const struct lws_context_creation_info *info);
#else
 #define _lws_vhost_init_server(_a, _b) (0)
 #define lws_parse_ws(_a, _b, _c) (0)
 #define lws_server_get_canonical_hostname(_a, _b)
#endif

int
__remove_wsi_socket_from_fds(struct lws *wsi);

enum {
	LWSRXFC_ERROR = -1,
	LWSRXFC_CACHED = 0,
	LWSRXFC_ADDITIONAL = 1,
	LWSRXFC_TRIMMED = 2,
};


int
_lws_plat_service_forced_tsi(struct lws_context *context, int tsi);

int
lws_rxflow_cache(struct lws *wsi, unsigned char *buf, size_t n, size_t len);

int
lws_service_flag_pending(struct lws_context *context, int tsi);

int
lws_has_buffered_out(struct lws *wsi);

int LWS_WARN_UNUSED_RESULT
lws_ws_client_rx_sm(struct lws *wsi, unsigned char c);

lws_parser_return_t LWS_WARN_UNUSED_RESULT
lws_parse(struct lws *wsi, unsigned char *buf, int *len);

int LWS_WARN_UNUSED_RESULT
lws_parse_urldecode(struct lws *wsi, uint8_t *_c);

void
lws_sa46_copy_address(lws_sockaddr46 *sa46a, const void *in, int af);

int LWS_WARN_UNUSED_RESULT
lws_http_action(struct lws *wsi);

void
__lws_close_free_wsi_final(struct lws *wsi);
void
lws_libuv_closehandle(struct lws *wsi);
int
lws_libuv_check_watcher_active(struct lws *wsi);

#if defined(LWS_WITH_EVLIB_PLUGINS) || defined(LWS_WITH_PLUGINS)
const lws_plugin_header_t *
lws_plat_dlopen(struct lws_plugin **pplugin, const char *libpath,
		const char *sofilename, const char *_class,
		each_plugin_cb_t each, void *each_user);

int
lws_plat_destroy_dl(struct lws_plugin *p);
#endif

struct lws *
lws_adopt_socket_vhost(struct lws_vhost *vh, lws_sockfd_type accept_fd);

void
lws_vhost_bind_wsi(struct lws_vhost *vh, struct lws *wsi);
void
__lws_vhost_unbind_wsi(struct lws *wsi); /* req cx + vh lock */

void
__lws_set_timeout(struct lws *wsi, enum pending_timeout reason, int secs);
int
__lws_change_pollfd(struct lws *wsi, int _and, int _or);


int
lws_callback_as_writeable(struct lws *wsi);

int
lws_role_call_client_bind(struct lws *wsi,
			  const struct lws_client_connect_info *i);
void
lws_remove_child_from_any_parent(struct lws *wsi);

char *
lws_generate_client_ws_handshake(struct lws *wsi, char *p, const char *conn1);
int
lws_client_ws_upgrade(struct lws *wsi, const char **cce);
int
lws_create_client_ws_object(const struct lws_client_connect_info *i,
			    struct lws *wsi);
int
lws_alpn_comma_to_openssl(const char *comma, uint8_t *os, int len);
int
lws_role_call_alpn_negotiated(struct lws *wsi, const char *alpn);
int
lws_tls_server_conn_alpn(struct lws *wsi);

int
lws_ws_client_rx_sm_block(struct lws *wsi, unsigned char **buf, size_t len);
void
lws_destroy_event_pipe(struct lws *wsi);

/* socks */
int
lws_socks5c_generate_msg(struct lws *wsi, enum socks_msg_type type, ssize_t *msg_len);

int LWS_WARN_UNUSED_RESULT
__insert_wsi_socket_into_fds(struct lws_context *context, struct lws *wsi);

int LWS_WARN_UNUSED_RESULT
lws_issue_raw(struct lws *wsi, unsigned char *buf, size_t len);

lws_usec_t
__lws_seq_timeout_check(struct lws_context_per_thread *pt, lws_usec_t usnow);

lws_usec_t
__lws_ss_timeout_check(struct lws_context_per_thread *pt, lws_usec_t usnow);

struct lws * LWS_WARN_UNUSED_RESULT
lws_client_connect_2_dnsreq(struct lws *wsi);

LWS_VISIBLE struct lws * LWS_WARN_UNUSED_RESULT
lws_client_reset(struct lws **wsi, int ssl, const char *address, int port,
		 const char *path, const char *host, char weak);

struct lws * LWS_WARN_UNUSED_RESULT
lws_create_new_server_wsi(struct lws_vhost *vhost, int fixed_tsi, const char *desc);

char * LWS_WARN_UNUSED_RESULT
lws_generate_client_handshake(struct lws *wsi, char *pkt);

int
lws_handle_POLLOUT_event(struct lws *wsi, struct lws_pollfd *pollfd);

struct lws *
lws_http_client_connect_via_info2(struct lws *wsi);


struct lws *
__lws_wsi_create_with_role(struct lws_context *context, int tsi,
			 const struct lws_role_ops *ops,
			 lws_log_cx_t *log_cx_template);
int
lws_wsi_inject_to_loop(struct lws_context_per_thread *pt, struct lws *wsi);

int
lws_wsi_extract_from_loop(struct lws *wsi);


#if defined(LWS_WITH_CLIENT)
int
lws_http_client_socket_service(struct lws *wsi, struct lws_pollfd *pollfd);

int LWS_WARN_UNUSED_RESULT
lws_http_transaction_completed_client(struct lws *wsi);
#if !defined(LWS_WITH_TLS)
	#define lws_context_init_client_ssl(_a, _b) (0)
#endif
void
lws_decode_ssl_error(void);
#else
#define lws_context_init_client_ssl(_a, _b) (0)
#endif

int
__lws_rx_flow_control(struct lws *wsi);

int
_lws_change_pollfd(struct lws *wsi, int _and, int _or, struct lws_pollargs *pa);

#if defined(LWS_WITH_SERVER)
int
lws_handshake_server(struct lws *wsi, unsigned char **buf, size_t len);
#else
#define lws_server_socket_service(_b, _c) (0)
#define lws_handshake_server(_a, _b, _c) (0)
#endif

#ifdef LWS_WITH_ACCESS_LOG
int
lws_access_log(struct lws *wsi);
void
lws_prepare_access_log_info(struct lws *wsi, char *uri_ptr, int len, int meth);
#else
#define lws_access_log(_a)
#endif

#if defined(_DEBUG)
void
lws_wsi_txc_describe(struct lws_tx_credit *txc, const char *at, uint32_t sid);
#else
#define lws_wsi_txc_describe(x, y, z) { (void)x; }
#endif

int
lws_wsi_txc_check_skint(struct lws_tx_credit *txc, int32_t tx_cr);

int
lws_wsi_txc_report_manual_txcr_in(struct lws *wsi, int32_t bump);

void
lws_mux_mark_immortal(struct lws *wsi);
void
lws_http_close_immortal(struct lws *wsi);

int
lws_cgi_kill_terminated(struct lws_context_per_thread *pt);

void
lws_cgi_remove_and_kill(struct lws *wsi);

void
lws_plat_delete_socket_from_fds(struct lws_context *context,
				struct lws *wsi, int m);
void
lws_plat_insert_socket_into_fds(struct lws_context *context,
				struct lws *wsi);

int
lws_plat_change_pollfd(struct lws_context *context, struct lws *wsi,
		       struct lws_pollfd *pfd);

#if defined(LWS_WITH_SERVER) && defined(LWS_WITH_SECURE_STREAMS)
int
lws_adopt_ss_server_accept(struct lws *new_wsi);
#endif

int
lws_plat_pipe_create(struct lws *wsi);
int
lws_plat_pipe_signal(struct lws_context *ctx, int tsi);
void
lws_plat_pipe_close(struct lws *wsi);

void
lws_addrinfo_clean(struct lws *wsi);

void
lws_add_wsi_to_draining_ext_list(struct lws *wsi);
void
lws_remove_wsi_from_draining_ext_list(struct lws *wsi);
int
lws_poll_listen_fd(struct lws_pollfd *fd);
int
lws_plat_service(struct lws_context *context, int timeout_ms);
LWS_VISIBLE int
_lws_plat_service_tsi(struct lws_context *context, int timeout_ms, int tsi);

int
lws_pthread_self_to_tsi(struct lws_context *context);
const char * LWS_WARN_UNUSED_RESULT
lws_plat_inet_ntop(int af, const void *src, char *dst, socklen_t cnt);
int LWS_WARN_UNUSED_RESULT
lws_plat_inet_pton(int af, const char *src, void *dst);

void
lws_same_vh_protocol_remove(struct lws *wsi);
void
__lws_same_vh_protocol_remove(struct lws *wsi);
void
lws_same_vh_protocol_insert(struct lws *wsi, int n);

int
lws_client_stash_create(struct lws *wsi, const char **cisin);

void
lws_seq_destroy_all_on_pt(struct lws_context_per_thread *pt);

void
lws_addrinfo_clean(struct lws *wsi);

int
_lws_route_pt_close_unroutable(struct lws_context_per_thread *pt);

void
_lws_routing_entry_dump(struct lws_context *cx, lws_route_t *rou);

void
_lws_routing_table_dump(struct lws_context *cx);

#define LRR_IGNORE_PRI			(1 << 0)
#define LRR_MATCH_SRC			(1 << 1)
#define LRR_MATCH_DST			(1 << 2)

lws_route_t *
_lws_route_remove(struct lws_context_per_thread *pt, lws_route_t *robj, int flags);

void
_lws_route_table_empty(struct lws_context_per_thread *pt);

void
_lws_route_table_ifdown(struct lws_context_per_thread *pt, int idx);

lws_route_uidx_t
_lws_route_get_uidx(struct lws_context *cx);

int
_lws_route_pt_close_route_users(struct lws_context_per_thread *pt,
			        lws_route_uidx_t uidx);

lws_route_t *
_lws_route_est_outgoing(struct lws_context_per_thread *pt,
		        const lws_sockaddr46 *dest);

int
lws_sort_dns(struct lws *wsi, const struct addrinfo *result);

int
lws_broadcast(struct lws_context_per_thread *pt, int reason, void *in, size_t len);


#if defined(LWS_WITH_PEER_LIMITS)
void
lws_peer_track_wsi_close(struct lws_context *context, struct lws_peer *peer);
int
lws_peer_confirm_ah_attach_ok(struct lws_context *context,
			      struct lws_peer *peer);
void
lws_peer_track_ah_detach(struct lws_context *context, struct lws_peer *peer);
void
lws_peer_cull_peer_wait_list(struct lws_context *context);
struct lws_peer *
lws_get_or_create_peer(struct lws_vhost *vhost, lws_sockfd_type sockfd);
void
lws_peer_add_wsi(struct lws_context *context, struct lws_peer *peer,
		 struct lws *wsi);
void
lws_peer_dump_from_wsi(struct lws *wsi);
#endif

#ifdef LWS_WITH_HUBBUB
hubbub_error
html_parser_cb(const hubbub_token *token, void *pw);
#endif

#if defined(_DEBUG)
void
lws_service_assert_loop_thread(struct lws_context *cx, int tsi);
#else
#define lws_service_assert_loop_thread(_cx, _tsi)
#endif

int
lws_threadpool_tsi_context(struct lws_context *context, int tsi);

void
lws_threadpool_wsi_closing(struct lws *wsi);

void
__lws_wsi_remove_from_sul(struct lws *wsi);

void
lws_validity_confirmed(struct lws *wsi);
void
_lws_validity_confirmed_role(struct lws *wsi);

int
lws_seq_pt_init(struct lws_context_per_thread *pt);

int
lws_buflist_aware_read(struct lws_context_per_thread *pt, struct lws *wsi,
		       struct lws_tokens *ebuf, char fr, const char *hint);
int
lws_buflist_aware_finished_consuming(struct lws *wsi, struct lws_tokens *ebuf,
				     int used, int buffered, const char *hint);

extern const struct lws_protocols protocol_abs_client_raw_skt,
				  protocol_abs_client_unit_test;

void
__lws_reset_wsi(struct lws *wsi);

void
lws_metrics_dump(struct lws_context *ctx);

void
lws_inform_client_conn_fail(struct lws *wsi, void *arg, size_t len);

#if defined(LWS_WITH_SYS_ASYNC_DNS)
lws_async_dns_server_check_t
lws_plat_asyncdns_init(struct lws_context *context, lws_sockaddr46 *sa);
int
lws_async_dns_init(struct lws_context *context);
void
lws_async_dns_deinit(lws_async_dns_t *dns);
#endif

int
lws_protocol_init_vhost(struct lws_vhost *vh, int *any);
int
_lws_generic_transaction_completed_active_conn(struct lws **wsi, char take_vh_lock);

#define ACTIVE_CONNS_SOLO 0
#define ACTIVE_CONNS_MUXED 1
#define ACTIVE_CONNS_QUEUED 2
#define ACTIVE_CONNS_FAILED 3

#if defined(_DEBUG) && !defined(LWS_PLAT_FREERTOS) && !defined(WIN32) && !defined(LWS_PLAT_OPTEE)

int
sanity_assert_no_wsi_traces(const struct lws_context *context, struct lws *wsi);
int
sanity_assert_no_sockfd_traces(const struct lws_context *context,
			       lws_sockfd_type sfd);
#else
static inline int sanity_assert_no_wsi_traces(const struct lws_context *context, struct lws *wsi) { (void)context; (void)wsi; return 0; }
static inline int sanity_assert_no_sockfd_traces(const struct lws_context *context, lws_sockfd_type sfd) { (void)context; (void)sfd; return 0; }
#endif


void
delete_from_fdwsi(const struct lws_context *context, struct lws *wsi);

int
lws_vhost_active_conns(struct lws *wsi, struct lws **nwsi, const char *adsin);

const char *
lws_wsi_client_stash_item(struct lws *wsi, int stash_idx, int hdr_idx);

int
lws_plat_BINDTODEVICE(lws_sockfd_type fd, const char *ifname);

int
lws_socks5c_ads_server(struct lws_vhost *vh,
		       const struct lws_context_creation_info *info);

int
lws_socks5c_handle_state(struct lws *wsi, struct lws_pollfd *pollfd,
			 const char **pcce);

int
lws_socks5c_greet(struct lws *wsi, const char **pcce);

int
lws_plat_mbedtls_net_send(void *ctx, const uint8_t *buf, size_t len);

int
lws_plat_mbedtls_net_recv(void *ctx, unsigned char *buf, size_t len);

lws_usec_t
lws_sul_nonmonotonic_adjust(struct lws_context *ctx, int64_t step_us);

void
__lws_vhost_destroy_pt_wsi_dieback_start(struct lws_vhost *vh);

int
lws_vhost_compare_listen(struct lws_vhost *v1, struct lws_vhost *v2);

void
lws_netdev_instance_remove_destroy(struct lws_netdev_instance *ni);

int
lws_score_dns_results(struct lws_context *ctx,
			     const struct addrinfo **result);

#if defined(LWS_WITH_SYS_SMD)
int
lws_netdev_smd_cb(void *opaque, lws_smd_class_t _class, lws_usec_t timestamp,
		  void *buf, size_t len);
#endif

void
lws_netdev_instance_create(lws_netdev_instance_t *ni, struct lws_context *ctx,
			   const lws_netdev_ops_t *ops, const char *name,
			   void *platinfo);

int
lws_netdev_wifi_rssi_sort_compare(const lws_dll2_t *d, const lws_dll2_t *i);
void
lws_netdev_wifi_scan_empty(lws_netdev_instance_wifi_t *wnd);

lws_wifi_sta_t *
lws_netdev_wifi_scan_find(lws_netdev_instance_wifi_t *wnd, const char *ssid,
			  const uint8_t *bssid);

int
lws_netdev_wifi_scan_select(lws_netdev_instance_wifi_t *wnd);

lws_wifi_creds_t *
lws_netdev_credentials_find(lws_netdevs_t *netdevs, const char *ssid,
			    const uint8_t *bssid);

int
lws_netdev_wifi_redo_last(lws_netdev_instance_wifi_t *wnd);

void
lws_ntpc_trigger(struct lws_context *ctx);

void
lws_netdev_wifi_scan(lws_sorted_usec_list_t *sul);

#define lws_netdevs_from_ndi(ni) \
		lws_container_of((ni)->list.owner, lws_netdevs_t, owner)

#define lws_context_from_netdevs(nd) \
		lws_container_of(nd, struct lws_context, netdevs)

/* get the owner of the ni, then compute the context the owner is embedded in */
#define netdev_instance_to_ctx(ni) \
		lws_container_of(lws_netdevs_from_ndi(ni), \
				 struct lws_context, netdevs)

enum {
	LW5CHS_RET_RET0,
	LW5CHS_RET_BAIL3,
	LW5CHS_RET_STARTHS,
	LW5CHS_RET_NOTHING
};

void
lws_4to6(uint8_t *v6addr, const uint8_t *v4addr);
void
lws_sa46_4to6(lws_sockaddr46 *sa46, const uint8_t *v4addr, uint16_t port);

#ifdef __cplusplus
};
#endif

#endif
