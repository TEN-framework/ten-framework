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

#ifndef _PRIVATE_LIB_ROLES_MQTT
#define _PRIVATE_LIB_ROLES_MQTT 1

extern struct lws_role_ops role_ops_mqtt;

#define lwsi_role_mqtt(wsi) (wsi->role_ops == &role_ops_mqtt)

#define LWS_MQTT_MAX_CHILDREN 8 /* max child streams on same parent */

#define LMQCP_LUT_FLAG_RESERVED_FLAGS  0x10
#define LMQCP_LUT_FLAG_PACKET_ID_NONE  0x00
#define LMQCP_LUT_FLAG_PACKET_ID_HAS   0x20
#define LMQCP_LUT_FLAG_PACKET_ID_QOS12 0x40
#define LMQCP_LUT_FLAG_PACKET_ID_MASK  0x60
#define LMQCP_LUT_FLAG_PAYLOAD	       0x80	/* payload req (publish = opt)*/

#define lws_mqtt_str_is_not_empty(s) ( ((s)) &&		\
				       ((s))->len &&	\
				       ((s))->buf &&	\
				       *((s))->buf )

#define LWS_MQTT_RESPONSE_TIMEOUT      (3 * LWS_US_PER_SEC)
#define LWS_MQTT_RETRY_CEILING         (60 * LWS_US_PER_SEC)
#define LWS_MQTT_MAX_PUBLISH_RETRY 	   (3)

typedef enum {
	LMSPR_COMPLETED			=  0,
	LMSPR_NEED_MORE			=  1,

	LMSPR_FAILED_OOM		= -1,
	LMSPR_FAILED_OVERSIZE		= -2,
	LMSPR_FAILED_FORMAT		= -3,
	LMSPR_FAILED_ALREADY_COMPLETED	= -4,
} lws_mqtt_stateful_primitive_return_t;

typedef struct {
	uint32_t value;
	char budget;
	char consumed;
} lws_mqtt_vbi;

/* works for vbi, 2-byte and 4-byte fixed length */
static inline int
lws_mqtt_mb_first(lws_mqtt_vbi *vbi) { return !vbi->consumed; }

int
lws_mqtt_vbi_encode(uint32_t value, void *buf);

/*
 * Decode is done statefully on an arbitrary amount of input data (which may
 * be one byte).  It's like this so it can continue seamlessly if a buffer ends
 * partway through the primitive, and the api matches the bulk binary data case.
 *
 * VBI decode:
 *
 * Initialize the lws_mqtt_vbi state by calling lws_mqtt_vbi_init() on it, then
 * feed lws_mqtt_vbi_r() bytes to decode.
 *
 * Returns <0 for error, LMSPR_COMPLETED if done (vbi->value is valid), or
 * LMSPR_NEED_MORE if more calls to lws_mqtt_vbi_r() with subsequent bytes
 * needed.
 *
 * *in and *len are updated accordingly.
 *
 * 2-byte and 4-byte decode:
 *
 * Initialize the lws_mqtt_vbi state by calling lws_mqtt_2byte_init() or
 * lws_mqtt_4byte_init() on it, then feed lws_mqtt_mb_parse() bytes
 * to decode.
 *
 * Returns <0 for error, LMSPR_COMPLETED if done (vbi->value is valid), or
 * LMSPR_NEED_MORE if more calls to lws_mqtt_mb_parse() with subsequent
 * bytes needed.
 *
 * *in and *len are updated accordingly.
 */

void
lws_mqtt_vbi_init(lws_mqtt_vbi *vbi);

void
lws_mqtt_2byte_init(lws_mqtt_vbi *vbi);

void
lws_mqtt_4byte_init(lws_mqtt_vbi *vbi);

lws_mqtt_stateful_primitive_return_t
lws_mqtt_vbi_r(lws_mqtt_vbi *vbi, const uint8_t **in, size_t *len);

lws_mqtt_stateful_primitive_return_t
lws_mqtt_mb_parse(lws_mqtt_vbi *vbi, const uint8_t **in, size_t *len);

struct lws_mqtt_str_st {
	uint8_t		*buf;
	uint16_t	len;

	uint16_t	limit; /* it's cheaper to add the state here than
				* the pointer to point to it elsewhere */
	uint16_t	pos;
	char		len_valid;
	char		needs_freeing;
};

