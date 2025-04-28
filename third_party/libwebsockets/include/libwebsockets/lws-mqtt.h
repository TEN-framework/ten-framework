/*
 * libwebsockets - protocol - mqtt
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
 *
 * included from libwebsockets.h
 */

#ifndef _LWS_MQTT_H
#define _LWS_MQTT_H 1

struct _lws_mqtt_related;
typedef struct _lws_mqtt_related lws_mqtt_related_t;
struct lws_mqtt_str_st;
typedef struct lws_mqtt_str_st lws_mqtt_str_t;

#define MQTT_VER_3_1_1 4

#define LWS_MQTT_FINAL_PART 1

#define LWS_MQTT_MAX_AWSIOT_TOPICLEN  256
#define LWS_MQTT_MAX_TOPICLEN  65535
#define LWS_MQTT_MAX_CIDLEN    128
#define LWS_MQTT_RANDOM_CIDLEN 23 /* 3.1.3.1-5: Server MUST... between
				     1 and 23 chars... */

#define LWS_MQTT_SHADOW_MAX_THING_LEN 128
#define LWS_MQTT_SHADOW_MAX_SHADOW_LEN 64
#define LWS_MQTT_SHADOW_UPDATE_STR "/update"
#define LWS_MQTT_SHADOW_DELETE_STR "/delete"
#define LWS_MQTT_SHADOW_GET_STR "/get"
#define LWS_MQTT_SHADOW_RESP_ACCEPTED_STR  "/accepted"
#define LWS_MQTT_SHADOW_RESP_REJECTED_STR "/rejected"
#define LWS_MQTT_SHADOW_RESP_DELTA_STR "/delta"
#define LWS_MQTT_SHADOW_RESP_DOCUMENT_STR "/documents"
#define LWS_MQTT_SHADOW_UPDATE_ACCEPTED_STR LWS_MQTT_SHADOW_UPDATE_STR LWS_MQTT_SHADOW_RESP_ACCEPTED_STR
#define LWS_MQTT_SHADOW_UPDATE_REJECTED_STR LWS_MQTT_SHADOW_UPDATE_STR LWS_MQTT_SHADOW_RESP_REJECTED_STR
#define LWS_MQTT_SHADOW_UPDATE_DELTA_STR LWS_MQTT_SHADOW_UPDATE_STR LWS_MQTT_SHADOW_RESP_DELTA_STR
#define LWS_MQTT_SHADOW_UPDATE_DOCUMENT_STR LWS_MQTT_SHADOW_UPDATE_STR LWS_MQTT_SHADOW_RESP_DOCUMENT_STR
#define LWS_MQTT_SHADOW_DELETE_ACCEPTED_STR LWS_MQTT_SHADOW_DELETE_STR LWS_MQTT_SHADOW_RESP_ACCEPTED_STR
#define LWS_MQTT_SHADOW_DELETE_REJECTED_STR LWS_MQTT_SHADOW_DELETE_STR LWS_MQTT_SHADOW_RESP_REJECTED_STR
#define LWS_MQTT_SHADOW_GET_ACCEPTED_STR LWS_MQTT_SHADOW_GET_STR LWS_MQTT_SHADOW_RESP_ACCEPTED_STR
#define LWS_MQTT_SHADOW_GET_REJECTED_STR LWS_MQTT_SHADOW_GET_STR LWS_MQTT_SHADOW_RESP_REJECTED_STR
#define LWS_MQTT_SHADOW_PREFIX_FORMAT "$aws/things/%s"
#define LWS_MQTT_SHADOW_NAMED_SHADOW_TOPIC_FORMAT LWS_MQTT_SHADOW_PREFIX_FORMAT "/shadow/name/%s%s"
#define LWS_MQTT_SHADOW_UNNAMED_SHADOW_TOPIC_FORMAT  LWS_MQTT_SHADOW_PREFIX_FORMAT "/shadow%s"
#define LWS_MQTT_SHADOW_UNNAMED_TOPIC_MATCH	"$aws/things/+/shadow/+"
#define LWS_MQTT_SHADOW_NAMED_TOPIC_MATCH	"$aws/things/+/shadow/name/+/+"

