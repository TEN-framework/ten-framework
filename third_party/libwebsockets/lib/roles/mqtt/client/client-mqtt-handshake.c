/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
 *                           Sakthi Kannan <saktr@amazon.com>
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

#include <private-lib-core.h>

#define MQTT_CONNECT_MSG_BASE_LEN (12)

struct lws *
lws_mqtt_client_send_connect(struct lws *wsi)
{
	/* static int */
	/* 	lws_mqttc_abs_writeable(lws_abs_protocol_inst_t *api, size_t budget) */
	const lws_mqttc_t *c = &wsi->mqtt->client;
	uint8_t b[256 + LWS_PRE], *start = b + LWS_PRE, *p = start;
	unsigned int len = MQTT_CONNECT_MSG_BASE_LEN;

	switch (lwsi_state(wsi)) {
	case LRS_MQTTC_IDLE:
		/*
		 * Transport connected - this is our chance to do the
		 * protocol connect action.
		 */

		/* 1. Fixed Headers */
		if (lws_mqtt_fill_fixed_header(p++, LMQCP_CTOS_CONNECT, 0, 0, 0)) {
			lwsl_err("%s: Failled to fill fixed header\n", __func__);
			return NULL;
		}

		/*
		 * 2. Remaining length - Add the lengths of client ID,
		 * username and password and their length fields if
		 * the respective flags are set.
		 */
		len +=  c->id->len;
		if (c->conn_flags & LMQCFT_USERNAME && c->username) {
			len = len + (unsigned int)c->username->len + 2;
			if (c->conn_flags & LMQCFT_PASSWORD)
				len += (unsigned int)(c->password ? c->password->len : 0) + 2u;
		}
		if (c->conn_flags & LMQCFT_WILL_FLAG && c->will.topic) {
			len = len + (unsigned int)c->will.topic->len + 2;
			len += (c->will.message ? c->will.message->len : 0) + 2u;
		}
		p += lws_mqtt_vbi_encode(len, p);

		/*
		 * 3. Variable Header - Protocol name & level, Connect
		 * flags and keep alive time (in secs).
		 */
		lws_ser_wu16be(p, 4); /* Length of protocol name */
		p += 2;
		*p++ = 'M';
		*p++ = 'Q';
		*p++ = 'T';
		*p++ = 'T';
		*p++ = MQTT_VER_3_1_1;
		*p++ = (uint8_t)c->conn_flags;
		lws_ser_wu16be(p, c->keep_alive_secs);
		p += 2;

		/*
		 * 4. Payload - Client ID, Will topic & message,
		 * Username & password.
		 */
		if (lws_mqtt_str_is_not_empty(c->id)) {
			lws_ser_wu16be(p, c->id->len);
			p += 2;
			memcpy(p, c->id->buf, c->id->len);
			p += c->id->len;
		} else {
			/*
			 * If the Client supplies a zero-byte
			 * ClientId, the Client MUST also set
			 * CleanSession to 1 [MQTT-3.1.3-7].
			 */
			if (!(c->conn_flags & LMQCFT_CLEAN_START)) {
				lwsl_err("%s: Empty client ID needs a clean start\n",
					 __func__);
				return NULL;
			}
			*p++ = 0;
		}

		if (c->conn_flags & LMQCFT_WILL_FLAG) {
			if (lws_mqtt_str_is_not_empty(c->will.topic)) {
				lws_ser_wu16be(p, c->will.topic->len);
				p += 2;
				memcpy(p, c->will.topic->buf, c->will.topic->len);
				p += c->will.topic->len;
				if (lws_mqtt_str_is_not_empty(c->will.message)) {
					lws_ser_wu16be(p, c->will.message->len);
					p += 2;
					memcpy(p, c->will.message->buf,
					       c->will.message->len);
					p += c->will.message->len;
				} else {
					lws_ser_wu16be(p, 0);
					p += 2;
				}
			} else {
				lwsl_err("%s: Missing Will Topic\n", __func__);
				return NULL;
			}
		}
		if (c->conn_flags & LMQCFT_USERNAME) {
			/*
			 * Detailed sanity check on the username and
			 * password strings.
			 */
			if (lws_mqtt_str_is_not_empty(c->username)) {
				lws_ser_wu16be(p, c->username->len);
				p += 2;
				memcpy(p, c->username->buf, c->username->len);
				p += c->username->len;
			} else {
				lwsl_err("%s: Empty / missing Username!\n",
					 __func__);
				return NULL;
			}
			if (c->conn_flags & LMQCFT_PASSWORD) {
				if (lws_mqtt_str_is_not_empty(c->password)) {
					lws_ser_wu16be(p, c->password->len);
					p += 2;
					memcpy(p, c->password->buf,
					       c->password->len);
					p += c->password->len;
				} else {
					lws_ser_wu16be(p, 0);
					p += 2;
				}
			}
		} else if (c->conn_flags & LMQCFT_PASSWORD) {
			lwsl_err("%s: Unsupported - Password without username\n",
				 __func__);
			return NULL;
		}
		break;
	default:
		lwsl_err("%s: unexpected state %d\n", __func__, lwsi_state(wsi));

		return NULL;
	}

	/*
	 * Perform the actual write
	 */
	if (lws_write(wsi, (unsigned char *)&b[LWS_PRE], lws_ptr_diff_size_t(p, start),
		  LWS_WRITE_BINARY) != lws_ptr_diff(p, start)) {
		lwsl_notice("%s: write failed\n", __func__);

		return NULL;
	}

	return wsi;
}

struct lws *
lws_mqtt_client_send_disconnect(struct lws *wsi)
{
	uint8_t b[256 + LWS_PRE], *start = b + LWS_PRE, *p = start;

	/* 1. Fixed Headers */
	if (lws_mqtt_fill_fixed_header(p++, LMQCP_DISCONNECT, 0, 0, 0))
	{
		lwsl_err("%s: Failled to fill fixed header\n", __func__);
		return NULL;
	}
	*p++ = 0;
	if (lws_write(wsi, (unsigned char *)&b[LWS_PRE], lws_ptr_diff_size_t(p, start),
				LWS_WRITE_BINARY) != lws_ptr_diff(p, start)) {
		lwsl_err("%s: write failed\n", __func__);

		return NULL;
	}

	return wsi;
}
