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

/* current SS Serialization protocol version */
#define LWS_SSS_CLIENT_PROTOCOL_VERSION 1

/*
 * Secure Stream state
 */

typedef enum {
	SSSEQ_IDLE,
	SSSEQ_TRY_CONNECT,
	SSSEQ_TRY_CONNECT_NAUTH,
	SSSEQ_TRY_CONNECT_SAUTH,
	SSSEQ_RECONNECT_WAIT,
	SSSEQ_DO_RETRY,
	SSSEQ_CONNECTED,
} lws_ss_seq_state_t;

struct conn;

/**
 * lws_ss_handle_t: publicly-opaque secure stream object implementation
 */

typedef struct lws_ss_handle {
	lws_ss_info_t		info;	  /**< copy of stream creation info */

	lws_lifecycle_t		lc;

#if defined(LWS_WITH_SYS_METRICS)
	lws_metrics_caliper_compose(cal_txn)
#endif

	struct lws_dll2		list;	  /**< pt lists active ss */
	struct lws_dll2		to_list;  /**< pt lists ss with pending to-s */
#if defined(LWS_WITH_SERVER)
	struct lws_dll2		cli_list;  /**< same server clients list */
#endif
#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	lws_fi_ctx_t		fic;	/**< Fault Injection context */
#endif

	struct lws_dll2_owner	src_list; /**< sink's list of bound sources */

	struct lws_context      *context; /**< lws context we are created on */
	const lws_ss_policy_t	*policy;  /**< system policy for stream */

	struct lws_sequencer	*seq;	  /**< owning sequencer if any */
	struct lws		*wsi;	  /**< the stream wsi if any */

	struct conn		*conn_if_sspc_onw;

#if defined(LWS_WITH_SSPLUGINS)
	void			*nauthi;  /**< the nauth plugin instance data */
	void			*sauthi;  /**< the sauth plugin instance data */
#endif

	lws_ss_metadata_t	*metadata;
#if defined(LWS_WITH_SS_DIRECT_PROTOCOL_STR)
	lws_ss_metadata_t	*instant_metadata; /**< for set instant metadata */
	struct lwsac            *imd_ac;           /**< for get custom header */
#endif
	const lws_ss_policy_t	*rideshare;
	struct lws_ss_handle	*h_in_svc;

#if defined(LWS_WITH_CONMON)
	char			*conmon_json;
#endif

	//struct lws_ss_handle	*h_sink;  /**< sink we are bound to, or NULL */
	//void 			*sink_obj;/**< sink's private object representing us */

	lws_sorted_usec_list_t	sul_timeout;
	lws_sorted_usec_list_t	sul;
	lws_ss_tx_ordinal_t	txord;

	/* protocol-specific connection helpers */

	union {

		/* ...for http-related protocols... */

		struct {

			/* common to all http-related protocols */

			/* incoming multipart parsing */

			char boundary[24];	/* --boundary from headers */
			uint8_t boundary_len;	/* length of --boundary */
			uint8_t boundary_seq;	/* current match amount */
			uint8_t boundary_dashes; /* check for -- after */
			uint8_t boundary_post; /* swallow post CRLF */

			uint8_t som:1;	/* SOM has been sent */
			uint8_t eom:1;  /* EOM has been sent */
			uint8_t any:1;	/* any content has been sent */


			uint8_t good_respcode:1; /* 200 type response code */

			union {
				struct { /* LWSSSP_H1 */
#if defined(WIN32)
					uint8_t dummy;
#endif
				} h1;
				struct { /* LWSSSP_H2 */
#if defined(WIN32)
					uint8_t dummy;
#endif
				} h2;
				struct { /* LWSSSP_WS */
#if defined(WIN32)
					uint8_t dummy;
#endif
				} ws;
			} u;
		} http;

		/* details for non-http related protocols... */
#if defined(LWS_ROLE_MQTT)
		struct {
			lws_mqtt_topic_elem_t		topic_qos;
			lws_mqtt_topic_elem_t		sub_top;
			lws_mqtt_subscribe_param_t 	sub_info;
			lws_mqtt_subscribe_param_t 	shadow_sub;
			/* allocation that must be destroyed with conn */
			void				*heap_baggage;
			const char			*subscribe_to;
			size_t				subscribe_to_len;
			struct lws_buflist		*buflist_unacked;
			uint32_t			unacked_size;
			uint8_t				retry_count;
			uint8_t				send_unacked:1;
		} mqtt;
#endif
#if defined(LWS_WITH_SYS_SMD)
		struct {
			struct lws_smd_peer		*smd_peer;
			lws_sorted_usec_list_t		sul_write;
		} smd;
#endif
	} u;

	unsigned long		writeable_len;

	lws_ss_constate_t	connstate;/**< public connection state */
	lws_ss_seq_state_t	seqstate; /**< private connection state */
	lws_ss_state_return_t	pending_ret; /**< holds desired disposition
						* for ss during CCE */

#if defined(LWS_WITH_SERVER)
	int			txn_resp;
#endif

	uint16_t		retry;	  /**< retry / backoff tracking */
#if defined(LWS_WITH_CONMON)
	uint16_t		conmon_len;
#endif
	int16_t			temp16;

	uint8_t			tsi;	  /**< service thread idx, usually 0 */
	uint8_t			subseq;	  /**< emulate SOM tracking */
	uint8_t			txn_ok;	  /**< 1 = transaction was OK */
	uint8_t			prev_ss_state;

	uint8_t			txn_resp_set:1; /**< user code set one */
	uint8_t			txn_resp_pending:1; /**< we have yet to send */
	uint8_t			hanging_som:1;
	uint8_t			inside_msg:1;
	uint8_t			being_serialized:1; /* we are not the consumer */
	uint8_t			destroying:1;
	uint8_t			ss_dangling_connected:1;
	uint8_t			proxy_onward:1; /* opaque is conn */
	uint8_t			inside_connect:1; /* set if we are currently
						   * creating the onward
						   * connect */
} lws_ss_handle_t;

