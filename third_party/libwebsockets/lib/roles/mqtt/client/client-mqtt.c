/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
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

/*
 * You can leave buf NULL, if so it will be allocated on the heap once the
 * actual length is known.  nf should be 0, it will be set at allocation time.
 *
 * Or you can ensure no allocation and use an external buffer by setting buf
 * and lim.  But buf must be in the ep context somehow, since it may have to
 * survive returns to the event loop unchanged.  Set nf to 0 in this case.
 *
 * Or you can set buf to an externally allocated buffer, in which case you may
 * set nf so it will be freed when the string is "freed".
 */

#include "private-lib-core.h"
/* #include "lws-mqtt.h" */
/* 3.1.3.1-5: MUST allow... that contain only the characters... */

static const uint8_t *code = (const uint8_t *)
	"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static int
lws_mqtt_generate_id(struct lws* wsi, lws_mqtt_str_t **ms, const char *client_id)
{
	struct lws_context *context = wsi->a.context;
	uint16_t ran[24]; /* 16-bit so wrap bias from %62 diluted by ~1000 */
	size_t n, len;
	uint8_t *buf;

	if (client_id)
		len = strlen(client_id);
	else
		len = LWS_MQTT_RANDOM_CIDLEN;

	*ms = lws_mqtt_str_create((uint16_t)(len + 1));
	if (!*ms)
		return 1;

	buf = lws_mqtt_str_next(*ms, NULL);

	if (client_id) {
		lws_strnncpy((char *)buf, client_id, len, len + 1);
		lwsl_notice("%s: User space provided a client ID '%s'\n",
			    __func__, (const char *)buf);
	} else {
		lwsl_notice("%s: generating random client id\n", __func__);
		n = len * sizeof(ran[0]);
		if (lws_get_random(context, ran, n) != n) {
			lws_mqtt_str_free(ms);

			return 1;
		}

		for (n = 0; n < len; n++)
			buf[n] = code[ran[n] % 62];
		buf[len] = '\0';
	}

	if (lws_mqtt_str_advance(*ms, (uint16_t)len)) {
		lws_mqtt_str_free(ms);

		return 1;
	}

	return 0;
}

int
lws_read_mqtt(struct lws *wsi, unsigned char *buf, lws_filepos_t len)
{
	lws_mqttc_t *c = &wsi->mqtt->client;

	return _lws_mqtt_rx_parser(wsi, &c->par, buf, (size_t)len);
}

int
lws_create_client_mqtt_object(const struct lws_client_connect_info *i,
			      struct lws *wsi)
{
	lws_mqttc_t *c;
	const lws_mqtt_client_connect_param_t *cp = i->mqtt_cp;

	/* allocate the ws struct for the wsi */
	wsi->mqtt = lws_zalloc(sizeof(*wsi->mqtt), "client mqtt struct");
	if (!wsi->mqtt)
		goto oom;

	wsi->mqtt->wsi = wsi;
	c = &wsi->mqtt->client;

	if (lws_mqtt_generate_id(wsi, &c->id, cp->client_id)) {
		lwsl_err("%s: Error generating client ID\n", __func__);
		return 1;
	}
	lwsl_info("%s: using client id '%.*s'\n", __func__, c->id->len,
			(const char *)c->id->buf);

	if (cp->clean_start || !(cp->client_id &&
				 cp->client_id[0]))
		c->conn_flags = LMQCFT_CLEAN_START;
	if (cp->client_id_nofree)
		c->conn_flags |= LMQCFT_CLIENT_ID_NOFREE;
	if (cp->username_nofree)
		c->conn_flags |= LMQCFT_USERNAME_NOFREE;
	if (cp->password_nofree)
		c->conn_flags |= LMQCFT_PASSWORD_NOFREE;

	if (!(c->conn_flags & LMQCFT_CLIENT_ID_NOFREE))
		lws_free((void *)cp->client_id);

	c->keep_alive_secs = cp->keep_alive;
	c->aws_iot = cp->aws_iot;