static inline int
lws_mqtt_str_first(struct lws_mqtt_str_st *s) { return !s->buf && !s->pos; }


lws_mqtt_stateful_primitive_return_t
lws_mqtt_str_parse(struct lws_mqtt_str_st *bd, const uint8_t **in, size_t *len);

typedef enum {
	LMQCPP_IDLE,

	/* receive packet type part of fixed header took us out of idle... */
	LMQCPP_CONNECT_PACKET = LMQCP_CTOS_CONNECT << 4,
	LMQCPP_CONNECT_REMAINING_LEN_VBI,
	LMQCPP_CONNECT_VH_PNAME,
	LMQCPP_CONNECT_VH_PVERSION,
	LMQCPP_CONNECT_VH_FLAGS,
	LMQCPP_CONNECT_VH_KEEPALIVE,
	LMQCPP_CONNECT_VH_PROPERTIES_VBI_LEN,

	LMQCPP_CONNACK_PACKET = LMQCP_STOC_CONNACK << 4,
	LMQCPP_CONNACK_VH_FLAGS,
	LMQCPP_CONNACK_VH_RETURN_CODE,

	LMQCPP_PUBLISH_PACKET = LMQCP_PUBLISH << 4,
	LMQCPP_PUBLISH_REMAINING_LEN_VBI,
	LMQCPP_PUBLISH_VH_TOPIC,
	LMQCPP_PUBLISH_VH_PKT_ID,

	LMQCPP_PUBACK_PACKET = LMQCP_PUBACK << 4,
	LMQCPP_PUBACK_VH_PKT_ID,
	LMQCPP_PUBACK_PROPERTIES_LEN_VBI,

	LMQCPP_PUBREC_PACKET = LMQCP_PUBREC << 4,
	LMQCPP_PUBREC_VH_PKT_ID,

	LMQCPP_PUBREL_PACKET = LMQCP_PUBREL << 4,
	LMQCPP_PUBREL_VH_PKT_ID,

	LMQCPP_PUBCOMP_PACKET = LMQCP_PUBCOMP << 4,
	LMQCPP_PUBCOMP_VH_PKT_ID,

	LMQCPP_SUBACK_PACKET = LMQCP_STOC_SUBACK << 4,
	LMQCPP_SUBACK_VH_PKT_ID,
	LMQCPP_SUBACK_PAYLOAD,

	LMQCPP_UNSUBACK_PACKET = LMQCP_STOC_UNSUBACK << 4,
	LMQCPP_UNSUBACK_VH_PKT_ID,

	LMQCPP_PINGRESP_ZERO = LMQCP_STOC_PINGRESP << 4,

	LMQCPP_PAYLOAD,

	LMQCPP_EAT_PROPERTIES_AND_COMPLETE,

	LMQCPP_PROP_ID_VBI,

	/* all possible property payloads */

	/* 3.3.2.3.2 */
	LMQCPP_PROP_PAYLOAD_FORMAT_INDICATOR_1BYTE			= 0x101,

	LMQCPP_PROP_MSG_EXPIRY_INTERVAL_4BYTE				= 0x102,

	LMQCPP_PROP_CONTENT_TYPE_UTF8S					= 0x103,

	LMQCPP_PROP_RESPONSE_TOPIC_UTF8S				= 0x108,

	LMQCPP_PROP_CORRELATION_BINDATA					= 0x109,

	LMQCPP_PROP_SUBSCRIPTION_ID_VBI					= 0x10b,

	LMQCPP_PROP_SESSION_EXPIRY_INTERVAL_4BYTE			= 0x111,

	LMQCPP_PROP_ASSIGNED_CLIENTID_UTF8S				= 0x112,

	LMQCPP_PROP_SERVER_KEEPALIVE_2BYTE				= 0x113,

	LMQCPP_PROP_AUTH_METHOD_UTF8S					= 0x115,

	LMQCPP_PROP_AUTH_DATA_BINDATA					= 0x116,

	LMQCPP_PROP_REQUEST_PROBLEM_INFO_1BYTE				= 0x117,

	LMQCPP_PROP_WILL_DELAY_INTERVAL_4BYTE				= 0x118,

	LMQCPP_PROP_REQUEST_REPSONSE_INFO_1BYTE				= 0x119,

	LMQCPP_PROP_RESPONSE_INFO_UTF8S					= 0x11a,

	LMQCPP_PROP_SERVER_REFERENCE_UTF8S				= 0x11c,

	LMQCPP_PROP_REASON_STRING_UTF8S					= 0x11f,

	LMQCPP_PROP_RECEIVE_MAXIMUM_2BYTE				= 0x121,

	LMQCPP_PROP_TOPIC_MAXIMUM_2BYTE					= 0x122,

	LMQCPP_PROP_TOPIC_ALIAS_2BYTE					= 0x123,

	LMQCPP_PROP_MAXIMUM_QOS_1BYTE					= 0x124,

	LMQCPP_PROP_RETAIN_AVAILABLE_1BYTE				= 0x125,

	LMQCPP_PROP_USER_PROPERTY_NAME_UTF8S				= 0x126,
	LMQCPP_PROP_USER_PROPERTY_VALUE_UTF8S				= 0x226,

	LMQCPP_PROP_MAXIMUM_PACKET_SIZE_4BYTE				= 0x127,

	LMQCPP_PROP_WILDCARD_SUBSCRIPTION_AVAILABLE_1BYTE		= 0x128,

	LMQCPP_PROP_SUBSCRIPTION_IDENTIFIER_AVAILABLE_1BYTE		= 0x129,

	LMQCPP_PROP_SHARED_SUBSCRIPTION_AVAILABLE_1BYTE			= 0x12a,

} lws_mqtt_packet_parse_state_t;

