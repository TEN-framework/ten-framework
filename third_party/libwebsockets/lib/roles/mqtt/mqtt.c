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
 *
 * MQTT v5
 *
 * http://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html
 *
 * Control Packet structure
 *
 *  - Always:           2+ byte:  Fixed Hdr
 *  - Required in some: variable: Variable Hdr + [(CONNECT)Will Props] + Props
 *  - Required in some: variable: Payload
 *
 * For CONNECT, the props if present MUST be in the order [MQTT-3.1.3-1]
 *
 *  - Client Identifier
 *  - Will Properties
 *  - Will Topic
 *  - Will Payload
 *  - User Name
 *  - Password
 */

#include "private-lib-core.h"
#include <string.h>
#include <sys/types.h>
#include <assert.h>

typedef enum {
	LMQPRS_AWAITING_CONNECT,

} lws_mqtt_protocol_server_connstate_t;

const char * const reason_names_g1[] = {
	"Success / Normal disconnection / QoS0",
	"QoS1",
	"QoS2",
	"Disconnect Will",
	"No matching subscriber",
	"No subscription existed",
	"Continue authentication",
	"Re-authenticate"
};

const char * const reason_names_g2[] = {
	"Unspecified error",
	"Malformed packet",
	"Protocol error",
	"Implementation specific error",
	"Unsupported protocol",
	"Client ID invalid",
	"Bad credentials",
	"Not Authorized",
	"Server Unavailable",
	"Server Busy",
	"Banned",
	"Server Shutting Down",
	"Bad Authentication Method",
	"Keepalive Timeout",
	"Session taken over",
	"Topic Filter Invalid",
	"Packet ID in use",
	"Packet ID not found",
	"Max RX Exceeded",
	"Topic Alias Invalid",
	"Packet too large",
	"Ratelimit",
	"Quota Exceeded",
	"Administrative Action",
	"Payload format invalid",
	"Retain not supported",
	"QoS not supported",
	"Use another server",
	"Server Moved",
	"Shared subscriptions not supported",
	"Connection rate exceeded",
	"Maximum Connect Time",
	"Subscription IDs not supported",
	"Wildcard subscriptions not supported"
};

#define LMQCP_WILL_PROPERTIES 0

/* For each property, a bitmap describing which commands it is valid for */

static const uint16_t property_valid[] = {
	[LMQPROP_PAYLOAD_FORMAT_INDICATOR]	= (1 << LMQCP_PUBLISH) |
						  (1 << LMQCP_WILL_PROPERTIES),
	[LMQPROP_MESSAGE_EXPIRY_INTERVAL]	= (1 << LMQCP_PUBLISH) |
						  (1 << LMQCP_WILL_PROPERTIES),
	[LMQPROP_CONTENT_TYPE]			= (1 << LMQCP_PUBLISH) |
						  (1 << LMQCP_WILL_PROPERTIES),
	[LMQPROP_RESPONSE_TOPIC]		= (1 << LMQCP_PUBLISH) |
						  (1 << LMQCP_WILL_PROPERTIES),
	[LMQPROP_CORRELATION_DATA]		= (1 << LMQCP_PUBLISH) |
						  (1 << LMQCP_WILL_PROPERTIES),
	[LMQPROP_SUBSCRIPTION_IDENTIFIER]	= (1 << LMQCP_PUBLISH) |
						  (1 << LMQCP_CTOS_SUBSCRIBE),
	[LMQPROP_SESSION_EXPIRY_INTERVAL]	= (1 << LMQCP_CTOS_CONNECT) |
						  (1 << LMQCP_STOC_CONNACK) |
						  (1 << LMQCP_DISCONNECT),
	[LMQPROP_ASSIGNED_CLIENT_IDENTIFIER]	= (1 << LMQCP_STOC_CONNACK),
	[LMQPROP_SERVER_KEEP_ALIVE]		= (1 << LMQCP_STOC_CONNACK),
	[LMQPROP_AUTHENTICATION_METHOD]		= (1 << LMQCP_CTOS_CONNECT) |
						  (1 << LMQCP_STOC_CONNACK) |
						  (1 << LMQCP_AUTH),
	[LMQPROP_AUTHENTICATION_DATA]		= (1 << LMQCP_CTOS_CONNECT) |
						  (1 << LMQCP_STOC_CONNACK) |
						  (1 << LMQCP_AUTH),
	[LMQPROP_REQUEST_PROBLEM_INFORMATION]	= (1 << LMQCP_CTOS_CONNECT),
	[LMQPROP_WILL_DELAY_INTERVAL]		= (1 << LMQCP_WILL_PROPERTIES),
	[LMQPROP_REQUEST_RESPONSE_INFORMATION]	= (1 << LMQCP_CTOS_CONNECT),
	[LMQPROP_RESPONSE_INFORMATION]		= (1 << LMQCP_STOC_CONNACK),
	[LMQPROP_SERVER_REFERENCE]		= (1 << LMQCP_STOC_CONNACK) |
						  (1 << LMQCP_DISCONNECT),
	[LMQPROP_REASON_STRING]			= (1 << LMQCP_STOC_CONNACK) |
						  (1 << LMQCP_PUBACK) |
						  (1 << LMQCP_PUBREC) |
						  (1 << LMQCP_PUBREL) |
						  (1 << LMQCP_PUBCOMP) |
						  (1 << LMQCP_STOC_SUBACK) |
						  (1 << LMQCP_STOC_UNSUBACK) |
						  (1 << LMQCP_DISCONNECT) |
						  (1 << LMQCP_AUTH),
	[LMQPROP_RECEIVE_MAXIMUM]		= (1 << LMQCP_CTOS_CONNECT) |
						  (1 << LMQCP_STOC_CONNACK),
	[LMQPROP_TOPIC_ALIAS_MAXIMUM]		= (1 << LMQCP_CTOS_CONNECT) |
						  (1 << LMQCP_STOC_CONNACK),
	[LMQPROP_TOPIC_ALIAS]			= (1 << LMQCP_PUBLISH),
	[LMQPROP_MAXIMUM_QOS]			= (1 << LMQCP_STOC_CONNACK),
	[LMQPROP_RETAIN_AVAILABLE]		= (1 << LMQCP_STOC_CONNACK),
	[LMQPROP_USER_PROPERTY]			= (1 << LMQCP_CTOS_CONNECT) |
						  (1 << LMQCP_STOC_CONNACK) |
						  (1 << LMQCP_PUBLISH) |
						  (1 << LMQCP_WILL_PROPERTIES) |
						  (1 << LMQCP_PUBACK) |
						  (1 << LMQCP_PUBREC) |
						  (1 << LMQCP_PUBREL) |
						  (1 << LMQCP_PUBCOMP) |
						  (1 << LMQCP_CTOS_SUBSCRIBE) |
						  (1 << LMQCP_STOC_SUBACK) |
						  (1 << LMQCP_CTOS_UNSUBSCRIBE) |
						  (1 << LMQCP_STOC_UNSUBACK) |
						  (1 << LMQCP_DISCONNECT) |
						  (1 << LMQCP_AUTH),
	[LMQPROP_MAXIMUM_PACKET_SIZE]		= (1 << LMQCP_CTOS_CONNECT) |
						  (1 << LMQCP_STOC_CONNACK),
	[LMQPROP_WILDCARD_SUBSCRIPTION_AVAIL]	= (1 << LMQCP_STOC_CONNACK),
	[LMQPROP_SUBSCRIPTION_IDENTIFIER_AVAIL]	= (1 << LMQCP_STOC_CONNACK),
	[LMQPROP_SHARED_SUBSCRIPTION_AVAIL]	= (1 << LMQCP_STOC_CONNACK)
};


/*
 * For each command index, maps flags, id, qos and payload legality
 * notice in most cases PUBLISH requires further processing
 */
static const uint8_t map_flags[] = {
	[LMQCP_RESERVED]		= 0x00,
	[LMQCP_CTOS_CONNECT]		= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PAYLOAD |
					  LMQCP_LUT_FLAG_PACKET_ID_NONE | 0x00,
	[LMQCP_STOC_CONNACK]		= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PACKET_ID_NONE | 0x00,
	[LMQCP_PUBLISH]			= LMQCP_LUT_FLAG_PAYLOAD | /* option */
					  LMQCP_LUT_FLAG_PACKET_ID_QOS12 | 0x00,
	[LMQCP_PUBACK]			= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PACKET_ID_HAS | 0x00,
	[LMQCP_PUBREC]			= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PACKET_ID_HAS | 0x00,
	[LMQCP_PUBREL]			= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PACKET_ID_HAS | 0x02,
	[LMQCP_PUBCOMP]			= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PACKET_ID_HAS | 0x00,
	[LMQCP_CTOS_SUBSCRIBE]		= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PAYLOAD |
					  LMQCP_LUT_FLAG_PACKET_ID_HAS | 0x02,
	[LMQCP_STOC_SUBACK]		= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PAYLOAD |
					  LMQCP_LUT_FLAG_PACKET_ID_HAS | 0x00,
	[LMQCP_CTOS_UNSUBSCRIBE]	= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PAYLOAD |
					  LMQCP_LUT_FLAG_PACKET_ID_HAS | 0x02,
	[LMQCP_STOC_UNSUBACK]		= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PAYLOAD |
					  LMQCP_LUT_FLAG_PACKET_ID_NONE | 0x00,
	[LMQCP_CTOS_PINGREQ]		= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PACKET_ID_NONE | 0x00,
	[LMQCP_STOC_PINGRESP]		= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PACKET_ID_NONE | 0x00,
	[LMQCP_DISCONNECT]		= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PACKET_ID_NONE | 0x00,
	[LMQCP_AUTH]			= LMQCP_LUT_FLAG_RESERVED_FLAGS |
					  LMQCP_LUT_FLAG_PACKET_ID_NONE | 0x00,
};

static int
lws_mqtt_pconsume(lws_mqtt_parser_t *par, int consumed)
{
	par->consumed += (unsigned int)consumed;

	if (par->consumed > par->props_len)
		return -1;

	/* more properties coming */

	if (par->consumed < par->props_len) {
		par->state = LMQCPP_PROP_ID_VBI;
		return 0;
	}

	/* properties finished: are we headed for payload or idle? */

	if ((map_flags[ctl_pkt_type(par)] & LMQCP_LUT_FLAG_PAYLOAD) &&
		/* A PUBLISH packet MUST NOT contain a Packet Identifier if
		 * its QoS value is set to 0 [MQTT-2.2.1-2]. */
	    (ctl_pkt_type(par) != LMQCP_PUBLISH ||
	     (par->packet_type_flags & 6))) {
		par->state = LMQCPP_PAYLOAD;
		return 0;
	}

	par->state = LMQCPP_IDLE;

	return 0;
}