/* connection helper that doesn't need to hang around after connection starts */

union lws_ss_contemp {
#if defined(LWS_ROLE_MQTT)
	lws_mqtt_client_connect_param_t ccp;
#else
#if defined(WIN32)
	uint8_t	dummy;
#endif
#endif
};

/*
 * When allocating the opaque handle, we overallocate for:
 *
 *  1) policy->nauth_plugin->alloc (.nauthi) if any
 *  2) policy->sauth_plugin->alloc (.sauthi) if any
 *  3) copy of creation info stream type pointed to by info.streamtype... this
 *     may be arbitrarily long and since it may be coming from socket ipc and be
 *     temporary at creation time, we need a place for the copy to stay in scope
 *  4) copy of info->streamtype contents
 */


/* the user object allocation is immediately after the ss object allocation */
#define ss_to_userobj(ss) ((void *)&(ss)[1])

/*
 * serialization parser state
 */

enum {
	KIND_C_TO_P,
	KIND_SS_TO_P,
};

struct lws_ss_serialization_parser {
	char			streamtype[32];
	char			rideshare[32];
	char			metadata_name[32];

	uint64_t		ust_pwait;

	lws_ss_metadata_t	*ssmd;
	uint8_t			*rxmetaval;

	int			ps;
	int			ctr;

	uint32_t		usd_phandling;
	uint32_t		flags;
	uint32_t		client_pid;
	int32_t			temp32;

	int32_t			txcr_out;
	int32_t			txcr_in;
	uint16_t		rem;

	uint8_t			type;
	uint8_t			frag1;
	uint8_t			slen;
	uint8_t			rsl_pos;
	uint8_t			rsl_idx;
	uint8_t			protocol_version;
};