typedef enum {
	QOS0,
	QOS1,
	QOS2,				/* not supported */
	RESERVED_QOS_LEVEL,
	FAILURE_QOS_LEVEL = 0x80
} lws_mqtt_qos_levels_t;

typedef union {
	struct {
		uint8_t		retain:1;
		uint8_t 	qos:2;
		uint8_t 	dup:1;
		uint8_t 	ctrl_pkt_type:4;
	} flags;
	uint8_t 		bits;
} lws_mqtt_fixed_hdr_t;

/*
 * MQTT connection parameters, passed into struct
 * lws_client_connect_info to establish a connection using
 * lws_client_connect_via_info().
*/
typedef struct lws_mqtt_client_connect_param_s {
	const char 			*client_id;	/* Client ID */
	uint16_t 			keep_alive;	/* MQTT keep alive
							   interval in
							   seconds */
	uint8_t 			clean_start:1;	/* MQTT clean
							   session */
	uint8_t				client_id_nofree:1;
	/**< do not free the client id */
	uint8_t				username_nofree:1;
	/**< do not free the username */
	uint8_t				password_nofree:1;
	/**< do not free the password */
	struct {
		const char 		*topic;
		const char 		*message;
		lws_mqtt_qos_levels_t	qos;
		uint8_t 		retain;
	} will_param;				/* MQTT LWT
						   parameters */
	struct {
		const char 		*topic;
		const char 		*message;
		lws_mqtt_qos_levels_t	qos;
		uint8_t 		retain;
	} birth_param;				/* MQTT Birth
						   parameters */
	const char 			*username;
	const char 			*password;
	uint8_t				aws_iot;
} lws_mqtt_client_connect_param_t;

/*
 * MQTT publish parameters
*/
typedef struct lws_mqtt_publish_param_s {
	char			*topic;		/* Topic Name */
	uint16_t 		topic_len;
	const void 		*payload;	/* Publish Payload */
	uint32_t 		payload_len;	/* Size of the
						   complete payload */
	uint32_t		payload_pos;	/* where we are in payload */
	lws_mqtt_qos_levels_t 	qos;

	/*--v-Following will be used by LWS-v--*/
	uint16_t 		packet_id;	/* Packet ID for QoS >
						   0 */
	uint8_t 		dup:1;		/* Retried PUBLISH,
						   for QoS > 0 */
	uint8_t			retain:1;	/* Retained message */
} lws_mqtt_publish_param_t;

typedef struct topic_elem {
	const char		*name;		/* Topic Name */
	lws_mqtt_qos_levels_t 	qos;		/* Requested QoS */

	/*--v-Following will be used by LWS-v--*/
	uint8_t 		acked;
} lws_mqtt_topic_elem_t;

/*
 * MQTT publish parameters
*/
typedef struct lws_mqtt_subscribe_param_s {
	uint32_t		num_topics;	/* Number of topics */
	lws_mqtt_topic_elem_t	*topic;		/* Array of topic elements */

	/*--v-Following will be used by LWS-v--*/
	uint16_t		packet_id;
} lws_mqtt_subscribe_param_t;

typedef enum {
	LMQCP_RESERVED,
	LMQCP_CTOS_CONNECT,	/* Connection request */
	LMQCP_STOC_CONNACK,	/* Connection acknowledgment */
	LMQCP_PUBLISH,		/* Publish Message */
	LMQCP_PUBACK,		/* QoS 1:   Publish acknowledgment */
	LMQCP_PUBREC,		/* QoS 2.1: Publish received */
	LMQCP_PUBREL,		/* QoS 2.2: Publish release */
	LMQCP_PUBCOMP,		/* QoS 2.3: Publish complete */
	LMQCP_CTOS_SUBSCRIBE,	/* Subscribe request */
	LMQCP_STOC_SUBACK,	/* Subscribe acknowledgment */
	LMQCP_CTOS_UNSUBSCRIBE, /* Unsubscribe request */
	LMQCP_STOC_UNSUBACK,	/* Unsubscribe acknowledgment */
	LMQCP_CTOS_PINGREQ,	/* PING request */
	LMQCP_STOC_PINGRESP,	/* PONG response */
	LMQCP_DISCONNECT,	/* Disconnect notification */
	LMQCP_AUTH		/* Authentication exchange */
} lws_mqtt_control_packet_t;