static int
lws_mqtt_set_client_established(struct lws *wsi)
{
	lws_role_transition(wsi, LWSIFR_CLIENT, LRS_ESTABLISHED,
			    &role_ops_mqtt);

	if (user_callback_handle_rxflow(wsi->a.protocol->callback,
					wsi, LWS_CALLBACK_MQTT_CLIENT_ESTABLISHED,
					wsi->user_space, NULL, 0) < 0) {
		lwsl_err("%s: MQTT_ESTABLISHED failed\n", __func__);

		return -1;
	}
	/*
	 * If we made a new connection and got the ACK, our connection is
	 * definitely working in both directions at the moment
	 */
	lws_validity_confirmed(wsi);

	/* clear connection timeout */
	lws_set_timeout(wsi, NO_PENDING_TIMEOUT, 0);

	return 0;
}

static lws_mqtt_validate_topic_return_t
lws_mqtt_validate_topic(const char *topic, size_t topiclen, uint8_t awsiot)
{
	size_t spos = 0;
	const char *sub = topic;
	int8_t slashes = 0;
	lws_mqtt_validate_topic_return_t ret = LMVTR_VALID;

	if (awsiot) {
		if (topiclen > LWS_MQTT_MAX_AWSIOT_TOPICLEN)
			return LMVTR_FAILED_OVERSIZE;
		if (topic[0] == '$') {
			ret = LMVTR_VALID_SHADOW;
			slashes = -3;
		}
	} else {
		if (topiclen > LWS_MQTT_MAX_TOPICLEN)
			return LMVTR_FAILED_OVERSIZE;
		if (topic[0] == '$')
			return LMVTR_FAILED_WILDCARD_FORMAT;
	}

	while (*sub != 0) {
		if (sub[0] == '+') {
			/* topic == "+foo" || "a/+foo" ? */
			if (spos > 0 && sub[-1] != '/')
				return LMVTR_FAILED_WILDCARD_FORMAT;

			/* topic == "foo+" or "foo+/a" ? */
			if (sub[1] != 0 && sub[1] != '/')
				return LMVTR_FAILED_WILDCARD_FORMAT;

			ret = LMVTR_VALID_WILDCARD;
		} else if (sub[0] == '#') {
			/* topic == "foo#" ? */
			if (spos > 0 && sub[-1] != '/')
				return LMVTR_FAILED_WILDCARD_FORMAT;

			/* topic == "#foo" ? */
			if (sub[1] != 0)
				return LMVTR_FAILED_WILDCARD_FORMAT;

			ret = LMVTR_VALID_WILDCARD;
		} else if (sub[0] == '/') {
			slashes++;
		}
		spos++;
		sub++;
	}

	if (awsiot && (slashes < 0 || slashes > 7))
		return LMVTR_FAILED_SHADOW_FORMAT;

	return ret;
}

static lws_mqtt_subs_t *
lws_mqtt_create_sub(struct _lws_mqtt_related *mqtt, const char *topic)
{
	lws_mqtt_subs_t *mysub;
	size_t topiclen = strlen(topic);
	lws_mqtt_validate_topic_return_t flag;

	flag = lws_mqtt_validate_topic(topic, topiclen, mqtt->client.aws_iot);
	switch (flag) {
	case LMVTR_FAILED_OVERSIZE:
		lwsl_err("%s: Topic is too long\n",
			 __func__);
		return NULL;
	case LMVTR_FAILED_SHADOW_FORMAT:
	case LMVTR_FAILED_WILDCARD_FORMAT:
		lwsl_err("%s: Invalid topic format \"%s\"\n",
			 __func__, topic);
		return NULL;

	case LMVTR_VALID:
	case LMVTR_VALID_WILDCARD:
	case LMVTR_VALID_SHADOW:
		mysub = lws_malloc(sizeof(*mysub) + topiclen + 1, "sub");
		if (!mysub) {
			lwsl_err("%s: Error allocating mysub\n",
				 __func__);
			return NULL;
		}
		mysub->wildcard = (flag == LMVTR_VALID_WILDCARD);
		mysub->shadow = (flag == LMVTR_VALID_SHADOW);
		break;

	default:
		lwsl_err("%s: Unknown flag - %d\n",
			 __func__, flag);
		return NULL;
	}

	mysub->next = mqtt->subs_head;
	mqtt->subs_head = mysub;
	memcpy(mysub->topic, topic, strlen(topic) + 1);
	mysub->ref_count = 1;

	lwsl_info("%s: Created mysub %p for wsi->mqtt %p\n",
		  __func__, mysub, mqtt);

	return mysub;
}

static int
lws_mqtt_client_remove_subs(struct _lws_mqtt_related *mqtt)
{
	lws_mqtt_subs_t *s = mqtt->subs_head;
	lws_mqtt_subs_t *temp = NULL;


	lwsl_info("%s: Called to remove subs from wsi->mqtt %p\n",
		  __func__, mqtt);

	while (s && s->next) {
		if (s->next->ref_count == 0)
			break;
		s = s->next;
	}

	if (s && s->next) {
		temp = s->next;
		lwsl_info("%s: Removing sub %p from wsi->mqtt %p\n",
			  __func__, temp, mqtt);
		s->next = temp->next;
		lws_free(temp);
		return 0;
	}
	return 1;
}

/*
 * This fires if the wsi did a PUBLISH under QoS1 or QoS2, but no PUBACK or
 * PUBREC came before the timeout period
 */

static void
lws_mqtt_publish_resend(struct lws_sorted_usec_list *sul)
{
	struct _lws_mqtt_related *mqtt = lws_container_of(sul,
			struct _lws_mqtt_related, sul_qos_puback_pubrec_wait);

	lwsl_notice("%s: %s\n", __func__, lws_wsi_tag(mqtt->wsi));

	if (mqtt->wsi->a.protocol->callback(mqtt->wsi, LWS_CALLBACK_MQTT_RESEND,
				mqtt->wsi->user_space, NULL, 0))
		lws_set_timeout(mqtt->wsi, 1, LWS_TO_KILL_ASYNC);
}

static void
lws_mqtt_unsuback_timeout(struct lws_sorted_usec_list *sul)
{
	struct _lws_mqtt_related *mqtt = lws_container_of(sul,
			struct _lws_mqtt_related, sul_unsuback_wait);

	lwsl_debug("%s: %s\n", __func__, lws_wsi_tag(mqtt->wsi));

	if (mqtt->wsi->a.protocol->callback(mqtt->wsi,
					   LWS_CALLBACK_MQTT_UNSUBSCRIBE_TIMEOUT,
					   mqtt->wsi->user_space, NULL, 0))
		lws_set_timeout(mqtt->wsi, 1, LWS_TO_KILL_ASYNC);
}

static void
lws_mqtt_shadow_timeout(struct lws_sorted_usec_list *sul)
{
	struct _lws_mqtt_related *mqtt = lws_container_of(sul,
			struct _lws_mqtt_related, sul_shadow_wait);

	lwsl_debug("%s: %s\n", __func__, lws_wsi_tag(mqtt->wsi));

	if (mqtt->wsi->a.protocol->callback(mqtt->wsi,
					    LWS_CALLBACK_MQTT_SHADOW_TIMEOUT,
					    mqtt->wsi->user_space, NULL, 0))
		lws_set_timeout(mqtt->wsi, 1, LWS_TO_KILL_ASYNC);
}

void
lws_mqttc_state_transition(lws_mqttc_t *c, lwsgs_mqtt_states_t s)
{
	lwsl_debug("%s: ep %p: state %d -> %d\n", __func__, c, c->estate, s);
	c->estate = s;
}

lws_mqtt_match_topic_return_t
lws_mqtt_is_topic_matched(const char* sub, const char* pub)
{
	const char *ppos = pub, *spos = sub;

	if (!ppos || !spos) {
		return LMMTR_TOPIC_MATCH_ERROR;
	}

	while (*spos) {
		if (*ppos == '#' || *ppos == '+') {
			lwsl_err("%s: PUBLISH to wildcard "
				 "topic \"%s\" not supported\n",
				 __func__, pub);
			return LMMTR_TOPIC_MATCH_ERROR;
		}
		/* foo/+/bar == foo/xyz/bar ? */
		if (*spos == '+') {
			/* Skip ahead */
			while (*ppos != '\0' && *ppos != '/') {
				ppos++;
			}
		} else if (*spos == '#') {
			return LMMTR_TOPIC_MATCH;
		} else {
			if (*ppos == '\0') {
				/* foo/bar == foo/bar/# ? */
				if (!strncmp(spos, "/#", 2))
					return LMMTR_TOPIC_MATCH;
				return LMMTR_TOPIC_NOMATCH;
			/* Non-matching character */
			} else if (*ppos != *spos) {
				return LMMTR_TOPIC_NOMATCH;
			}
			ppos++;
		}
		spos++;
	}

	if (*spos == '\0' && *ppos == '\0')
		return LMMTR_TOPIC_MATCH;

	return LMMTR_TOPIC_NOMATCH;
}

lws_mqtt_subs_t* lws_mqtt_find_sub(struct _lws_mqtt_related* mqtt,
				   const char* ptopic) {
	lws_mqtt_subs_t *s = mqtt->subs_head;

	while (s) {
		/*  SUB topic  ==   PUB topic  ? */
		/* foo/bar/xyz ==  foo/bar/xyz ? */
		if (!s->wildcard) {
			if (!strcmp((const char*)s->topic, ptopic))
				return s;
		} else {
			if (lws_mqtt_is_topic_matched(
			    s->topic, ptopic) == LMMTR_TOPIC_MATCH)
				return s;
		}

		s = s->next;
	}

	return NULL;
}

