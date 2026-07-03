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

extern const struct lws_role_ops role_ops_h2;
#define lwsi_role_h2(wsi) (wsi->role_ops == &role_ops_h2)

struct http2_settings {
	uint32_t s[H2SET_COUNT];
};

struct lws_vhost_role_h2 {
	struct http2_settings set;
};

enum lws_h2_wellknown_frame_types {
	LWS_H2_FRAME_TYPE_DATA,
	LWS_H2_FRAME_TYPE_HEADERS,
	LWS_H2_FRAME_TYPE_PRIORITY,
	LWS_H2_FRAME_TYPE_RST_STREAM,
	LWS_H2_FRAME_TYPE_SETTINGS,
	LWS_H2_FRAME_TYPE_PUSH_PROMISE,
	LWS_H2_FRAME_TYPE_PING,
	LWS_H2_FRAME_TYPE_GOAWAY,
	LWS_H2_FRAME_TYPE_WINDOW_UPDATE,
	LWS_H2_FRAME_TYPE_CONTINUATION,

	LWS_H2_FRAME_TYPE_COUNT /* always last */
};

enum lws_h2_flags {
	LWS_H2_FLAG_END_STREAM = 1,
	LWS_H2_FLAG_END_HEADERS = 4,
	LWS_H2_FLAG_PADDED = 8,
	LWS_H2_FLAG_PRIORITY = 0x20,

	LWS_H2_FLAG_SETTINGS_ACK = 1,
};

enum lws_h2_errors {
	H2_ERR_NO_ERROR,		   /* Graceful shutdown */
	H2_ERR_PROTOCOL_ERROR,	   /* Protocol error detected */
	H2_ERR_INTERNAL_ERROR,	   /* Implementation fault */
	H2_ERR_FLOW_CONTROL_ERROR,  /* Flow-control limits exceeded */
	H2_ERR_SETTINGS_TIMEOUT,	   /* Settings not acknowledged */
	H2_ERR_STREAM_CLOSED,	   /* Frame received for closed stream */
	H2_ERR_FRAME_SIZE_ERROR,	   /* Frame size incorrect */
	H2_ERR_REFUSED_STREAM,	   /* Stream not processed */
	H2_ERR_CANCEL,		   /* Stream cancelled */
	H2_ERR_COMPRESSION_ERROR,   /* Compression state not updated */
	H2_ERR_CONNECT_ERROR,	   /* TCP connection error for CONNECT method */
	H2_ERR_ENHANCE_YOUR_CALM,   /* Processing capacity exceeded */
	H2_ERR_INADEQUATE_SECURITY, /* Negotiated TLS parameters not acceptable */
	H2_ERR_HTTP_1_1_REQUIRED,   /* Use HTTP/1.1 for the request */
};

enum lws_h2_states {
	LWS_H2_STATE_IDLE,
	/*
	 * Send PUSH_PROMISE    -> LWS_H2_STATE_RESERVED_LOCAL
	 * Recv PUSH_PROMISE    -> LWS_H2_STATE_RESERVED_REMOTE
	 * Send HEADERS         -> LWS_H2_STATE_OPEN
	 * Recv HEADERS         -> LWS_H2_STATE_OPEN
	 *
	 *  - Only PUSH_PROMISE + HEADERS valid to send
	 *  - Only HEADERS or PRIORITY valid to receive
	 */
	LWS_H2_STATE_RESERVED_LOCAL,
	/*
	 * Send RST_STREAM      -> LWS_H2_STATE_CLOSED
	 * Recv RST_STREAM      -> LWS_H2_STATE_CLOSED
	 * Send HEADERS         -> LWS_H2_STATE_HALF_CLOSED_REMOTE
	 *
	 * - Only HEADERS, RST_STREAM, or PRIORITY valid to send
	 * - Only RST_STREAM, PRIORITY, or WINDOW_UPDATE valid to receive
	 */
	LWS_H2_STATE_RESERVED_REMOTE,
	/*
	 * Send RST_STREAM      -> LWS_H2_STATE_CLOSED
	 * Recv RST_STREAM      -> LWS_H2_STATE_CLOSED
	 * Recv HEADERS         -> LWS_H2_STATE_HALF_CLOSED_LOCAL
	 *
	 *  - Only RST_STREAM, WINDOW_UPDATE, or PRIORITY valid to send
	 *  - Only HEADERS, RST_STREAM, or PRIORITY valid to receive
	 */
	LWS_H2_STATE_OPEN,
	/*
	 * Send RST_STREAM      -> LWS_H2_STATE_CLOSED
	 * Recv RST_STREAM      -> LWS_H2_STATE_CLOSED
	 * Send END_STREAM flag -> LWS_H2_STATE_HALF_CLOSED_LOCAL
	 * Recv END_STREAM flag -> LWS_H2_STATE_HALF_CLOSED_REMOTE
	 */
	LWS_H2_STATE_HALF_CLOSED_REMOTE,
	/*
	 * Send RST_STREAM      -> LWS_H2_STATE_CLOSED
	 * Recv RST_STREAM      -> LWS_H2_STATE_CLOSED
	 * Send END_STREAM flag -> LWS_H2_STATE_CLOSED
	 *
	 *  - Any frame valid to send
	 *  - Only WINDOW_UPDATE, PRIORITY, or RST_STREAM valid to receive
	 */
	LWS_H2_STATE_HALF_CLOSED_LOCAL,
	/*
	 * Send RST_STREAM      -> LWS_H2_STATE_CLOSED
	 * Recv RST_STREAM      -> LWS_H2_STATE_CLOSED
	 * Recv END_STREAM flag -> LWS_H2_STATE_CLOSED
	 *
	 *  - Only WINDOW_UPDATE, PRIORITY, and RST_STREAM valid to send
	 *  - Any frame valid to receive
	 */
	LWS_H2_STATE_CLOSED,
	/*
	 *  - Only PRIORITY, WINDOW_UPDATE (IGNORE) and RST_STREAM (IGNORE)
	 *     may be received
	 *
	 *  - Only PRIORITY valid to send
	 */
};