/*
 * Unlike locally-fulfilled SS, SSS doesn't have to hold metadata on client side
 * but pass it through to the proxy.  The client side doesn't know the real
 * metadata names that are available in the policy (since it's hardcoded in code
 * no point passing them back to the client from the policy).  Because of that,
 * it doesn't know how many to allocate when we create the sspc_handle either.
 *
 * So we use a linked-list of changed-but-not-yet-proxied metadata allocated
 * on the heap and items removed as they are proxied out.  Anything on the list
 * is sent to the proxy before any requested tx is handled.
 *
 * This is also used to queue tx credit changes
 */

typedef struct lws_sspc_metadata {
	lws_dll2_t	list;
	char		name[32];  /* empty string, then actually TCXR */
	size_t		len;
	int		tx_cr_adjust;

	/* the value of length .len is overallocated after this */
} lws_sspc_metadata_t;

/* state of the upstream proxy onward connection */

enum {
	LWSSSPC_ONW_NONE,
	LWSSSPC_ONW_REQ,
	LWSSSPC_ONW_ONGOING,
	LWSSSPC_ONW_CONN,
};

typedef struct lws_sspc_handle {
	char			rideshare_list[128];

	lws_lifecycle_t		lc;

	lws_ss_info_t		ssi;
	lws_sorted_usec_list_t	sul_retry;

	struct lws_ss_serialization_parser parser;

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	lws_fi_ctx_t		fic;	/**< Fault Injection context */
#endif

	lws_dll2_owner_t	metadata_owner;
	lws_dll2_owner_t	metadata_owner_rx;

	struct lws_dll2		client_list;
	struct lws_tx_credit	txc;

#if defined(LWS_WITH_SYS_METRICS)
	lws_metrics_caliper_compose(cal_txn)
#endif

	struct lws		*cwsi;

	struct lws_dsh		*dsh;
	struct lws_context	*context;

	struct lws_sspc_handle	*h_in_svc;
	/*
	 * Used to detect illegal lws_sspc_destroy() calls while still
	 * being serviced
	 */

	lws_usec_t		us_earliest_write_req;
	lws_usec_t		us_start_upstream;

	unsigned long		writeable_len;

	lws_ss_conn_states_t	state;

	uint32_t		timeout_ms;
	uint32_t		ord;

	int16_t			temp16;

	uint8_t			rideshare_ofs[4];
	uint8_t			rsidx;

	uint8_t			prev_ss_state;

	uint8_t			conn_req_state:2;
	uint8_t			destroying:1;
	uint8_t			non_wsi:1;
	uint8_t			ignore_txc:1;
	uint8_t			pending_timeout_update:1;
	uint8_t			pending_writeable_len:1;
	uint8_t			creating_cb_done:1;
	uint8_t			ss_dangling_connected:1;
} lws_sspc_handle_t;

typedef struct backoffs {
	struct backoffs *next;
	const char *name;
	lws_retry_bo_t r;
} backoff_t;

union u {
	backoff_t		*b;
	lws_ss_x509_t		*x;
	lws_ss_trust_store_t	*t;
	lws_ss_policy_t		*p;
	lws_ss_auth_t		*a;
	lws_metric_policy_t	*m;
};

enum {
	LTY_BACKOFF,
	LTY_X509,
	LTY_TRUSTSTORE,
	LTY_POLICY,
	LTY_AUTH,
	LTY_METRICS,

	_LTY_COUNT /* always last */
};


struct policy_cb_args {
	struct lejp_ctx jctx;
	struct lws_context *context;
	struct lwsac *ac;

	const char *socks5_proxy;

	struct lws_b64state b64;

	lws_ss_http_respmap_t respmap[16];

	union u heads[_LTY_COUNT];
	union u curr[_LTY_COUNT];

	uint8_t *p;

	int count;
	char pending_respmap;

	uint8_t parse_data:1;
};