int
_lws_mqtt_rx_parser(struct lws *wsi, lws_mqtt_parser_t *par,
		    const uint8_t *buf, size_t len)
{
	struct lws *w;
	int n;

	if (par->flag_pending_send_reason_close)
		return 0;

	/*
	 * Stateful, fragmentation-immune parser
	 *
	 * Notice that len can always be 1 if under attack, even over tls if
	 * the server is compromised or malicious.
	 */

	while (len) {
		lwsl_debug("%s: %d, len = %d\n", __func__, par->state, (int)len);
		switch (par->state) {
		case LMQCPP_IDLE:
			par->packet_type_flags = *buf++;
			len--;

#if defined(LWS_WITH_CLIENT)
			/*
			 * The case where we sent the connect, but we received
			 * something else before any CONNACK
			 */
			if (lwsi_state(wsi) == LRS_MQTTC_AWAIT_CONNACK &&
			    par->packet_type_flags >> 4 != LMQCP_STOC_CONNACK) {
				lwsl_notice("%s: server sent non-CONNACK\n",
						__func__);
				goto send_protocol_error_and_close;
			}
#endif /* LWS_WITH_CLIENT */

			n = map_flags[par->packet_type_flags >> 4];
			/*
			 *  Where a flag bit is marked as “Reserved”, it is
			 *  reserved for future use and MUST be set to the value
			 *  listed [MQTT-2.1.3-1].
			 */
			if ((n & LMQCP_LUT_FLAG_RESERVED_FLAGS) &&
			    ((par->packet_type_flags & 0x0f) != (n & 0x0f))) {
				lwsl_notice("%s: %s: bad flags, 0x%02x mask 0x%02x (len %d)\n",
					    __func__, lws_wsi_tag(wsi),
					    par->packet_type_flags, n, (int)len + 1);
				lwsl_hexdump_err(buf - 1, len + 1);
				goto send_protocol_error_and_close;
			}

			lwsl_debug("%s: received pkt type 0x%x / flags 0x%x\n",
				   __func__, par->packet_type_flags >> 4,
				   par->packet_type_flags & 0xf);

			/* allows us to know if a property that can only be
			 * given once, appears twice */
			memset(par->props_seen, 0, sizeof(par->props_seen));
			par->state = par->packet_type_flags & 0xf0;
			break;

		case LMQCPP_CONNECT_PACKET:
			lwsl_debug("%s: received CONNECT pkt\n", __func__);
			par->state = LMQCPP_CONNECT_REMAINING_LEN_VBI;
			lws_mqtt_vbi_init(&par->vbit);
			break;

		case LMQCPP_CONNECT_REMAINING_LEN_VBI:
			switch (lws_mqtt_vbi_r(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				par->cpkt_remlen = par->vbit.value;
				n = map_flags[ctl_pkt_type(par)];
				lws_mqtt_str_init(&par->s_temp, par->temp,
						  sizeof(par->temp), 0);
				par->state = LMQCPP_CONNECT_VH_PNAME;
				break;
			default:
				lwsl_notice("%s: bad vbi\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_CONNECT_VH_PNAME:
			switch (lws_mqtt_str_parse(&par->s_temp, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				if (par->s_temp.len != 4 ||
				    memcmp(par->s_temp.buf, "MQTT",
					   par->s_temp.len)) {
					lwsl_notice("%s: protocol name: %.*s\n",
						  __func__, par->s_temp.len,
						  par->s_temp.buf);
					goto send_unsupp_connack_and_close;
				}
				par->state = LMQCPP_CONNECT_VH_PVERSION;
				break;
			default:
				lwsl_notice("%s: bad protocol name\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_CONNECT_VH_PVERSION:
			par->conn_protocol_version = *buf++;
			len--;
			if (par->conn_protocol_version != 5) {
				lwsl_info("%s: unsupported MQTT version %d\n",
					  __func__, par->conn_protocol_version);
				goto send_unsupp_connack_and_close;
			}
			par->state = LMQCPP_CONNECT_VH_FLAGS;
			break;

		case LMQCPP_CONNECT_VH_FLAGS:
			par->cpkt_flags = *buf++;
			len--;
			if (par->cpkt_flags & 1) {
				/*
				 * The Server MUST validate that the reserved
				 * flag in the CONNECT packet is set to 0
				 * [MQTT-3.1.2-3].
				 */
				par->reason = LMQCP_REASON_MALFORMED_PACKET;
				goto send_reason_and_close;
			}
			/*
			 * conn_flags specifies the Will Properties that should
			 * appear in the payload section
			 */
			lws_mqtt_2byte_init(&par->vbit);
			par->state = LMQCPP_CONNECT_VH_KEEPALIVE;
			break;

		case LMQCPP_CONNECT_VH_KEEPALIVE:
			switch (lws_mqtt_vbi_r(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				par->keepalive = (uint16_t)par->vbit.value;
				lws_mqtt_vbi_init(&par->vbit);
				par->state = LMQCPP_CONNECT_VH_PROPERTIES_VBI_LEN;
				break;
			default:
				lwsl_notice("%s: ka bad vbi\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_PINGRESP_ZERO:
			len--;
			/* second byte of PINGRESP must be zero */
			if (*buf++)
				goto send_protocol_error_and_close;
			goto cmd_completion;

		case LMQCPP_CONNECT_VH_PROPERTIES_VBI_LEN:
			switch (lws_mqtt_vbi_r(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				/* reset consumption counter */
				par->consumed = 0;
				par->props_len = par->vbit.value;
				lws_mqtt_vbi_init(&par->vbit);
				par->state = LMQCPP_PROP_ID_VBI;
				break;
			default:
				lwsl_notice("%s: connpr bad vbi\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		/* PUBREC */
		case LMQCPP_PUBREC_PACKET:
			lwsl_debug("%s: received PUBREC pkt\n", __func__);
			lws_mqtt_vbi_init(&par->vbit);
			switch (lws_mqtt_vbi_r(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				par->cpkt_remlen = par->vbit.value;
				lwsl_debug("%s: PUBREC pkt len = %d\n",
					   __func__, (int)par->cpkt_remlen);
				if (par->cpkt_remlen < 2)
					goto send_protocol_error_and_close;
				par->state = LMQCPP_PUBREC_VH_PKT_ID;
				break;
			default:
				lwsl_notice("%s: pubrec bad vbi\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_PUBREC_VH_PKT_ID:
			if (len < 2) {
				lwsl_notice("%s: len breakage 3\n", __func__);
				return -1;
			}

			par->cpkt_id = lws_ser_ru16be(buf);
			wsi->mqtt->ack_pkt_id = par->cpkt_id;
			buf += 2;
			len -= 2;
			par->cpkt_remlen -= 2;
			par->n = 0;

			goto cmd_completion;

		/* PUBREL */
		case LMQCPP_PUBREL_PACKET:
			lwsl_debug("%s: received PUBREL pkt\n", __func__);
			lws_mqtt_vbi_init(&par->vbit);
			switch (lws_mqtt_vbi_r(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				par->cpkt_remlen = par->vbit.value;
				lwsl_debug("%s: PUBREL pkt len = %d\n",
					   __func__, (int)par->cpkt_remlen);
				if (par->cpkt_remlen < 2)
					goto send_protocol_error_and_close;
				par->state = LMQCPP_PUBREL_VH_PKT_ID;
				break;
			default:
				lwsl_err("%s: pubrel bad vbi\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_PUBREL_VH_PKT_ID:
			if (len < 2) {
				lwsl_notice("%s: len breakage 3\n", __func__);
				return -1;
			}

			par->cpkt_id = lws_ser_ru16be(buf);
			wsi->mqtt->ack_pkt_id = par->cpkt_id;
			buf += 2;
			len -= 2;
			par->cpkt_remlen -= 2;
			par->n = 0;

			goto cmd_completion;

		/* PUBCOMP */
		case LMQCPP_PUBCOMP_PACKET:
			lwsl_debug("%s: received PUBCOMP pkt\n", __func__);
			lws_mqtt_vbi_init(&par->vbit);
			switch (lws_mqtt_vbi_r(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				par->cpkt_remlen = par->vbit.value;
				lwsl_debug("%s: PUBCOMP pkt len = %d\n",
					   __func__, (int)par->cpkt_remlen);
				if (par->cpkt_remlen < 2)
					goto send_protocol_error_and_close;
				par->state = LMQCPP_PUBCOMP_VH_PKT_ID;
				break;
			default:
				lwsl_err("%s: pubcmp bad vbi\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_PUBCOMP_VH_PKT_ID:
			if (len < 2) {
				lwsl_notice("%s: len breakage 3\n", __func__);
				return -1;
			}

			par->cpkt_id = lws_ser_ru16be(buf);
			wsi->mqtt->ack_pkt_id = par->cpkt_id;
			buf += 2;
			len -= 2;
			par->cpkt_remlen -= 2;
			par->n = 0;

			goto cmd_completion;

		case LMQCPP_PUBLISH_PACKET:
			if (lwsi_role_client(wsi) && wsi->mqtt->inside_subscribe) {
				lwsl_notice("%s: Topic rx before subscribing\n",
					    __func__);
				goto send_protocol_error_and_close;
			}
			lwsl_info("%s: received PUBLISH pkt\n", __func__);
			par->state = LMQCPP_PUBLISH_REMAINING_LEN_VBI;
			lws_mqtt_vbi_init(&par->vbit);
			break;
		case LMQCPP_PUBLISH_REMAINING_LEN_VBI:
			switch (lws_mqtt_vbi_r(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				par->cpkt_remlen = par->vbit.value;
				lwsl_debug("%s: PUBLISH pkt len = %d\n",
					   __func__, (int)par->cpkt_remlen);
				/* Move on to PUBLISH's variable header */
				par->state = LMQCPP_PUBLISH_VH_TOPIC;
				break;
			default:
				lwsl_notice("%s: pubrem bad vbi\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_PUBLISH_VH_TOPIC:
		{
			lws_mqtt_publish_param_t *pub = NULL;

			if (len < 2) {
				lwsl_notice("%s: topic too short\n", __func__);
				return -1;
			}

			/* Topic len */
			par->n = lws_ser_ru16be(buf);
			buf += 2;
			len -= 2;

			if (len < par->n) {/* the way this is written... */
				lwsl_notice("%s: len breakage\n", __func__);
				return -1;
			}

			/* Invalid topic len */
			if (par->n == 0) {
				lwsl_notice("%s: zero topic len\n", __func__);
				par->reason = LMQCP_REASON_MALFORMED_PACKET;
				goto send_reason_and_close;
			}
			lwsl_debug("%s: PUBLISH topic len %d\n",
				   __func__, (int)par->n);
			assert(!wsi->mqtt->rx_cpkt_param);
			wsi->mqtt->rx_cpkt_param = lws_zalloc(
				sizeof(lws_mqtt_publish_param_t), "rx pub param");
			if (!wsi->mqtt->rx_cpkt_param)
				goto oom;
			pub = (lws_mqtt_publish_param_t *)wsi->mqtt->rx_cpkt_param;

			pub->topic_len = (uint16_t)par->n;

			/* Topic Name */
			pub->topic = (char *)lws_zalloc((size_t)pub->topic_len + 1,
							"rx publish topic");
			if (!pub->topic)
				goto oom;
			lws_strncpy(pub->topic, (const char *)buf,
				    (size_t)pub->topic_len + 1);
			buf += pub->topic_len;
			len -= pub->topic_len;

			/* Extract QoS Level from Fixed Header Flags */
			pub->qos = (lws_mqtt_qos_levels_t)
					((par->packet_type_flags >> 1) & 0x3);

			pub->payload_pos = 0;

			pub->payload_len = par->cpkt_remlen -
				(unsigned int)(2 + pub->topic_len + ((pub->qos) ? 2 : 0));

			switch (pub->qos) {
			case QOS0:
				par->state = LMQCPP_PAYLOAD;
				if (pub->payload_len == 0)
					goto cmd_completion;

				break;
			case QOS1:
			case QOS2:
				par->state = LMQCPP_PUBLISH_VH_PKT_ID;
				break;
			default:
				par->reason = LMQCP_REASON_MALFORMED_PACKET;
				lws_free_set_NULL(pub->topic);
				lws_free_set_NULL(wsi->mqtt->rx_cpkt_param);
				goto send_reason_and_close;
			}
			break;
		}
		case LMQCPP_PUBLISH_VH_PKT_ID:
		{
			lws_mqtt_publish_param_t *pub =
				(lws_mqtt_publish_param_t *)wsi->mqtt->rx_cpkt_param;

			if (len < 2) {
				lwsl_notice("%s: len breakage 2\n", __func__);
				return -1;
			}

			par->cpkt_id = lws_ser_ru16be(buf);
			buf += 2;
			len -= 2;
			wsi->mqtt->peer_ack_pkt_id = par->cpkt_id;
			lwsl_debug("%s: Packet ID %d\n",
					__func__, (int)par->cpkt_id);
			par->state = LMQCPP_PAYLOAD;
			pub->payload_pos = 0;
			pub->payload_len = par->cpkt_remlen -
				(unsigned int)(2 + pub->topic_len + ((pub->qos) ? 2 : 0));
			if (pub->payload_len == 0)
				goto cmd_completion;

			break;
		}
		case LMQCPP_PAYLOAD:
		{
			lws_mqtt_publish_param_t *pub =
				(lws_mqtt_publish_param_t *)wsi->mqtt->rx_cpkt_param;
			if (pub == NULL) {
				lwsl_err("%s: Uninitialized pub_param\n",
						__func__);
				goto send_protocol_error_and_close;
			}

			pub->payload = buf;
			goto cmd_completion;
		}

		case LMQCPP_CONNACK_PACKET:
			if (!lwsi_role_client(wsi)) {
				lwsl_err("%s: CONNACK is only Server to Client",
						__func__);
				goto send_unsupp_connack_and_close;
			}

			lwsl_debug("%s: received CONNACK pkt\n", __func__);
			lws_mqtt_vbi_init(&par->vbit);
			switch (lws_mqtt_vbi_r(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				par->cpkt_remlen = par->vbit.value;
				lwsl_debug("%s: CONNACK pkt len = %d\n",
					   __func__, (int)par->cpkt_remlen);
				if (par->cpkt_remlen != 2)
					goto send_protocol_error_and_close;

				par->state = LMQCPP_CONNACK_VH_FLAGS;
				break;
			default:
				lwsl_notice("%s: connack bad vbi\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_CONNACK_VH_FLAGS:
		{
			lws_mqttc_t *c = &wsi->mqtt->client;
			par->cpkt_flags = *buf++;
			len--;

			if (par->cpkt_flags & ~LMQCFT_SESSION_PRESENT) {
				/*
				 * Byte 1 is the "Connect Acknowledge
				 * Flags". Bits 7-1 are reserved and
				 * MUST be set to 0.
				 */
				par->reason = LMQCP_REASON_MALFORMED_PACKET;
				goto send_reason_and_close;
			}
			/*
			 * If the Server accepts a connection with
			 * CleanSession set to 1, the Server MUST set
			 * Session Present to 0 in the CONNACK packet
			 * in addition to setting a zero return code
			 * in the CONNACK packet [MQTT-3.2.2-1]. If
			 * the Server accepts a connection with
			 * CleanSession set to 0, the value set in
			 * Session Present depends on whether the
			 * Server already has stored Session state for
			 * the supplied client ID. If the Server has
			 * stored Session state, it MUST set
			 * SessionPresent to 1 in the CONNACK packet
			 * [MQTT-3.2.2-2]. If the Server does not have
			 * stored Session state, it MUST set Session
			 * Present to 0 in the CONNACK packet. This is
			 * in addition to setting a zero return code
			 * in the CONNACK packet [MQTT-3.2.2-3].
			 */
			if ((c->conn_flags & LMQCFT_CLEAN_START) &&
			    (par->cpkt_flags & LMQCFT_SESSION_PRESENT))
				goto send_protocol_error_and_close;

			wsi->mqtt->session_resumed = ((unsigned int)par->cpkt_flags &
						      LMQCFT_SESSION_PRESENT);

			/* Move on to Connect Return Code */
			par->state = LMQCPP_CONNACK_VH_RETURN_CODE;
			break;
		}
		case LMQCPP_CONNACK_VH_RETURN_CODE:
			par->conn_rc = *buf++;
			len--;
			/*
			 * If a server sends a CONNACK packet containing a
			 * non-zero return code it MUST then close the Network
			 * Connection [MQTT-3.2.2-5]
			 */
			switch (par->conn_rc) {
			case 0:
				goto cmd_completion;
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
				par->reason = LMQCP_REASON_UNSUPPORTED_PROTOCOL +
						par->conn_rc - 1;
				goto send_reason_and_close;
			default:
				lwsl_notice("%s: bad connack retcode\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		/* SUBACK */
		case LMQCPP_SUBACK_PACKET:
			if (!lwsi_role_client(wsi)) {
				lwsl_err("%s: SUBACK is only Server to Client",
						__func__);
				goto send_unsupp_connack_and_close;
			}

			lwsl_debug("%s: received SUBACK pkt\n", __func__);
			lws_mqtt_vbi_init(&par->vbit);
			switch (lws_mqtt_vbi_r(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				par->cpkt_remlen = par->vbit.value;
				lwsl_debug("%s: SUBACK pkt len = %d\n",
					   __func__, (int)par->cpkt_remlen);
				if (par->cpkt_remlen <= 2)
					goto send_protocol_error_and_close;
				par->state = LMQCPP_SUBACK_VH_PKT_ID;
				break;
			default:
				lwsl_notice("%s: suback bad vbi\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_SUBACK_VH_PKT_ID:

			if (len < 2) {
				lwsl_notice("%s: len breakage 4\n", __func__);
				return -1;
			}

			par->cpkt_id = lws_ser_ru16be(buf);
			wsi->mqtt->ack_pkt_id = par->cpkt_id;
			buf += 2;
			len -= 2;
			par->cpkt_remlen -= 2;
			par->n = 0;
			par->state = LMQCPP_SUBACK_PAYLOAD;
			*par->temp = 0;
			break;

		case LMQCPP_SUBACK_PAYLOAD:
		{
			lws_mqtt_qos_levels_t qos = (lws_mqtt_qos_levels_t)*buf++;

			len--;
			switch (qos) {
				case QOS0:
				case QOS1:
				case QOS2:
					break;
				case FAILURE_QOS_LEVEL:
					goto send_protocol_error_and_close;

				default:
					par->reason = LMQCP_REASON_MALFORMED_PACKET;
					goto send_reason_and_close;
			}

			if (++(par->n) == par->cpkt_remlen) {
				par->n = 0;
				goto cmd_completion;
			}

			break;
		}

		/* UNSUBACK */
		case LMQCPP_UNSUBACK_PACKET:
			if (!lwsi_role_client(wsi)) {
				lwsl_err("%s: UNSUBACK is only Server to Client",
						__func__);
				goto send_unsupp_connack_and_close;
			}

			lwsl_debug("%s: received UNSUBACK pkt\n", __func__);
			lws_mqtt_vbi_init(&par->vbit);
			switch (lws_mqtt_vbi_r(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				par->cpkt_remlen = par->vbit.value;
				lwsl_debug("%s: UNSUBACK pkt len = %d\n",
					   __func__, (int)par->cpkt_remlen);
				if (par->cpkt_remlen < 2)
					goto send_protocol_error_and_close;
				par->state = LMQCPP_UNSUBACK_VH_PKT_ID;
				break;
			default:
				lwsl_notice("%s: unsuback bad vbi\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_UNSUBACK_VH_PKT_ID:

			if (len < 2) {
				lwsl_notice("%s: len breakage 3\n", __func__);
				return -1;
			}

			par->cpkt_id = lws_ser_ru16be(buf);
			wsi->mqtt->ack_pkt_id = par->cpkt_id;
			buf += 2;
			len -= 2;
			par->cpkt_remlen -= 2;
			par->n = 0;

			goto cmd_completion;

		case LMQCPP_PUBACK_PACKET:
			lws_mqtt_vbi_init(&par->vbit);
			switch (lws_mqtt_vbi_r(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				par->cpkt_remlen = par->vbit.value;
				lwsl_info("%s: PUBACK pkt len = %d\n", __func__,
					  (int)par->cpkt_remlen);
				/*
				 * must be 4 or more, with special case that 2
				 * means success with no reason code or props
				 */
				if (par->cpkt_remlen <= 1 ||
				    par->cpkt_remlen == 3)
					goto send_protocol_error_and_close;

				par->state = LMQCPP_PUBACK_VH_PKT_ID;
				par->fixed_seen[2] = par->fixed_seen[3] = 0;
				par->fixed = 0;
				par->n = 0;
				break;
			default:
				lwsl_notice("%s: puback bad vbi\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_PUBACK_VH_PKT_ID:
			/*
			 * There are 3 fixed bytes and then a VBI for the
			 * property section length
			 */
			par->fixed_seen[par->fixed++] = *buf++;
			if (len < par->cpkt_remlen - par->n) {
				lwsl_notice("%s: len breakage 4\n", __func__);
				return -1;
			}
			len--;
			par->n++;
			if (par->fixed == 2)
				par->cpkt_id = lws_ser_ru16be(par->fixed_seen);

			if (par->fixed == 3) {
				lws_mqtt_vbi_init(&par->vbit);
				par->props_consumed = 0;
				par->state = LMQCPP_PUBACK_PROPERTIES_LEN_VBI;
			}
			/* length of 2 is truncated packet and we completed it */
			if (par->cpkt_remlen == par->fixed)
				goto cmd_completion;
			break;

		case LMQCPP_PUBACK_PROPERTIES_LEN_VBI:
			switch (lws_mqtt_vbi_r(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				par->props_len = par->vbit.value;
				lwsl_info("%s: PUBACK props len = %d\n",
					  __func__, (int)par->cpkt_remlen);
				/*
				 * If there are no properties, this is a
				 * command completion event in itself
				 */
				if (!par->props_len)
					goto cmd_completion;

				/*
				 * Otherwise consume the properties before
				 * completing the command
				 */
				lws_mqtt_vbi_init(&par->vbit);
				par->state = LMQCPP_PUBACK_VH_PKT_ID;
				break;
			default:
				lwsl_notice("%s: puback pr bad vbi\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_EAT_PROPERTIES_AND_COMPLETE:
			/*
			 * TODO: stash the props
			 */
			par->props_consumed++;
			len--;
			buf++;
			if (par->props_len != par->props_consumed)
				break;

cmd_completion:
			/*
			 * We come here when we understood we just processed
			 * the last byte of a command packet, regardless of the
			 * packet type
			 */
			par->state = LMQCPP_IDLE;

			switch (par->packet_type_flags >> 4) {
			case LMQCP_STOC_CONNACK:
				lwsl_info("%s: cmd_completion: CONNACK\n",
					  __func__);

				/*
				 * Getting the CONNACK means we are the first,
				 * the nwsi, and we succeeded to create a new
				 * network connection ourselves.
				 *
				 * Since others may join us sharing the nwsi,
				 * and we may close while they still want to use
				 * it, our wsi lifecycle alone can no longer
				 * define the lifecycle of the nwsi... it means
				 * we need to do a "magic trick" and instead of
				 * being both the nwsi and act like a child
				 * stream, create a new wsi to take over the
				 * nwsi duties and turn our wsi into a child of
				 * the nwsi with its own lifecycle.
				 *
				 * The nwsi gets a mostly empty wsi->nwsi used
				 * to track already-subscribed topics globally
				 * for the connection.
				 */

				/* we were under SENT_CLIENT_HANDSHAKE timeout */
				lws_set_timeout(wsi, 0, 0);

				w = lws_create_new_server_wsi(wsi->a.vhost,
							      wsi->tsi, "mqtt_sid1");
				if (!w) {
					lwsl_notice("%s: sid 1 migrate failed\n",
							__func__);
					return -1;
				}

				wsi->mux.highest_sid = 1;
				lws_wsi_mux_insert(w, wsi, wsi->mux.highest_sid++);

				wsi->mux_substream = 1;
				w->mux_substream = 1;
				w->client_mux_substream = 1;
				wsi->client_mux_migrated = 1;
				wsi->told_user_closed = 1; /* don't tell nwsi closed */

				lwsi_set_state(w, LRS_ESTABLISHED);
				lwsi_set_state(wsi, LRS_ESTABLISHED);
				lwsi_set_role(w, lwsi_role(wsi));

#if defined(LWS_WITH_CLIENT)
				w->flags = wsi->flags;
#endif

				w->mqtt = wsi->mqtt;
				wsi->mqtt = lws_zalloc(sizeof(*wsi->mqtt), "nwsi mqtt");
				if (!wsi->mqtt)
					return -1;
				w->mqtt->wsi = w;
				w->a.protocol = wsi->a.protocol;
				if (w->user_space &&
				    !w->user_space_externally_allocated)
					lws_free_set_NULL(w->user_space);
				w->user_space = wsi->user_space;
				wsi->user_space = NULL;
				w->user_space_externally_allocated =
					wsi->user_space_externally_allocated;
				if (lws_ensure_user_space(w))
					goto bail1;
				w->a.opaque_user_data = wsi->a.opaque_user_data;
				wsi->a.opaque_user_data = NULL;
				w->stash = wsi->stash;
				wsi->stash = NULL;

				lws_mux_mark_immortal(w);

				lwsl_notice("%s: migrated nwsi %s to sid 1 %s\n",
						__func__, lws_wsi_tag(wsi),
						lws_wsi_tag(w));

				/*
				 * It was the last thing we were waiting for
				 * before we can be fully ESTABLISHED
				 */
				if (lws_mqtt_set_client_established(w)) {
					lwsl_notice("%s: set EST fail\n", __func__);
					return -1;
				}

				/* get the ball rolling */
				lws_validity_confirmed(wsi);

				/* well, add the queued guys as children */
				lws_wsi_mux_apply_queue(wsi);
				break;

bail1:
				/* undo the insert */
				wsi->mux.child_list = w->mux.sibling_list;
				wsi->mux.child_count--;

				if (w->user_space)
					lws_free_set_NULL(w->user_space);
				w->a.vhost->protocols[0].callback(w,
							LWS_CALLBACK_WSI_DESTROY,
							NULL, NULL, 0);
				__lws_vhost_unbind_wsi(w); /* cx + vh lock */
				lws_free(w);

				return 0;

			case LMQCP_PUBREC:
				lwsl_err("%s: cmd_completion: PUBREC\n",
						__func__);
				/*
				 * Figure out which child asked for this
				 */
				n = 0;
				lws_start_foreach_ll(struct lws *, w,
						     wsi->mux.child_list) {
					if (w->mqtt->unacked_publish &&
					    w->mqtt->ack_pkt_id == par->cpkt_id) {
						char requested_close = 0;

						w->mqtt->unacked_publish = 0;
						w->mqtt->unacked_pubrel = 1;

						if (user_callback_handle_rxflow(
							    w->a.protocol->callback,
							    w, LWS_CALLBACK_MQTT_ACK,
							    w->user_space, NULL, 0) < 0) {
							lwsl_info("%s: MQTT_ACK requests close\n",
								 __func__);
							requested_close = 1;
						}
						n = 1;

						/*
						 * We got an assertive PUBREC,
						 * no need for timeout wait
						 * any more
						 */
						lws_sul_cancel(&w->mqtt->
							  sul_qos_puback_pubrec_wait);

						if (requested_close) {
							__lws_close_free_wsi(w,
								0, "ack cb");
							break;
						}

						break;
					}
				} lws_end_foreach_ll(w, mux.sibling_list);

				if (!n) {
					lwsl_err("%s: unsolicited PUBREC\n",
							__func__);
					return -1;
				}
				wsi->mqtt->send_pubrel = 1;
				lws_callback_on_writable(wsi);
				break;

			case LMQCP_PUBCOMP:
				lwsl_err("%s: cmd_completion: PUBCOMP\n",
						__func__);
				n = 0;
				lws_start_foreach_ll(struct lws *, w,
						     wsi->mux.child_list) {
					if (w->mqtt->unacked_pubrel > 0 &&
					    w->mqtt->ack_pkt_id == par->cpkt_id) {
						w->mqtt->unacked_pubrel = 0;
						n = 1;
					}
				} lws_end_foreach_ll(w, mux.sibling_list);

				if (!n) {
					lwsl_err("%s: unsolicited PUBCOMP\n",
							__func__);
					return -1;
				}

				/*
				 * If we published something and PUBCOMP arrived,
				 * our connection is definitely working in both
				 * directions at the moment.
				 */
				lws_validity_confirmed(wsi);
				break;

			case LMQCP_PUBREL:
				lwsl_err("%s: cmd_completion: PUBREL\n",
						__func__);
				wsi->mqtt->send_pubcomp = 1;
				lws_callback_on_writable(wsi);
				break;

			case LMQCP_PUBACK:
				lwsl_info("%s: cmd_completion: PUBACK\n",
						__func__);

				/*
				 * Figure out which child asked for this
				 */

				n = 0;
				lws_start_foreach_ll(struct lws *, w,
						      wsi->mux.child_list) {
					if (w->mqtt->unacked_publish &&
					    w->mqtt->ack_pkt_id == par->cpkt_id) {
						char requested_close = 0;

						w->mqtt->unacked_publish = 0;
						if (user_callback_handle_rxflow(
							    w->a.protocol->callback,
							    w, LWS_CALLBACK_MQTT_ACK,
							    w->user_space, NULL, 0) < 0) {
							lwsl_info("%s: MQTT_ACK requests close\n",
								 __func__);
							requested_close = 1;
						}
						n = 1;

						/*
						 * We got an assertive PUBACK,
						 * no need for ACK timeout wait
						 * any more
						 */
						lws_sul_cancel(&w->mqtt->sul_qos_puback_pubrec_wait);

						if (requested_close) {
							__lws_close_free_wsi(w,
								0, "ack cb");
							break;
						}

						break;
					}
				} lws_end_foreach_ll(w, mux.sibling_list);

				if (!n) {
					lwsl_err("%s: unsolicited PUBACK\n",
							__func__);
					return -1;
				}

				/*
				 * If we published something and it was acked,
				 * our connection is definitely working in both
				 * directions at the moment.
				 */
				lws_validity_confirmed(wsi);
				break;

			case LMQCP_STOC_PINGRESP:
				lwsl_info("%s: cmd_completion: PINGRESP\n",
						__func__);
				/*
				 * If we asked for a PINGRESP and it came,
				 * our connection is definitely working in both
				 * directions at the moment.
				 */
				lws_validity_confirmed(wsi);
				break;

			case LMQCP_STOC_SUBACK:
				lwsl_info("%s: cmd_completion: SUBACK\n",
						__func__);

				/*
				 * Figure out which child asked for this
				 */

				n = 0;
				lws_start_foreach_ll(struct lws *, w,
						      wsi->mux.child_list) {
					if (w->mqtt->inside_subscribe &&
					    w->mqtt->ack_pkt_id == par->cpkt_id) {
						w->mqtt->inside_subscribe = 0;
						if (user_callback_handle_rxflow(
							    w->a.protocol->callback,
							    w, LWS_CALLBACK_MQTT_SUBSCRIBED,
							    w->user_space, NULL, 0) < 0) {
							lwsl_err("%s: MQTT_SUBSCRIBE failed\n",
								 __func__);
							return -1;
						}
						n = 1;
						break;
					}
				} lws_end_foreach_ll(w, mux.sibling_list);

				if (!n) {
					lwsl_err("%s: unsolicited SUBACK\n",
							__func__);
					return -1;
				}

				/*
				 * If we subscribed to something and SUBACK came,
				 * our connection is definitely working in both
				 * directions at the moment.
				 */
				lws_validity_confirmed(wsi);

				break;

			case LMQCP_STOC_UNSUBACK:
			{
				char requested_close = 0;
				lwsl_info("%s: cmd_completion: UNSUBACK\n",
						__func__);
				/*
				 * Figure out which child asked for this
				 */
				n = 0;
				lws_start_foreach_ll(struct lws *, w,
						      wsi->mux.child_list) {
					if (w->mqtt->inside_unsubscribe &&
					    w->mqtt->ack_pkt_id == par->cpkt_id) {
						struct lws *nwsi = lws_get_network_wsi(w);

						/*
						 * No more subscribers left,
						 * remove the topic from nwsi
						 */
						lws_mqtt_client_remove_subs(nwsi->mqtt);

						w->mqtt->inside_unsubscribe = 0;
						if (user_callback_handle_rxflow(
							    w->a.protocol->callback,
							    w, LWS_CALLBACK_MQTT_UNSUBSCRIBED,
							    w->user_space, NULL, 0) < 0) {
							lwsl_info("%s: MQTT_UNSUBACK requests close\n",
								 __func__);
							requested_close = 1;
						}
						n = 1;

						lws_sul_cancel(&w->mqtt->sul_unsuback_wait);
						if (requested_close) {
							__lws_close_free_wsi(w,
									     0, "unsub ack cb");
							break;
						}
						break;
					}
				} lws_end_foreach_ll(w, mux.sibling_list);

				if (!n) {
					lwsl_err("%s: unsolicited UNSUBACK\n",
							__func__);
					return -1;
				}


				/*
				 * If we unsubscribed to something and
				 * UNSUBACK came, our connection is
				 * definitely working in both
				 * directions at the moment.
				 */
				lws_validity_confirmed(wsi);

				break;
			}
			case LMQCP_PUBLISH:
			{
				lws_mqtt_publish_param_t *pub =
						(lws_mqtt_publish_param_t *)
							wsi->mqtt->rx_cpkt_param;
				size_t chunk;

				if (pub == NULL) {
					lwsl_notice("%s: no pub\n", __func__);
					return -1;
				}

				/*
				 * RX PUBLISH is delivered to any children that
				 * registered for the related topic
				 */

				n = wsi->role_ops->rx_cb[lwsi_role_server(wsi)];

				chunk = pub->payload_len - pub->payload_pos;
				if (chunk > len)
					chunk = len;

				lws_start_foreach_ll(struct lws *, w,
						      wsi->mux.child_list) {
					if (lws_mqtt_find_sub(w->mqtt,
							      pub->topic))
						if (w->a.protocol->callback(
							    w, (enum lws_callback_reasons)n,
							    w->user_space,
							    (void *)pub,
							    chunk)) {
								par->payload_consumed = 0;
								lws_free_set_NULL(pub->topic);
								lws_free_set_NULL(wsi->mqtt->rx_cpkt_param);
								return 1;
							}
				} lws_end_foreach_ll(w, mux.sibling_list);


				pub->payload_pos += (uint32_t)chunk;
				len -= chunk;
				buf += chunk;

				lwsl_debug("%s: post pos %d, plen %d, len %d\n",
					    __func__, (int)pub->payload_pos,
					    (int)pub->payload_len, (int)len);

				if (pub->payload_pos != pub->payload_len) {
					/*
					 * More chunks of the payload pending,
					 * blocking this connection from doing
					 * anything else
					 */
					par->state = LMQCPP_PAYLOAD;
					break;
				}

				if (pub->qos == 1) {
				/* For QOS = 1, send out PUBACK */
					wsi->mqtt->send_puback = 1;
					lws_callback_on_writable(wsi);
				} else if (pub->qos == 2) {
				/* For QOS = 2, send out PUBREC */
					wsi->mqtt->send_pubrec = 1;
					lws_callback_on_writable(wsi);
				}

				par->payload_consumed = 0;
				lws_free_set_NULL(pub->topic);
				lws_free_set_NULL(wsi->mqtt->rx_cpkt_param);

				break;
			}
			default:
				break;
			}

			break;


		case LMQCPP_PROP_ID_VBI:
			switch (lws_mqtt_vbi_r(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				par->consumed = (uint32_t)((unsigned int)par->consumed + (unsigned int)(unsigned char)par->vbit.consumed);
				if (par->vbit.value >
				    LWS_ARRAY_SIZE(property_valid)) {
					lwsl_notice("%s: undef prop id 0x%x\n",
						  __func__, (int)par->vbit.value);
					goto send_protocol_error_and_close;
				}
				if (!(property_valid[par->vbit.value] &
					(1 << ctl_pkt_type(par)))) {
					lwsl_notice("%s: prop id 0x%x invalid for"
						  " control pkt %d\n", __func__,
						  (int)par->vbit.value,
						  ctl_pkt_type(par));
					goto send_protocol_error_and_close;
				}
				par->prop_id = par->vbit.value;
				par->flag_prop_multi = !!(
					par->props_seen[par->prop_id >> 3] &
					(1 << (par->prop_id & 7)));
				par->props_seen[par->prop_id >> 3] =
						(uint8_t)((par->props_seen[par->prop_id >> 3]) | (1 << (par->prop_id & 7)));
				/*
				 *  even if it's not a vbi property arg,
				 * .consumed of this will be zero the first time
				 */
				lws_mqtt_vbi_init(&par->vbit);
				/*
				 * if it's a string, next state must set the
				 * destination and size limit itself.  But
				 * resetting it generically here lets it use
				 * lws_mqtt_str_first() to understand it's the
				 * first time around.
				 */
				 lws_mqtt_str_init(&par->s_temp, NULL, 0, 0);

				/* property arg state enums are so encoded */
				par->state = 0x100 | par->vbit.value;
				break;
			default:
				lwsl_notice("%s: prop id bad vbi\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		/*
		 * All possible property payloads... restricting which ones
		 * can appear in which control packets is already done above
		 * in LMQCPP_PROP_ID_VBI
		 */

		case LMQCPP_PROP_REQUEST_PROBLEM_INFO_1BYTE:
		case LMQCPP_PROP_REQUEST_REPSONSE_INFO_1BYTE:
		case LMQCPP_PROP_MAXIMUM_QOS_1BYTE:
		case LMQCPP_PROP_RETAIN_AVAILABLE_1BYTE:
		case LMQCPP_PROP_WILDCARD_SUBSCRIPTION_AVAILABLE_1BYTE:
		case LMQCPP_PROP_SUBSCRIPTION_IDENTIFIER_AVAILABLE_1BYTE:
		case LMQCPP_PROP_SHARED_SUBSCRIPTION_AVAILABLE_1BYTE:
		case LMQCPP_PROP_PAYLOAD_FORMAT_INDICATOR_1BYTE: /* 3.3.2.3.2 */
			if (par->flag_prop_multi)
				goto singular_prop_seen_twice;
			par->payload_format = *buf++;
			len--;
			if (lws_mqtt_pconsume(par, 1))
				goto send_protocol_error_and_close;
			break;

		case LMQCPP_PROP_MAXIMUM_PACKET_SIZE_4BYTE:
		case LMQCPP_PROP_WILL_DELAY_INTERVAL_4BYTE:
		case LMQCPP_PROP_SESSION_EXPIRY_INTERVAL_4BYTE:
		case LMQCPP_PROP_MSG_EXPIRY_INTERVAL_4BYTE:
			if (par->flag_prop_multi)
				goto singular_prop_seen_twice;

			if (lws_mqtt_mb_first(&par->vbit))
				lws_mqtt_4byte_init(&par->vbit);

			switch (lws_mqtt_mb_parse(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				if (lws_mqtt_pconsume(par, par->vbit.consumed))
					goto send_protocol_error_and_close;
				break;
			default:
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_PROP_SERVER_KEEPALIVE_2BYTE:
		case LMQCPP_PROP_RECEIVE_MAXIMUM_2BYTE:
		case LMQCPP_PROP_TOPIC_MAXIMUM_2BYTE:
		case LMQCPP_PROP_TOPIC_ALIAS_2BYTE:
			if (par->flag_prop_multi)
				goto singular_prop_seen_twice;

			if (lws_mqtt_mb_first(&par->vbit))
				lws_mqtt_2byte_init(&par->vbit);

			switch (lws_mqtt_mb_parse(&par->vbit, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				if (lws_mqtt_pconsume(par, par->vbit.consumed))
					goto send_protocol_error_and_close;
				break;
			default:
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_PROP_ASSIGNED_CLIENTID_UTF8S:
		case LMQCPP_PROP_AUTH_METHOD_UTF8S:
		case LMQCPP_PROP_USER_PROPERTY_NAME_UTF8S:
		case LMQCPP_PROP_USER_PROPERTY_VALUE_UTF8S:
		case LMQCPP_PROP_RESPONSE_INFO_UTF8S:
		case LMQCPP_PROP_SERVER_REFERENCE_UTF8S:
		case LMQCPP_PROP_REASON_STRING_UTF8S:
		case LMQCPP_PROP_RESPONSE_TOPIC_UTF8S:
		case LMQCPP_PROP_CONTENT_TYPE_UTF8S:
			if (par->flag_prop_multi)
				goto singular_prop_seen_twice;

			if (lws_mqtt_str_first(&par->s_temp))
				lws_mqtt_str_init(&par->s_temp, par->temp,
						  sizeof(par->temp), 0);

			switch (lws_mqtt_str_parse(&par->s_temp, &buf, &len)) {
			case LMSPR_NEED_MORE:
				break;
			case LMSPR_COMPLETED:
				if (lws_mqtt_pconsume(par, par->s_temp.len))
					goto send_protocol_error_and_close;
				break;

			default:
				lwsl_info("%s: bad protocol name\n", __func__);
				goto send_protocol_error_and_close;
			}
			break;

		case LMQCPP_PROP_SUBSCRIPTION_ID_VBI:

		case LMQCPP_PROP_CORRELATION_BINDATA:
		case LMQCPP_PROP_AUTH_DATA_BINDATA:

		/* TODO */
			lwsl_err("%s: Unimplemented packet state 0x%x\n",
					__func__, par->state);
			return -1;
		}
	}

	return 0;

oom:
	lwsl_err("%s: OOM!\n", __func__);
	goto send_protocol_error_and_close;

singular_prop_seen_twice:
	lwsl_info("%s: property appears twice\n", __func__);

send_protocol_error_and_close:
	lwsl_notice("%s: peac\n", __func__);
	par->reason = LMQCP_REASON_PROTOCOL_ERROR;

send_reason_and_close:
	lwsl_notice("%s: srac\n", __func__);
	par->flag_pending_send_reason_close = 1;
	goto ask;

send_unsupp_connack_and_close:
	lwsl_notice("%s: unsupac\n", __func__);
	par->reason = LMQCP_REASON_UNSUPPORTED_PROTOCOL;
	par->flag_pending_send_connack_close = 1;

ask:
	/* Should we ask for clients? */
	lws_callback_on_writable(wsi);

	return -1;
}

int
lws_mqtt_fill_fixed_header(uint8_t *p, lws_mqtt_control_packet_t ctrl_pkt_type,
			   uint8_t dup, lws_mqtt_qos_levels_t qos,
			   uint8_t retain)
{
	lws_mqtt_fixed_hdr_t hdr;

	hdr.bits = 0;
	hdr.flags.ctrl_pkt_type = ctrl_pkt_type & 0xf;

	switch(ctrl_pkt_type) {
	case LMQCP_PUBLISH:
		hdr.flags.dup = !!dup;
		/*
		 * A PUBLISH Packet MUST NOT have both QoS bits set to
		 * 1. If a Server or Client receives a PUBLISH Packet
		 * which has both QoS bits set to 1 it MUST close the
		 * Network Connection [MQTT-3.3.1-4].
		 */
		if (qos >= RESERVED_QOS_LEVEL) {
			lwsl_err("%s: Unsupport QoS level 0x%x\n",
				 __func__, qos);
			return -1;
		}
		hdr.flags.qos = qos & 3;
		hdr.flags.retain = !!retain;
		break;

	case LMQCP_CTOS_CONNECT:
	case LMQCP_STOC_CONNACK:
	case LMQCP_PUBACK:
	case LMQCP_PUBREC:
	case LMQCP_PUBCOMP:
	case LMQCP_STOC_SUBACK:
	case LMQCP_STOC_UNSUBACK:
	case LMQCP_CTOS_PINGREQ:
	case LMQCP_STOC_PINGRESP:
	case LMQCP_DISCONNECT:
	case LMQCP_AUTH:
		hdr.bits &= 0xf0;
		break;

	/*
	 * Bits 3,2,1 and 0 of the fixed header of the PUBREL,
	 * SUBSCRIBE, UNSUBSCRIBE Control Packets are reserved and
	 * MUST be set to 0,0,1 and 0 respectively. The Server MUST
	 * treat any other value as malformed and close the Network
	 * Connection [MQTT-3.6.1-1], [MQTT-3.8.1-1], [MQTT-3.10.1-1].
	 */
	case LMQCP_PUBREL:
	case LMQCP_CTOS_SUBSCRIBE:
	case LMQCP_CTOS_UNSUBSCRIBE:
		hdr.bits |= 0x02;
		break;

	default:
		return -1;
	}

	*p = hdr.bits;

	return 0;
}

int
lws_mqtt_client_send_publish(struct lws *wsi, lws_mqtt_publish_param_t *pub,
			     const void *buf, uint32_t len, int is_complete)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	uint8_t *b = (uint8_t *)pt->serv_buf, *start, *p;
	struct lws *nwsi = lws_get_network_wsi(wsi);
	lws_mqtt_str_t mqtt_vh_payload;
	uint32_t vh_len, rem_len;

	assert(pub->topic);

	lwsl_debug("%s: len = %d, is_complete = %d\n",
		   __func__, (int)len, (int)is_complete);

	if (lwsi_state(wsi) != LRS_ESTABLISHED) {
		lwsl_err("%s: %s: unknown state 0x%x\n", __func__,
				lws_wsi_tag(wsi), lwsi_state(wsi));
		assert(0);
		return 1;
	}

	if (wsi->mqtt->inside_payload) {
		/*
		 * Headers are filled, we are sending
		 * the payload - a buffer with LWS_PRE
		 * in front it.
		 */
		start = (uint8_t *)buf;
		p = start + len;
		if (is_complete)
			wsi->mqtt->inside_payload = 0;
		goto do_write;
	}

	start = b + LWS_PRE;
	p = start;
	/*
	 * Fill headers and the first chunk of the
	 * payload (if any)
	 */
	if (lws_mqtt_fill_fixed_header(p++, LMQCP_PUBLISH,
				       pub->dup, pub->qos, pub->retain)) {
		lwsl_err("%s: Failed to fill fixed header\n", __func__);
		return 1;
	}

	/*
	 * Topic len field + Topic len + Packet ID
	 * (for QOS>0) + Payload len
	 */
	vh_len = (unsigned int)(2 + pub->topic_len + ((pub->qos) ? 2 : 0));
	rem_len = vh_len + pub->payload_len;
	lwsl_debug("%s: Remaining len = %d\n", __func__, (int) rem_len);

	/* Will the chunk of payload fit? */
	if ((vh_len + len) >=
	    (wsi->a.context->pt_serv_buf_size - LWS_PRE)) {
		lwsl_err("%s: Payload is too big\n", __func__);
		return 1;
	}

	p += lws_mqtt_vbi_encode(rem_len, p);

	/* Topic's Len */
	lws_ser_wu16be(p, pub->topic_len);
	p += 2;

	/*
	 * Init lws_mqtt_str for "MQTT Variable
	 * Headers + payload" (only the supplied
	 * chuncked payload)
	 */
	lws_mqtt_str_init(&mqtt_vh_payload, (uint8_t *)p,
			  (uint16_t)(unsigned int)(pub->topic_len + ((pub->qos) ? 2u : 0u) + len),
			  0);

	p = lws_mqtt_str_next(&mqtt_vh_payload, NULL);
	lws_strncpy((char *)p, pub->topic, (size_t)pub->topic_len+1);
	if (lws_mqtt_str_advance(&mqtt_vh_payload, pub->topic_len)) {
		lwsl_err("%s: a\n", __func__);
		return 1;
	}

	/* Packet ID */
	if (pub->qos != QOS0) {
		p = lws_mqtt_str_next(&mqtt_vh_payload, NULL);
		if (!pub->dup)
			nwsi->mqtt->pkt_id++;
		wsi->mqtt->ack_pkt_id = pub->packet_id = nwsi->mqtt->pkt_id;
		lwsl_debug("%s: pkt_id = %d\n", __func__,
			   (int)wsi->mqtt->ack_pkt_id);
		lws_ser_wu16be(p, pub->packet_id);
		if (lws_mqtt_str_advance(&mqtt_vh_payload, 2)) {
			lwsl_err("%s: b\n", __func__);
			return 1;
		}
	}

	p = lws_mqtt_str_next(&mqtt_vh_payload, NULL);
	memcpy(p, buf, len);
	if (lws_mqtt_str_advance(&mqtt_vh_payload, (int)len))
		return 1;
	p = lws_mqtt_str_next(&mqtt_vh_payload, NULL);

	if (!is_complete)
		nwsi->mqtt->inside_payload = wsi->mqtt->inside_payload = 1;

do_write:

	// lwsl_hexdump_err(start, lws_ptr_diff(p, start));

	if (lws_write(nwsi, start, lws_ptr_diff_size_t(p, start), LWS_WRITE_BINARY) !=
			lws_ptr_diff(p, start)) {
		lwsl_err("%s: write failed\n", __func__);
		return 1;
	}

	if (!is_complete) {
		/* still some more chunks to come... */
		lws_callback_on_writable(wsi);

		return 0;
	}

	wsi->mqtt->inside_payload = nwsi->mqtt->inside_payload = 0;

	if (pub->qos != QOS0)
		wsi->mqtt->unacked_publish = 1;

	/* this was the last part of the publish message */

	if (pub->qos == QOS0) {
		/*
		 * There won't be any real PUBACK, act like we got one
		 * so the user callback logic is the same for QoS0 or
		 * QoS1
		 */
		if (wsi->a.protocol->callback(wsi, LWS_CALLBACK_MQTT_ACK,
					    wsi->user_space, NULL, 0)) {
			lwsl_err("%s: ACK callback exited\n", __func__);
			return 1;
		}
	} else if (pub->qos == QOS1 || pub->qos == QOS2) {
		/* For QoS1 or QoS2, if no PUBACK or PUBREC coming after 3s,
		 * we must RETRY the publish
		 */
		wsi->mqtt->sul_qos_puback_pubrec_wait.cb = lws_mqtt_publish_resend;
		__lws_sul_insert_us(&pt->pt_sul_owner[wsi->conn_validity_wakesuspend],
				    &wsi->mqtt->sul_qos_puback_pubrec_wait,
				    3 * LWS_USEC_PER_SEC);
	}

	if (wsi->mqtt->inside_shadow) {
		wsi->mqtt->sul_shadow_wait.cb = lws_mqtt_shadow_timeout;
		__lws_sul_insert_us(&pt->pt_sul_owner[wsi->conn_validity_wakesuspend],
				    &wsi->mqtt->sul_shadow_wait,
				    60 * LWS_USEC_PER_SEC);
	}

	return 0;
}

int
lws_mqtt_client_send_subcribe(struct lws *wsi, lws_mqtt_subscribe_param_t *sub)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	uint8_t *b = (uint8_t *)pt->serv_buf + LWS_PRE, *start = b, *p = start;
	struct lws *nwsi = lws_get_network_wsi(wsi);
	lws_mqtt_str_t mqtt_vh_payload;
	uint8_t exists[8], extant;
	lws_mqtt_subs_t *mysub;
	uint32_t rem_len;
#if defined(_DEBUG)
	uint32_t tops;
#endif
	uint32_t n;

	assert(sub->num_topics);
	assert(sub->num_topics < sizeof(exists));

	switch (lwsi_state(wsi)) {
	case LRS_ESTABLISHED: /* Protocol connection established */
		if (lws_mqtt_fill_fixed_header(p++, LMQCP_CTOS_SUBSCRIBE,
					       0, 0, 0)) {
			lwsl_err("%s: Failed to fill fixed header\n", __func__);
			return 1;
		}

		/*
		 * The stream wants to subscribe to one or more topic, but
		 * the shared nwsi may already be subscribed to some or all of
		 * them from interactions with other streams.  For those cases,
		 * we filter them from the list the child wants until we just
		 * have ones that are new to the nwsi.  If nothing left, we just
		 * synthesize the callback to the child as if SUBACK had come
		 * and we're done, otherwise just ask the server for topics that
		 * are new to the wsi.
		 */

		extant = 0;
		memset(&exists, 0, sizeof(exists));
		for (n = 0; n < sub->num_topics; n++) {
			lwsl_info("%s: Subscribing to topic[%d] = \"%s\"\n",
				  __func__, (int)n, sub->topic[n].name);

			mysub = lws_mqtt_find_sub(nwsi->mqtt, sub->topic[n].name);
			if (mysub && mysub->ref_count) {
				mysub->ref_count++; /* another stream using it */
				exists[n] = 1;
				extant++;
			}

			/*
			 * Attach the topic we're subscribing to, to wsi->mqtt
			 */
			if (!lws_mqtt_create_sub(wsi->mqtt, sub->topic[n].name)) {
				lwsl_err("%s: create sub fail\n", __func__);
				return 1;
			}
		}

		if (extant == sub->num_topics) {
			/*
			 * It turns out there's nothing to do here, the nwsi has
			 * already subscribed to all the topics this stream
			 * wanted.  Just tell it it can have them.
			 */
			lwsl_notice("%s: all topics already subscribed\n", __func__);
			if (user_callback_handle_rxflow(
				    wsi->a.protocol->callback,
				    wsi, LWS_CALLBACK_MQTT_SUBSCRIBED,
				    wsi->user_space, NULL, 0) < 0) {
				lwsl_err("%s: MQTT_SUBSCRIBE failed\n",
					 __func__);
				return -1;
			}

			return 0;
		}

#if defined(_DEBUG)
		/*
		 * zero or more of the topics already existed, but not all,
		 * so we must go to the server with a filtered list of the
		 * new ones only
		 */

		tops = sub->num_topics - extant;
#endif

		/*
		 * Pid + (Topic len field + Topic len + Req. QoS) x Num of Topics
		 */
		rem_len = 2;
		for (n = 0; n < sub->num_topics; n++)
			if (!exists[n])
				rem_len += (2 + (uint32_t)strlen(sub->topic[n].name) + (uint32_t)1);

		wsi->mqtt->sub_size = (uint16_t)rem_len;

#if defined(_DEBUG)
		lwsl_debug("%s: Number of topics = %d, Remaining len = %d\n",
			   __func__, (int)tops, (int)rem_len);
#endif

		p += lws_mqtt_vbi_encode(rem_len, p);

		if ((rem_len + lws_ptr_diff_size_t(p, start)) >=
					       wsi->a.context->pt_serv_buf_size) {
			lwsl_err("%s: Payload is too big\n", __func__);
			return 1;
		}

		/* Init lws_mqtt_str */
		lws_mqtt_str_init(&mqtt_vh_payload, (uint8_t *)p, (uint16_t)rem_len, 0);
		p = lws_mqtt_str_next(&mqtt_vh_payload, NULL);

		/* Packet ID */
		wsi->mqtt->ack_pkt_id = sub->packet_id = ++nwsi->mqtt->pkt_id;
		lwsl_debug("%s: pkt_id = %d\n", __func__,
			   (int)sub->packet_id);
		lws_ser_wu16be(p, wsi->mqtt->ack_pkt_id);

		nwsi->mqtt->client.aws_iot = wsi->mqtt->client.aws_iot;

		if (lws_mqtt_str_advance(&mqtt_vh_payload, 2))
			return 1;

		p = lws_mqtt_str_next(&mqtt_vh_payload, NULL);

		for (n = 0; n < sub->num_topics; n++) {
			lwsl_info("%s: topics[%d] = %s\n", __func__,
				   (int)n, sub->topic[n].name);

			/* if the nwsi already has it, don't ask server for it */
			if (exists[n]) {
				lwsl_info("%s: topics[%d] \"%s\" exists in nwsi\n",
					    __func__, (int)n, sub->topic[n].name);
				continue;
			}

			/*
			 * Attach the topic we're subscribing to, to nwsi->mqtt
			 * so we know the nwsi itself has a subscription to it
			 */

			if (!lws_mqtt_create_sub(nwsi->mqtt, sub->topic[n].name))
				return 1;

			/* Topic's Len */
			lws_ser_wu16be(p, (uint16_t)strlen(sub->topic[n].name));
			if (lws_mqtt_str_advance(&mqtt_vh_payload, 2))
				return 1;
			p = lws_mqtt_str_next(&mqtt_vh_payload, NULL);

			/* Topic Name */
			lws_strncpy((char *)p, sub->topic[n].name,
				    strlen(sub->topic[n].name) + 1);
			if (lws_mqtt_str_advance(&mqtt_vh_payload,
						 (int)strlen(sub->topic[n].name)))
				return 1;
			p = lws_mqtt_str_next(&mqtt_vh_payload, NULL);

			/* QoS */
			*p = (uint8_t)sub->topic[n].qos;
			if (lws_mqtt_str_advance(&mqtt_vh_payload, 1))
				return 1;
			p = lws_mqtt_str_next(&mqtt_vh_payload, NULL);
		}
		break;

	default:
		return 1;
	}

	if (wsi->mqtt->inside_resume_session)
		return 0;

	if (lws_write(nwsi, start, lws_ptr_diff_size_t(p, start), LWS_WRITE_BINARY) !=
					lws_ptr_diff(p, start))
		return 1;

	wsi->mqtt->inside_subscribe = 1;

	return 0;
}

int
lws_mqtt_client_send_unsubcribe(struct lws *wsi,
				const lws_mqtt_subscribe_param_t *unsub)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	uint8_t *b = (uint8_t *)pt->serv_buf + LWS_PRE, *start = b, *p = start;
	struct lws *nwsi = lws_get_network_wsi(wsi);
	lws_mqtt_str_t mqtt_vh_payload;
	uint8_t send_unsub[8], orphaned;
	uint32_t rem_len, n;
	lws_mqtt_subs_t *mysub;
#if defined(_DEBUG)
	uint32_t tops;
#endif

	lwsl_info("%s: Enter\n", __func__);

	switch (lwsi_state(wsi)) {
	case LRS_ESTABLISHED: /* Protocol connection established */
		orphaned = 0;
		memset(&send_unsub, 0, sizeof(send_unsub));
		for (n = 0; n < unsub->num_topics; n++) {
			mysub = lws_mqtt_find_sub(nwsi->mqtt,
						  unsub->topic[n].name);
			assert(mysub);

			if (mysub && --mysub->ref_count == 0) {
				lwsl_notice("%s: Need to send UNSUB\n", __func__);
				send_unsub[n] = 1;
				orphaned++;
			}
		}

		if (!orphaned) {
			/*
			 * The nwsi still has other subscribers bound to the
			 * topics.
			 *
			 * So, don't send UNSUB to server, and just fake the
			 * UNSUB ACK event for the guy going away.
			 */
			lwsl_notice("%s: unsubscribed!\n", __func__);
			if (user_callback_handle_rxflow(
				    wsi->a.protocol->callback,
				    wsi, LWS_CALLBACK_MQTT_UNSUBSCRIBED,
				    wsi->user_space, NULL, 0) < 0) {
				/*
				 * We can't directly close here, because the
				 * caller still has the wsi.  Inform the
				 * caller that we want to close
				 */

				return 1;
			}

			return 0;
		}
#if defined(_DEBUG)
		/*
		 * one or more of the topics needs to be unsubscribed
		 * from, so we must go to the server with a filtered
		 * list of the new ones only
		 */

		tops = orphaned;
#endif

		if (lws_mqtt_fill_fixed_header(p++, LMQCP_CTOS_UNSUBSCRIBE,
					       0, 0, 0)) {
			lwsl_err("%s: Failed to fill fixed header\n", __func__);
			return 1;
		}

		/*
		 * Pid + (Topic len field + Topic len) x Num of Topics
		 */
		rem_len = 2;
		for (n = 0; n < unsub->num_topics; n++)
			if (send_unsub[n])
				rem_len += (2 + (uint32_t)strlen(unsub->topic[n].name));

		wsi->mqtt->sub_size = (uint16_t)rem_len;

#if defined(_DEBUG)
		lwsl_debug("%s: Number of topics = %d, Remaining len = %d\n",
			   __func__, (int)tops, (int)rem_len);
#endif

		p += lws_mqtt_vbi_encode(rem_len, p);

		if ((rem_len + lws_ptr_diff_size_t(p, start)) >=
					       wsi->a.context->pt_serv_buf_size) {
			lwsl_err("%s: Payload is too big\n", __func__);
			return 1;
		}

		/* Init lws_mqtt_str */
		lws_mqtt_str_init(&mqtt_vh_payload, (uint8_t *)p, (uint16_t)rem_len, 0);
		p = lws_mqtt_str_next(&mqtt_vh_payload, NULL);

		/* Packet ID */
		wsi->mqtt->ack_pkt_id = ++nwsi->mqtt->pkt_id;
		lwsl_debug("%s: pkt_id = %d\n", __func__,
			   (int)wsi->mqtt->ack_pkt_id);
		lws_ser_wu16be(p, wsi->mqtt->ack_pkt_id);

		nwsi->mqtt->client.aws_iot = wsi->mqtt->client.aws_iot;

		if (lws_mqtt_str_advance(&mqtt_vh_payload, 2))
			return 1;

		p = lws_mqtt_str_next(&mqtt_vh_payload, NULL);

		for (n = 0; n < unsub->num_topics; n++) {
			lwsl_info("%s: topics[%d] = %s\n", __func__,
				   (int)n, unsub->topic[n].name);

			/*
			 * Subscriber still bound to it, don't UBSUB
			 * from the server
			 */
			if (!send_unsub[n])
				continue;

			/* Topic's Len */
			lws_ser_wu16be(p, (uint16_t)strlen(unsub->topic[n].name));
			if (lws_mqtt_str_advance(&mqtt_vh_payload, 2))
				return 1;
			p = lws_mqtt_str_next(&mqtt_vh_payload, NULL);

			/* Topic Name */
			lws_strncpy((char *)p, unsub->topic[n].name,
				    strlen(unsub->topic[n].name) + 1);
			if (lws_mqtt_str_advance(&mqtt_vh_payload,
						 (int)strlen(unsub->topic[n].name)))
				return 1;
			p = lws_mqtt_str_next(&mqtt_vh_payload, NULL);
		}
		break;

	default:
		return 1;
	}

	if (lws_write(nwsi, start, lws_ptr_diff_size_t(p, start), LWS_WRITE_BINARY) !=
					lws_ptr_diff(p, start))
		return 1;

	wsi->mqtt->inside_unsubscribe = 1;

	wsi->mqtt->sul_unsuback_wait.cb = lws_mqtt_unsuback_timeout;
	__lws_sul_insert_us(&pt->pt_sul_owner[wsi->conn_validity_wakesuspend],
			    &wsi->mqtt->sul_unsuback_wait,
			    3 * LWS_USEC_PER_SEC);

	return 0;
}

/*
 * This is called when child streams bind to an already-existing and compatible
 * MQTT stream
 */

struct lws *
lws_wsi_mqtt_adopt(struct lws *parent_wsi, struct lws *wsi)
{
	/* no more children allowed by parent? */

	if (parent_wsi->mux.child_count + 1 > LWS_MQTT_MAX_CHILDREN) {
		lwsl_err("%s: reached concurrent stream limit\n", __func__);
		return NULL;
	}

#if defined(LWS_WITH_CLIENT)
	wsi->client_mux_substream = 1;
#endif

	lws_wsi_mux_insert(wsi, parent_wsi, wsi->mux.my_sid);

	if (lws_ensure_user_space(wsi))
		goto bail1;

	lws_mqtt_set_client_established(wsi);
	lws_callback_on_writable(wsi);

	return wsi;

bail1:
	/* undo the insert */
	parent_wsi->mux.child_list = wsi->mux.sibling_list;
	parent_wsi->mux.child_count--;

	if (wsi->user_space)
		lws_free_set_NULL(wsi->user_space);

	wsi->a.protocol->callback(wsi, LWS_CALLBACK_WSI_DESTROY, NULL, NULL, 0);
	lws_free(wsi);

	return NULL;
}