/* flags from byte 8 of C_TO_S CONNECT */
typedef enum {
	LMQCFT_USERNAME_NOFREE					= (1 << 10),
	LMQCFT_PASSWORD_NOFREE					= (1 << 9),
	LMQCFT_CLIENT_ID_NOFREE					= (1 << 8),
	/* only the low 8 are standardized and go out in the protocol */
	LMQCFT_USERNAME						= (1 << 7),
	LMQCFT_PASSWORD						= (1 << 6),
	LMQCFT_WILL_RETAIN					= (1 << 5),
	LMQCFT_WILL_QOS						= (1 << 3),
	LMQCFT_WILL_FLAG					= (1 << 2),
	LMQCFT_CLEAN_START					= (1 << 1),
	LMQCFT_RESERVED						= (1 << 0),

	LMQCFT_WILL_QOS_MASK					= (3 << 3),
} lws_mqtt_connect_flags_t;

/* flags for S_TO_C CONNACK */
typedef enum {
	LMQCFT_SESSION_PRESENT					= (1 << 0),
} lws_mqtt_connack_flags_t;

typedef enum {
	LMQCP_REASON_SUCCESS					= 0x00,
	LMQCP_REASON_NORMAL_DISCONNECTION			= 0x00,
	LMQCP_REASON_GRANTED_QOS0				= 0x00,
	LMQCP_REASON_GRANTED_QOS1				= 0x01,
	LMQCP_REASON_GRANTED_QOS2				= 0x02,
	LMQCP_REASON_DISCONNECT_WILL				= 0x04,
	LMQCP_REASON_NO_MATCHING_SUBSCRIBER			= 0x10,
	LMQCP_REASON_NO_SUBSCRIPTION_EXISTED			= 0x11,
	LMQCP_REASON_CONTINUE_AUTHENTICATION			= 0x18,
	LMQCP_REASON_RE_AUTHENTICATE				= 0x19,

	LMQCP_REASON_UNSPECIFIED_ERROR				= 0x80,
	LMQCP_REASON_MALFORMED_PACKET				= 0x81,
	LMQCP_REASON_PROTOCOL_ERROR				= 0x82,
	LMQCP_REASON_IMPLEMENTATION_SPECIFIC_ERROR		= 0x83,

	/* Begin - Error codes for CONNACK */
	LMQCP_REASON_UNSUPPORTED_PROTOCOL			= 0x84,
	LMQCP_REASON_CLIENT_ID_INVALID				= 0x85,
	LMQCP_REASON_BAD_CREDENTIALS				= 0x86,
	LMQCP_REASON_NOT_AUTHORIZED				= 0x87,
	/* End - Error codes for CONNACK */

	LMQCP_REASON_SERVER_UNAVAILABLE				= 0x88,
	LMQCP_REASON_SERVER_BUSY				= 0x89,
	LMQCP_REASON_BANNED					= 0x8a,
	LMQCP_REASON_SERVER_SHUTTING_DOWN			= 0x8b,
	LMQCP_REASON_BAD_AUTHENTICATION_METHOD			= 0x8c,
	LMQCP_REASON_KEEPALIVE_TIMEOUT				= 0x8d,
	LMQCP_REASON_SESSION_TAKEN_OVER				= 0x8e,
	LMQCP_REASON_TOPIC_FILTER_INVALID			= 0x8f,
	LMQCP_REASON_TOPIC_NAME_INVALID				= 0x90,
	LMQCP_REASON_PACKET_ID_IN_USE				= 0x91,
	LMQCP_REASON_PACKET_ID_NOT_FOUND			= 0x92,
	LMQCP_REASON_MAX_RX_EXCEEDED				= 0x93,
	LMQCP_REASON_TOPIC_ALIAS_INVALID			= 0x94,
	LMQCP_REASON_PACKET_TOO_LARGE				= 0x95,
	LMQCP_REASON_RATELIMIT					= 0x96,
	LMQCP_REASON_QUOTA_EXCEEDED				= 0x97,
	LMQCP_REASON_ADMINISTRATIVE_ACTION			= 0x98,
	LMQCP_REASON_PAYLOAD_FORMAT_INVALID			= 0x99,
	LMQCP_REASON_RETAIN_NOT_SUPPORTED			= 0x9a,
	LMQCP_REASON_QOS_NOT_SUPPORTED				= 0x9b,
	LMQCP_REASON_USE_ANOTHER_SERVER				= 0x9c,
	LMQCP_REASON_SERVER_MOVED				= 0x9d,
	LMQCP_REASON_SHARED_SUBSCRIPTIONS_NOT_SUPPORTED		= 0x9e,
	LMQCP_REASON_CONNECTION_RATE_EXCEEDED			= 0x9f,
	LMQCP_REASON_MAXIMUM_CONNECT_TIME			= 0xa0,
	LMQCP_REASON_SUBSCRIPTION_IDS_NOT_SUPPORTED		= 0xa1,
	LMQCP_REASON_WILDCARD_SUBSCRIPTIONS_NOT_SUPPORTED	= 0xa2,
} lws_mqtt_reason_t;