/*
 * the states an MQTT connection can be in
 */

typedef enum {
	LGSMQTT_UNKNOWN,
	LGSMQTT_IDLE,
	LGSMQTT_TRANSPORT_CONNECTED,

	LGSMQTT_SENT_CONNECT,
	LGSMQTT_ESTABLISHED,

	LGSMQTT_SENT_SUBSCRIBE,
	LGSMQTT_SUBSCRIBED,

} lwsgs_mqtt_states_t;

typedef struct lws_mqtt_parser_st {
	/* struct lws_mqtt_str_st s_content_type; */
	lws_mqtt_packet_parse_state_t state;
	lws_mqtt_vbi vbit;

	lws_mqtt_reason_t reason;

	lws_mqtt_str_t s_temp;

	uint8_t fixed_seen[4];
	uint8_t props_seen[8];

	uint8_t cpkt_flags;
	uint32_t cpkt_remlen;

	uint32_t props_len;
	uint32_t consumed;
	uint32_t prop_id;
	uint32_t props_consumed;
	uint32_t payload_consumed;

	uint16_t keepalive;
	uint16_t cpkt_id;
	uint32_t n;

	uint8_t temp[32];
	uint8_t conn_rc;
	uint8_t payload_format;
	uint8_t packet_type_flags;
	uint8_t conn_protocol_version;
	uint8_t fixed;

	uint8_t flag_pending_send_connack_close:1;
	uint8_t flag_pending_send_reason_close:1;
	uint8_t flag_prop_multi:1;
	uint8_t flag_server:1;

} lws_mqtt_parser_t;

typedef enum {
	LMVTR_VALID				=  0,
	LMVTR_VALID_WILDCARD			=  1,
	LMVTR_VALID_SHADOW			=  2,

	LMVTR_FAILED_OVERSIZE			= -1,
	LMVTR_FAILED_WILDCARD_FORMAT		= -2,
	LMVTR_FAILED_SHADOW_FORMAT		= -3,
} lws_mqtt_validate_topic_return_t;

typedef enum {
	LMMTR_TOPIC_NOMATCH			= 0,
	LMMTR_TOPIC_MATCH			= 1,

	LMMTR_TOPIC_MATCH_ERROR			= -1
} lws_mqtt_match_topic_return_t;

typedef struct lws_mqtt_subs {
	struct lws_mqtt_subs	*next;

	uint8_t			ref_count; /* number of children referencing */

	/* Flags */
	uint8_t			wildcard:1;
	uint8_t			shadow:1;

	/* subscription name + NUL overallocated here */
	char			topic[];
} lws_mqtt_subs_t;

typedef struct lws_mqtts {
	lws_mqtt_parser_t	par;
	lwsgs_mqtt_states_t	estate;
	struct lws_dll2		active_session_list_head;
	struct lws_dll2		limbo_session_list_head;
} lws_mqtts_t;