void
lws_h2_state(struct lws *wsi, enum lws_h2_states s);

#define LWS_H2_STREAM_ID_MASTER 0
#define LWS_H2_SETTINGS_LEN 6
#define LWS_H2_FLAG_SETTINGS_ACK 1

enum http2_hpack_state {
	HPKS_TYPE,

	HPKS_IDX_EXT,

	HPKS_HLEN,
	HPKS_HLEN_EXT,

	HPKS_DATA,
};

/*
 * lws general parsimonious header strategy is only store values from known
 * headers, and refer to them by index.
 *
 * That means if we can't map the peer header name to one that lws knows, we
 * will drop the content but track the indexing with associated_lws_hdr_idx =
 * LWS_HPACK_IGNORE_ENTRY.
 */

enum http2_hpack_type {
	HPKT_INDEXED_HDR_7,		/* 1xxxxxxx: just "header field" */
	HPKT_INDEXED_HDR_6_VALUE_INCR,  /* 01xxxxxx: NEW indexed hdr with value */
	HPKT_LITERAL_HDR_VALUE_INCR,	/* 01000000: NEW literal hdr with value */
	HPKT_INDEXED_HDR_4_VALUE,	/* 0000xxxx: indexed hdr with value */
	HPKT_INDEXED_HDR_4_VALUE_NEVER,	/* 0001xxxx: indexed hdr with value NEVER NEW */
	HPKT_LITERAL_HDR_VALUE,		/* 00000000: literal hdr with value */
	HPKT_LITERAL_HDR_VALUE_NEVER,	/* 00010000: literal hdr with value NEVER NEW */
	HPKT_SIZE_5
};

#define LWS_HPACK_IGNORE_ENTRY 0xffff


struct hpack_dt_entry {
	char *value; /* malloc'd */
	uint16_t value_len;
	uint16_t hdr_len; /* virtual, for accounting */
	uint16_t lws_hdr_idx; /* LWS_HPACK_IGNORE_ENTRY = IGNORE */
};

struct hpack_dynamic_table {
	struct hpack_dt_entry *entries; /* malloc'd */
	uint32_t virtual_payload_usage;
	uint32_t virtual_payload_max;
	uint16_t pos;
	uint16_t used_entries;
	uint16_t num_entries;
};

enum lws_h2_protocol_send_type {
	LWS_PPS_NONE,
	LWS_H2_PPS_MY_SETTINGS,
	LWS_H2_PPS_ACK_SETTINGS,
	LWS_H2_PPS_PING,
	LWS_H2_PPS_PONG,
	LWS_H2_PPS_GOAWAY,
	LWS_H2_PPS_RST_STREAM,
	LWS_H2_PPS_UPDATE_WINDOW,
	LWS_H2_PPS_SETTINGS_INITIAL_UPDATE_WINDOW
};

struct lws_h2_protocol_send {
	struct lws_h2_protocol_send *next; /* linked list */
	enum lws_h2_protocol_send_type type;

	union uu {
		struct {
			char		str[32];
			uint32_t	highest_sid;
			uint32_t	err;
		} ga;
		struct {
			uint32_t	sid;
			uint32_t	err;
		} rs;
		struct {
			uint8_t		ping_payload[8];
		} ping;
		struct {
			uint32_t	sid;
			uint32_t	credit;
		} update_window;
	} u;
};

struct lws_h2_ghost_sid {
	struct lws_h2_ghost_sid *next;
	uint32_t sid;
};

/*
 * http/2 connection info that is only used by the root connection that has
 * the network connection.
 *
 * h2 tends to spawn many child connections from one network connection, so
 * it's necessary to make members only needed by the network connection
 * distinct and only malloc'd on network connections.
 *
 * There's only one HPACK parser per network connection.
 *
 * But there is an ah per logical child connection... the network connection
 * fills it but it belongs to the logical child.
 */