typedef enum {
	LMQPROP_INVALID,
	LMQPROP_PAYLOAD_FORMAT_INDICATOR			= 0x01,
	LMQPROP_MESSAGE_EXPIRY_INTERVAL				= 0x02,
	LMQPROP_CONTENT_TYPE					= 0x03,
	LMQPROP_RESPONSE_TOPIC					= 0x08,
	LMQPROP_CORRELATION_DATA				= 0x09,
	LMQPROP_SUBSCRIPTION_IDENTIFIER				= 0x0b,
	LMQPROP_SESSION_EXPIRY_INTERVAL				= 0x11,
	LMQPROP_ASSIGNED_CLIENT_IDENTIFIER			= 0x12,
	LMQPROP_SERVER_KEEP_ALIVE				= 0x13,
	LMQPROP_AUTHENTICATION_METHOD				= 0x15,
	LMQPROP_AUTHENTICATION_DATA				= 0x16,
	LMQPROP_REQUEST_PROBLEM_INFORMATION			= 0x17,
	LMQPROP_WILL_DELAY_INTERVAL				= 0x18,
	LMQPROP_REQUEST_RESPONSE_INFORMATION			= 0x19,
	LMQPROP_RESPONSE_INFORMATION				= 0x1a,
	LMQPROP_SERVER_REFERENCE				= 0x1c,
	LMQPROP_REASON_STRING					= 0x1f,
	LMQPROP_RECEIVE_MAXIMUM					= 0x21,
	LMQPROP_TOPIC_ALIAS_MAXIMUM				= 0x22,
	LMQPROP_TOPIC_ALIAS					= 0x23,
	LMQPROP_MAXIMUM_QOS					= 0x24,
	LMQPROP_RETAIN_AVAILABLE				= 0x25,
	LMQPROP_USER_PROPERTY					= 0x26,
	LMQPROP_MAXIMUM_PACKET_SIZE				= 0x27,
	LMQPROP_WILDCARD_SUBSCRIPTION_AVAIL			= 0x28,
	LMQPROP_SUBSCRIPTION_IDENTIFIER_AVAIL			= 0x29,
	LMQPROP_SHARED_SUBSCRIPTION_AVAIL			= 0x2a
} lws_mqtt_property;

int
lws_read_mqtt(struct lws *wsi, unsigned char *buf, lws_filepos_t len);

/* returns 0 if bd1 and bd2 are "the same", that includes empty, else nonzero */
LWS_VISIBLE LWS_EXTERN int
lws_mqtt_bindata_cmp(const lws_mqtt_str_t *bd1, const lws_mqtt_str_t *bd2);