typedef struct lws_mqttc {
	lws_mqtt_parser_t	par;
	lwsgs_mqtt_states_t	estate;
	struct lws_mqtt_str_st	*id;
	struct lws_mqtt_str_st	*username;
	struct lws_mqtt_str_st	*password;
	struct {
		struct lws_mqtt_str_st	*topic;
		struct lws_mqtt_str_st	*message;
		lws_mqtt_qos_levels_t qos;
		uint8_t		retain;
	} will;
	uint16_t		keep_alive_secs;
	uint16_t			conn_flags;
	uint8_t			aws_iot;
} lws_mqttc_t;

struct _lws_mqtt_related {
	lws_mqttc_t		client;
	lws_sorted_usec_list_t	sul_qos_puback_pubrec_wait; /* QoS1 puback or QoS2 pubrec wait TO */
	lws_sorted_usec_list_t	sul_qos1_puback_wait; /* QoS1 puback wait TO */
	lws_sorted_usec_list_t	sul_unsuback_wait; /* unsuback wait TO */
	lws_sorted_usec_list_t	sul_qos2_pubrec_wait; /* QoS2 pubrec wait TO */
	lws_sorted_usec_list_t	sul_shadow_wait; /* Device Shadow wait TO */
	struct lws		*wsi; /**< so sul can use lws_container_of */
	lws_mqtt_subs_t		*subs_head; /**< Linked-list of heap-allocated subscription objects */
	void			*rx_cpkt_param;
	uint16_t		pkt_id;
	uint16_t		ack_pkt_id;
	uint16_t		peer_ack_pkt_id;
	uint16_t		sub_size;

#if defined(LWS_WITH_CLIENT)
	uint8_t 		send_pingreq:1;
	uint8_t			session_resumed:1;
#endif
	uint8_t			inside_payload:1;
	uint8_t			inside_subscribe:1;
	uint8_t			inside_unsubscribe:1;
	uint8_t			inside_birth:1;
	uint8_t			inside_resume_session:1;
	uint8_t 		send_puback:1;
	uint8_t 		send_pubrel:1;
	uint8_t 		send_pubrec:1;
	uint8_t 		send_pubcomp:1;
	uint8_t			unacked_publish:1;
	uint8_t			unacked_pubrel:1;

	uint8_t			done_subscribe:1;
	uint8_t			done_birth:1;
	uint8_t			inside_shadow:1;
	uint8_t			done_shadow_subscribe:1;
	uint8_t			send_shadow_unsubscribe:1;
};

/*
 * New sessions are created by starting CONNECT.  If the ClientID sent
 * by the client matches a different, extant session, then the
 * existing one is taken over and the new one created for duration of
 * CONNECT processing is destroyed.
 *
 * On the server side, bearing in mind multiple simultaneous,
 * fragmented CONNECTs may be interleaved ongoing, all state and
 * parsing temps for a session must live in the session object.
 */

struct lws_mqtt_endpoint_st;

typedef struct lws_mqtts_session_st {
	struct lws_dll2 session_list;

} lws_mqtts_session_t;

#define ctl_pkt_type(x) (x->packet_type_flags >> 4)


void
lws_mqttc_state_transition(lws_mqttc_t *ep, lwsgs_mqtt_states_t s);

int
_lws_mqtt_rx_parser(struct lws *wsi, lws_mqtt_parser_t *par,
		    const uint8_t *buf, size_t len);

int
lws_mqtt_client_socket_service(struct lws *wsi, struct lws_pollfd *pollfd,
			       struct lws *wsi_conn);

int
lws_create_client_mqtt_object(const struct lws_client_connect_info *i,
			      struct lws *wsi);

struct lws *
lws_mqtt_client_send_connect(struct lws *wsi);

struct lws *
lws_mqtt_client_send_disconnect(struct lws *wsi);

int
lws_mqtt_fill_fixed_header(uint8_t *p, lws_mqtt_control_packet_t ctrl_pkt_type,
			   uint8_t dup, lws_mqtt_qos_levels_t qos,
			   uint8_t retain);

struct lws *
lws_wsi_mqtt_adopt(struct lws *parent_wsi, struct lws *wsi);

lws_mqtt_subs_t *
lws_mqtt_find_sub(struct _lws_mqtt_related *mqtt, const char *topic);

lws_mqtt_match_topic_return_t
lws_mqtt_is_topic_matched(const char* sub, const char* pub);

#endif /* _PRIVATE_LIB_ROLES_MQTT */