#if defined(LWS_WITH_SYS_SMD)
extern const lws_ss_policy_t pol_smd;
#endif


/*
 * returns one of
 *
 * 	LWSSSSRET_OK
 *	LWSSSSRET_DISCONNECT_ME
 *	LWSSSSRET_DESTROY_ME
 */
int
lws_ss_deserialize_parse(struct lws_ss_serialization_parser *par,
			 struct lws_context *context,
			 struct lws_dsh *dsh, const uint8_t *cp, size_t len,
			 lws_ss_conn_states_t *state, void *parconn,
			 lws_ss_handle_t **pss, lws_ss_info_t *ssi, char client);
int
lws_ss_serialize_rx_payload(struct lws_dsh *dsh, const uint8_t *buf,
			    size_t len, int flags, const char *rsp);
int
lws_ss_deserialize_tx_payload(struct lws_dsh *dsh, struct lws *wsi,
			      lws_ss_tx_ordinal_t ord, uint8_t *buf,
			      size_t *len, int *flags);
int
lws_ss_serialize_state(struct lws *wsi, struct lws_dsh *dsh, lws_ss_constate_t state,
		       lws_ss_tx_ordinal_t ack);

const lws_ss_policy_t *
lws_ss_policy_lookup(const struct lws_context *context, const char *streamtype);

/* can be used as a cb from lws_dll2_foreach_safe() to destroy ss */
int
lws_ss_destroy_dll(struct lws_dll2 *d, void *user);

int
lws_sspc_destroy_dll(struct lws_dll2 *d, void *user);

void
lws_sspc_rxmetadata_destroy(lws_sspc_handle_t *h);

int
lws_ss_policy_set(struct lws_context *context, const char *name);

int
lws_ss_sys_fetch_policy(struct lws_context *context);

lws_ss_state_return_t
lws_ss_event_helper(lws_ss_handle_t *h, lws_ss_constate_t cs);

lws_ss_state_return_t
_lws_ss_backoff(lws_ss_handle_t *h, lws_usec_t us_override);

lws_ss_state_return_t
lws_ss_backoff(lws_ss_handle_t *h);

int
_lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(lws_ss_state_return_t r, struct lws *wsi,
			 lws_ss_handle_t **ph);

int
lws_ss_set_timeout_us(lws_ss_handle_t *h, lws_usec_t us);

void
ss_proxy_onward_txcr(void *userobj, int bump);

int
lws_ss_serialize_txcr(struct lws_dsh *dsh, int txcr);

int
lws_ss_sys_auth_api_amazon_com(struct lws_context *context);

lws_ss_metadata_t *
lws_ss_get_handle_metadata(struct lws_ss_handle *h, const char *name);
lws_ss_metadata_t *
lws_ss_policy_metadata_index(const lws_ss_policy_t *p, size_t index);

#if defined(LWS_WITH_SS_DIRECT_PROTOCOL_STR)
lws_ss_metadata_t *
lws_ss_get_handle_instant_metadata(struct lws_ss_handle *h, const char *name);
#endif

lws_ss_metadata_t *
lws_ss_policy_metadata(const lws_ss_policy_t *p, const char *name);

int
lws_ss_exp_cb_metadata(void *priv, const char *name, char *out, size_t *pos,
			size_t olen, size_t *exp_ofs);

int
_lws_ss_set_metadata(lws_ss_metadata_t *omd, const char *name,
		     const void *value, size_t len);

int
_lws_ss_alloc_set_metadata(lws_ss_metadata_t *omd, const char *name,
			   const void *value, size_t len);

lws_ss_state_return_t
_lws_ss_client_connect(lws_ss_handle_t *h, int is_retry, void *conn_if_sspc_onw);

lws_ss_state_return_t
_lws_ss_request_tx(lws_ss_handle_t *h);

int
__lws_ss_proxy_bind_ss_to_conn_wsi(void *parconn, size_t dsh_size);