struct lws_h2_netconn {
	struct http2_settings our_set;
	struct http2_settings peer_set;
	struct hpack_dynamic_table hpack_dyn_table;
	uint8_t	ping_payload[8];
	uint8_t one_setting[LWS_H2_SETTINGS_LEN];
	char goaway_str[32]; /* for rx */
	struct lws *swsi;
	struct lws_h2_protocol_send *pps; /* linked list */

	enum http2_hpack_state hpack;
	enum http2_hpack_type hpack_type;

	unsigned int huff:1;
	unsigned int value:1;
	unsigned int unknown_header:1;
	unsigned int cont_exp:1;
	unsigned int cont_exp_headers:1;
	unsigned int we_told_goaway:1;
	unsigned int pad_length:1;
	unsigned int collected_priority:1;
	unsigned int is_first_header_char:1;
	unsigned int zero_huff_padding:1;
	unsigned int last_action_dyntable_resize:1;
	unsigned int sent_preface:1;

	uint32_t hdr_idx;
	uint32_t hpack_len;
	uint32_t hpack_e_dep;
	uint32_t count;
	uint32_t preamble;
	uint32_t length;
	uint32_t sid;
	uint32_t inside;
	uint32_t highest_sid;
	uint32_t highest_sid_opened;
	uint32_t cont_exp_sid;
	uint32_t dep;
	uint32_t goaway_last_sid;
	uint32_t goaway_err;
	uint32_t hpack_hdr_len;

	uint16_t hpack_pos;

	uint8_t frame_state;
	uint8_t type;
	uint8_t flags;
	uint8_t padding;
	uint8_t weight_temp;
	uint8_t huff_pad;
	char first_hdr_char;
	uint8_t hpack_m;
	uint8_t ext_count;
};

struct _lws_h2_related {

	struct lws_h2_netconn	*h2n; /* malloc'd for root net conn */

	char			*pending_status_body;

	uint8_t			h2_state; /* RFC7540 state of the connection */

	uint8_t			END_STREAM:1;
	uint8_t			END_HEADERS:1;
	uint8_t			send_END_STREAM:1;
	uint8_t			long_poll:1;
	uint8_t			initialized:1;
};

#define HTTP2_IS_TOPLEVEL_WSI(wsi) (!wsi->mux.parent_wsi)

int
lws_h2_rst_stream(struct lws *wsi, uint32_t err, const char *reason);
struct lws * lws_h2_get_nth_child(struct lws *wsi, int n);
void lws_h2_init(struct lws *wsi);
int
lws_h2_settings(struct lws *nwsi, struct http2_settings *settings,
		unsigned char *buf, int len);
int
lws_h2_parser(struct lws *wsi, unsigned char *in, lws_filepos_t inlen,
	      lws_filepos_t *inused);
int
lws_h2_do_pps_send(struct lws *wsi);
int
lws_h2_frame_write(struct lws *wsi, int type, int flags, unsigned int sid,
		   unsigned int len, unsigned char *buf);
struct lws *
lws_wsi_mux_from_id(struct lws *wsi, unsigned int sid);
int
lws_hpack_interpret(struct lws *wsi, unsigned char c);
int
lws_add_http2_header_by_name(struct lws *wsi,
			     const unsigned char *name,
			     const unsigned char *value, int length,
			     unsigned char **p, unsigned char *end);
int
lws_add_http2_header_by_token(struct lws *wsi,
			      enum lws_token_indexes token,
			      const unsigned char *value, int length,
			      unsigned char **p, unsigned char *end);
int
lws_add_http2_header_status(struct lws *wsi,
			    unsigned int code, unsigned char **p,
			    unsigned char *end);
void
lws_hpack_destroy_dynamic_header(struct lws *wsi);
int
lws_hpack_dynamic_size(struct lws *wsi, int size);
int
lws_h2_goaway(struct lws *wsi, uint32_t err, const char *reason);
int
lws_h2_tx_cr_get(struct lws *wsi);
void
lws_h2_tx_cr_consume(struct lws *wsi, int consumed);
int
lws_hdr_extant(struct lws *wsi, enum lws_token_indexes h);
void
lws_pps_schedule(struct lws *wsi, struct lws_h2_protocol_send *pss);

extern const struct http2_settings lws_h2_defaults;
int
lws_h2_ws_handshake(struct lws *wsi);
int lws_h2_issue_preface(struct lws *wsi);
int
lws_h2_client_handshake(struct lws *wsi);
struct lws *
lws_wsi_h2_adopt(struct lws *parent_wsi, struct lws *wsi);
int
lws_handle_POLLOUT_event_h2(struct lws *wsi);
int
lws_read_h2(struct lws *wsi, unsigned char *buf, lws_filepos_t len);
struct lws_h2_protocol_send *
lws_h2_new_pps(enum lws_h2_protocol_send_type type);