	if (cp->will_param.topic &&
	    *cp->will_param.topic) {
		c->will.topic = lws_mqtt_str_create_cstr_dup(
						cp->will_param.topic, 0);
		if (!c->will.topic)
			goto oom1;
		c->conn_flags |= LMQCFT_WILL_FLAG;
		if (cp->will_param.message) {
			c->will.message = lws_mqtt_str_create_cstr_dup(
						cp->will_param.message, 0);
			if (!c->will.message)
				goto oom2;
		}
		c->conn_flags = (uint16_t)(unsigned int)(c->conn_flags | ((cp->will_param.qos << 3) & LMQCFT_WILL_QOS_MASK));
		c->conn_flags |= (uint16_t)((!!cp->will_param.retain) * LMQCFT_WILL_RETAIN);
	}

	if (cp->username &&
	    *cp->username) {
		c->username = lws_mqtt_str_create_cstr_dup(cp->username, 0);
		if (!c->username)
			goto oom3;
		c->conn_flags |= LMQCFT_USERNAME;
		if (!(c->conn_flags & LMQCFT_USERNAME_NOFREE))
			lws_free((void *)cp->username);
		if (cp->password) {
			c->password =
				lws_mqtt_str_create_cstr_dup(cp->password, 0);
			if (!c->password)
				goto oom4;
			c->conn_flags |= LMQCFT_PASSWORD;
			if (!(c->conn_flags & LMQCFT_PASSWORD_NOFREE))
				lws_free((void *)cp->password);
		}
	}

	return 0;
oom4:
	lws_mqtt_str_free(&c->username);
oom3:
	lws_mqtt_str_free(&c->will.message);
oom2:
	lws_mqtt_str_free(&c->will.topic);
oom1:
	lws_mqtt_str_free(&c->id);
oom:
	lwsl_err("%s: OOM!\n", __func__);
	return 1;
}

int
lws_mqtt_client_socket_service(struct lws *wsi, struct lws_pollfd *pollfd,
			  struct lws *wsi_conn)
{
	struct lws_context *context = wsi->a.context;
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];
	int n = 0, m = 0;
	struct lws_tokens ebuf;
	int buffered = 0;
	int pending = 0;
#if defined(LWS_WITH_TLS)
	char erbuf[128];