LWS_VISIBLE LWS_EXTERN void
lws_mqtt_str_init(lws_mqtt_str_t *s, uint8_t *buf, uint16_t lim, char nf);

LWS_VISIBLE LWS_EXTERN lws_mqtt_str_t *
lws_mqtt_str_create(uint16_t lim);

LWS_VISIBLE LWS_EXTERN lws_mqtt_str_t *
lws_mqtt_str_create_init(uint8_t *buf, uint16_t len, uint16_t lim);

LWS_VISIBLE LWS_EXTERN lws_mqtt_str_t *
lws_mqtt_str_create_cstr_dup(const char *buf, uint16_t lim);

LWS_VISIBLE LWS_EXTERN uint8_t *
lws_mqtt_str_next(lws_mqtt_str_t *s, uint16_t *budget);

LWS_VISIBLE LWS_EXTERN int
lws_mqtt_str_advance(lws_mqtt_str_t *s, int n);

LWS_VISIBLE LWS_EXTERN void
lws_mqtt_str_free(lws_mqtt_str_t **s);


/**
 * lws_mqtt_client_send_publish() - lws_write a publish packet
 *
 * \param wsi: the mqtt child wsi
 * \param pub: additional information on what we're publishing
 * \param buf: payload to send
 * \param len: length of data in buf
 * \param final: flag indicating this is the last part
 *
 * Issues part of, or the whole of, a PUBLISH frame.  The first part of the
 * frame contains the header, and uses the .qos and .payload_len parts of \p pub
 * since MQTT requires the frame to specify the PUBLISH message length at the
 * start.  The \p len paramter may be less than \p pub.payload_len, in which
 * case subsequent calls with more payload are needed to complete the frame.
 *
 * Although the connection is stuck waiting for the remainder, in that it can't
 * issue any other frames until the current one is completed, lws returns to the
 * event loop normally and can continue the calls with additional payload even
 * for huge frames as the data becomes available, consistent with timeout needs
 * and latency to start any new frame (even, eg, related to ping / pong).
 *
 * If you're sending large frames, the OS will typically not allow the data to
 * be sent all at once to kernel side.  So you should ideally cut the payload
 * up into 1 or 2- mtu sized chunks and send that.
 *
 * Final should be set when you're calling with the last part of the payload.
 */
LWS_VISIBLE LWS_EXTERN int
lws_mqtt_client_send_publish(struct lws *wsi, lws_mqtt_publish_param_t *pub,
			     const void *buf, uint32_t len, int final);

/**
 * lws_mqtt_client_send_subcribe() - lws_write a subscribe packet
 *
 * \param wsi: the mqtt child wsi
 * \param sub: which topic(s) we want to subscribe to
 *
 * For topics other child streams have not already subscribed to, send a packet
 * to the server asking to subscribe to them.  If all topics listed are already
 * subscribed to be the shared network connection, just trigger the
 * LWS_CALLBACK_MQTT_SUBSCRIBED callback as if a SUBACK had come.
 *
 * \p sub doesn't need to exist after the return from this function.
 */
LWS_VISIBLE LWS_EXTERN int
lws_mqtt_client_send_subcribe(struct lws *wsi, lws_mqtt_subscribe_param_t *sub);

/**
 * lws_mqtt_client_send_unsubcribe() - lws_write a unsubscribe packet
 *
 * \param wsi: the mqtt child wsi
 * \param sub: which topic(s) we want to unsubscribe from
 *
 * For topics other child streams are not subscribed to, send a packet
 * to the server asking to unsubscribe from them.  If all topics
 * listed are already subscribed by other child streams on the shared
 * network connection, just trigger the LWS_CALLBACK_MQTT_UNSUBSCRIBED
 * callback as if a UNSUBACK had come.
 *
 * \p unsub doesn't need to exist after the return from this function.
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_mqtt_client_send_unsubcribe(struct lws *wsi,
				const lws_mqtt_subscribe_param_t *unsub);

#endif /* _LWS_MQTT_H */