struct lws_vhost *
lws_ss_policy_ref_trust_store(struct lws_context *context,
			      const lws_ss_policy_t *pol, char doref);

lws_ss_state_return_t
lws_sspc_event_helper(lws_sspc_handle_t *h, lws_ss_constate_t cs,
		      lws_ss_tx_ordinal_t flags);

int
lws_ss_check_next_state(lws_lifecycle_t *lc, uint8_t *prevstate,
			lws_ss_constate_t cs);

int
lws_ss_check_next_state_ss(lws_ss_handle_t *ss, uint8_t *prevstate,
			   lws_ss_constate_t cs);

int
lws_ss_check_next_state_sspc(lws_sspc_handle_t *ss, uint8_t *prevstate,
			     lws_ss_constate_t cs);

void
lws_proxy_clean_conn_ss(struct lws *wsi);

int
lws_ss_cancel_notify_dll(struct lws_dll2 *d, void *user);

int
lws_sspc_cancel_notify_dll(struct lws_dll2 *d, void *user);

#if defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY) || defined(LWS_WITH_SECURE_STREAMS_CPP)
int
lws_ss_policy_unref_trust_store(struct lws_context *context,
				const lws_ss_policy_t *pol);
#endif

int
lws_ss_sys_cpd(struct lws_context *cx);

#if defined(LWS_WITH_SECURE_STREAMS_AUTH_SIGV4)
int lws_ss_apply_sigv4(struct lws *wsi, struct lws_ss_handle *h,
		       unsigned char **p, unsigned char *end);
#endif

#if defined(_DEBUG)
void
lws_ss_assert_extant(struct lws_context *cx, int tsi, struct lws_ss_handle *h);
#else
#define lws_ss_assert_extant(_a, _b, _c)
#endif

typedef int (* const secstream_protocol_connect_munge_t)(lws_ss_handle_t *h,
		char *buf, size_t len, struct lws_client_connect_info *i,
		union lws_ss_contemp *ct);

typedef int (* const secstream_protocol_add_txcr_t)(lws_ss_handle_t *h, int add);

typedef int (* const secstream_protocol_get_txcr_t)(lws_ss_handle_t *h);

struct ss_pcols {
	const char					*name;
	const char					*alpn;
	const struct lws_protocols			*protocol;
	secstream_protocol_connect_munge_t		munge;
	secstream_protocol_add_txcr_t			tx_cr_add;
	secstream_protocol_get_txcr_t			tx_cr_est;
};

/*
 * Because both sides of the connection share the conn, we allocate it
 * during accepted adoption, and both sides point to it.
 *
 * When .ss or .wsi close, they must NULL their entry here so no dangling
 * refereneces.
 *
 * The last one of the accepted side and the onward side to close frees it.
 */

lws_ss_state_return_t
lws_conmon_ss_json(lws_ss_handle_t *h);

void
ss_proxy_onward_link_req_writeable(lws_ss_handle_t *h_onward);

struct conn {
	struct lws_ss_serialization_parser parser;

	lws_dsh_t		*dsh;	/* unified buffer for both sides */
	struct lws		*wsi;	/* the proxy's client side */
	lws_ss_handle_t		*ss;	/* the onward, ss side */

	lws_ss_conn_states_t	state;

	char			onward_in_flow_control;
};

extern const struct ss_pcols ss_pcol_h1;
extern const struct ss_pcols ss_pcol_h2;
extern const struct ss_pcols ss_pcol_ws;
extern const struct ss_pcols ss_pcol_mqtt;
extern const struct ss_pcols ss_pcol_raw;

extern const struct lws_protocols protocol_secstream_h1;
extern const struct lws_protocols protocol_secstream_h2;
extern const struct lws_protocols protocol_secstream_ws;
extern const struct lws_protocols protocol_secstream_mqtt;
extern const struct lws_protocols protocol_secstream_raw;