#endif
	const char *cce = NULL;

	switch (lwsi_state(wsi)) {
#if defined(LWS_WITH_SOCKS5)
	/* SOCKS Greeting Reply */
	case LRS_WAITING_SOCKS_GREETING_REPLY:
	case LRS_WAITING_SOCKS_AUTH_REPLY:
	case LRS_WAITING_SOCKS_CONNECT_REPLY:

		switch (lws_socks5c_handle_state(wsi, pollfd, &cce)) {
		case LW5CHS_RET_RET0:
			return 0;
		case LW5CHS_RET_BAIL3:
			goto bail3;
		case LW5CHS_RET_STARTHS:

			/*
			 * Now we got the socks5 connection, we need to go down
			 * the tls path on it if that's what we want
			 */

			if (!(wsi->tls.use_ssl & LCCSCF_USE_SSL))
				goto start_ws_handshake;

			switch (lws_client_create_tls(wsi, &cce, 0)) {
			case 0:
				break;
			case 1:
				return 0;
			default:
				goto bail3;
			}

			break;

		default:
			break;
		}
		break;
#endif
	case LRS_WAITING_DNS:
		/*
		 * we are under PENDING_TIMEOUT_SENT_CLIENT_HANDSHAKE
		 * timeout protection set in client-handshake.c
		 */
		if (!lws_client_connect_2_dnsreq(wsi)) {
			/* closed */
			lwsl_client("closed\n");
			return -1;
		}

		/* either still pending connection, or changed mode */
		return 0;

	case LRS_WAITING_CONNECT:

		/*
		 * we are under PENDING_TIMEOUT_SENT_CLIENT_HANDSHAKE
		 * timeout protection set in client-handshake.c
		 */
		if (pollfd->revents & LWS_POLLOUT)
			lws_client_connect_3_connect(wsi, NULL, NULL, 0, NULL);
		break;

#if defined(LWS_WITH_TLS)
	case LRS_WAITING_SSL:

		if (wsi->tls.use_ssl & LCCSCF_USE_SSL) {
			n = lws_ssl_client_connect2(wsi, erbuf, sizeof(erbuf));
			if (!n)
				return 0;
			if (n < 0) {
				cce = erbuf;
				goto bail3;
			}
		} else
			wsi->tls.ssl = NULL;
#endif /* LWS_WITH_TLS */

		/* fallthru */

#if defined(LWS_WITH_SOCKS5)
start_ws_handshake:
#endif
		lwsi_set_state(wsi, LRS_MQTTC_IDLE);
		lws_set_timeout(wsi, PENDING_TIMEOUT_AWAITING_CLIENT_HS_SEND,
				(int)context->timeout_secs);

		/* fallthru */

	case LRS_MQTTC_IDLE:
		/*
		 * we should be ready to send out MQTT CONNECT
		 */
		lwsl_info("%s: %s: Transport established, send out CONNECT\n",
				__func__, lws_wsi_tag(wsi));
		if (lws_change_pollfd(wsi, LWS_POLLOUT, 0))
			return -1;
		if (!lws_mqtt_client_send_connect(wsi)) {
			lwsl_err("%s: Unable to send MQTT CONNECT\n", __func__);
			return -1;
		}
		if (lws_change_pollfd(wsi, 0, LWS_POLLIN))
			return -1;

		lwsi_set_state(wsi, LRS_MQTTC_AWAIT_CONNACK);
		return 0;

	case LRS_ESTABLISHED:
	case LRS_MQTTC_AWAIT_CONNACK:
		buffered = 0;
		ebuf.token = pt->serv_buf;
		ebuf.len = (int)wsi->a.context->pt_serv_buf_size;

		if ((unsigned int)ebuf.len > wsi->a.context->pt_serv_buf_size)
			ebuf.len = (int)wsi->a.context->pt_serv_buf_size;

		if ((int)pending > ebuf.len)
			pending = (char)ebuf.len;

		ebuf.len = lws_ssl_capable_read(wsi, ebuf.token,
						(unsigned int)(pending ? pending :
						ebuf.len));
		switch (ebuf.len) {
		case 0:
			lwsl_info("%s: zero length read\n",
				  __func__);
			goto fail;
		case LWS_SSL_CAPABLE_MORE_SERVICE:
			lwsl_info("SSL Capable more service\n");
			return 0;
		case LWS_SSL_CAPABLE_ERROR:
			lwsl_info("%s: LWS_SSL_CAPABLE_ERROR\n",
					__func__);
			goto fail;
		}

		if (ebuf.len < 0)
			n = -1;
		else
			n = lws_read_mqtt(wsi, ebuf.token, (unsigned int)ebuf.len);
		if (n < 0) {
			lwsl_err("%s: Parsing packet failed\n", __func__);
			goto fail;
		}

		m = ebuf.len - n;
		// lws_buflist_describe(&wsi->buflist, wsi, __func__);
		lwsl_debug("%s: consuming %d / %d\n", __func__, n, ebuf.len);
		if (lws_buflist_aware_finished_consuming(wsi, &ebuf, m,
							 buffered,
							 __func__))
			return -1;

		return 0;

#if defined(LWS_WITH_TLS) || defined(LWS_WITH_SOCKS5)
bail3:
#endif
		lwsl_info("closing conn at LWS_CONNMODE...SERVER_REPLY\n");
		if (cce)
			lwsl_info("reason: %s\n", cce);
		lws_inform_client_conn_fail(wsi, (void *)cce, strlen(cce));

		lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS, "cbail3");
		return -1;

	default:
		break;
	}

	return 0;
fail:
	lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NOSTATUS, "mqtt svc fail");

	return LWS_HPI_RET_WSI_ALREADY_DIED;
}
