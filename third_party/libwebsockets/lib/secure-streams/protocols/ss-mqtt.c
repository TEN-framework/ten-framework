/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2019 - 2022 Andy Green <andy@warmcat.com>
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

static void
secstream_mqtt_cleanup(lws_ss_handle_t *h)
{
	uint32_t i;

	if (h->u.mqtt.heap_baggage) {
		lws_free(h->u.mqtt.heap_baggage);
		h->u.mqtt.heap_baggage = NULL;
	}

	if (h->u.mqtt.sub_info.topic) {
		for (i = 0; i < h->u.mqtt.sub_info.num_topics; i++) {
			if (h->u.mqtt.sub_info.topic[i].name) {
				lws_free((void*)h->u.mqtt.sub_info.topic[i].name);
				h->u.mqtt.sub_info.topic[i].name = NULL;
			}
		}
		lws_free(h->u.mqtt.sub_info.topic);
		h->u.mqtt.sub_info.topic = NULL;
	}
	lws_buflist_destroy_all_segments(&h->u.mqtt.buflist_unacked);
}

static int
secstream_mqtt_subscribe(struct lws *wsi)
{
	size_t used_in, used_out, topic_limit;
	lws_strexp_t exp;
	char* expbuf;
	lws_ss_handle_t *h = (lws_ss_handle_t *)lws_get_opaque_user_data(wsi);

	if (!h || !h->policy)
		return -1;

	if (h->policy->u.mqtt.aws_iot)
		topic_limit = LWS_MQTT_MAX_AWSIOT_TOPICLEN;
	else
		topic_limit = LWS_MQTT_MAX_TOPICLEN;

	if (!h->policy->u.mqtt.subscribe || wsi->mqtt->done_subscribe)
		return 0;

	lws_strexp_init(&exp, (void*)h, lws_ss_exp_cb_metadata, NULL,
			topic_limit);
	/*
	 * Expand with no output first to calculate the size of
	 * expanded string then, allocate new buffer and expand
	 * again with the buffer
	 */
	if (lws_strexp_expand(&exp, h->policy->u.mqtt.subscribe,
			      strlen(h->policy->u.mqtt.subscribe), &used_in,
			      &used_out) != LSTRX_DONE) {
		lwsl_err(
			"%s, failed to expand MQTT subscribe"
			" topic with no output\n",
			__func__);
		return 1;
	}

	expbuf = lws_malloc(used_out + 1, __func__);
	if (!expbuf) {
		lwsl_err(
			 "%s, failed to allocate MQTT subscribe"
			 "topic",
			 __func__);
		return 1;
	}

	lws_strexp_init(&exp, (void*)h, lws_ss_exp_cb_metadata, expbuf,
			used_out + 1);

	if (lws_strexp_expand(&exp, h->policy->u.mqtt.subscribe,
			      strlen(h->policy->u.mqtt.subscribe), &used_in,
			      &used_out) != LSTRX_DONE) {
		lwsl_err("%s, failed to expand MQTT subscribe topic\n",
			 __func__);
		lws_free(expbuf);
		return 1;
	}
	lwsl_notice("%s, expbuf - %s\n", __func__, expbuf);
	h->u.mqtt.sub_top.name = expbuf;

	/*
	 * The policy says to subscribe to something, and we
	 * haven't done it yet.  Do it using the pre-prepared
	 * string-substituted version of the policy string.
	 */

	lwsl_notice("%s: subscribing %s\n", __func__,
		    h->u.mqtt.sub_top.name);

	h->u.mqtt.sub_top.qos = h->policy->u.mqtt.qos;
	memset(&h->u.mqtt.sub_info, 0, sizeof(h->u.mqtt.sub_info));
	h->u.mqtt.sub_info.num_topics = 1;
	h->u.mqtt.sub_info.topic = &h->u.mqtt.sub_top;
	h->u.mqtt.sub_info.topic =
			    lws_malloc(sizeof(lws_mqtt_topic_elem_t), __func__);
	h->u.mqtt.sub_info.topic[0].name = lws_strdup(expbuf);
	h->u.mqtt.sub_info.topic[0].qos = h->policy->u.mqtt.qos;

	if (lws_mqtt_client_send_subcribe(wsi, &h->u.mqtt.sub_info)) {
		lwsl_notice("%s: unable to subscribe", __func__);
		lws_free(expbuf);
		h->u.mqtt.sub_top.name = NULL;
		return -1;
	}
	lws_free(expbuf);
	h->u.mqtt.sub_top.name = NULL;

	/* Expect a SUBACK */
	if (lws_change_pollfd(wsi, 0, LWS_POLLIN)) {
		lwsl_err("%s: Unable to set LWS_POLLIN\n", __func__);
		return -1;
	}
	return 0;
}

static int
secstream_mqtt_publish(struct lws *wsi, uint8_t *buf, size_t buf_len,
			uint32_t payload_len, const char* topic,
			lws_mqtt_qos_levels_t qos,  uint8_t retain, uint8_t dup,
			int f)
{
	lws_ss_handle_t *h = (lws_ss_handle_t *)lws_get_opaque_user_data(wsi);
	size_t used_in, used_out, topic_limit;
	lws_strexp_t exp;
	char *expbuf;
	lws_mqtt_publish_param_t mqpp;

	if (h->policy->u.mqtt.aws_iot)
		topic_limit = LWS_MQTT_MAX_AWSIOT_TOPICLEN;
	else
		topic_limit = LWS_MQTT_MAX_TOPICLEN;

	memset(&mqpp, 0, sizeof(mqpp));

	lws_strexp_init(&exp, h, lws_ss_exp_cb_metadata, NULL,
			topic_limit);

	if (lws_strexp_expand(&exp, topic, strlen(topic), &used_in,
			      &used_out) != LSTRX_DONE) {
		lwsl_err("%s, failed to expand MQTT publish"
			 " topic with no output\n", __func__);
		return 1;
	}
	expbuf = lws_malloc(used_out + 1, __func__);
	if (!expbuf) {
		lwsl_err("%s, failed to allocate MQTT publish topic",
			  __func__);
		return 1;
	}

	lws_strexp_init(&exp, (void *)h, lws_ss_exp_cb_metadata, expbuf,
			used_out + 1);

	if (lws_strexp_expand(&exp, topic, strlen(topic), &used_in,
			      &used_out) != LSTRX_DONE) {
		lws_free(expbuf);
		return 1;
	}
	lwsl_notice("%s, expbuf - %s\n", __func__, expbuf);
	mqpp.topic = (char *)expbuf;

	mqpp.topic_len = (uint16_t)strlen(mqpp.topic);
	mqpp.packet_id = (uint16_t)(h->txord - 1);
	mqpp.qos = qos;
	mqpp.retain = !!retain;
	mqpp.payload = buf;
	mqpp.dup = !!dup;
	if (payload_len)
		mqpp.payload_len = payload_len;
	else
		mqpp.payload_len = (uint32_t)buf_len;

	lwsl_notice("%s: payload len %d\n", __func__,
		    (int)mqpp.payload_len);

	if (lws_mqtt_client_send_publish(wsi, &mqpp,
					 (const char *)buf,
					 (uint32_t)buf_len,
					 f & LWSSS_FLAG_EOM)) {
		lwsl_notice("%s: failed to publish\n", __func__);
		lws_free(expbuf);
		return -1;
	}
	lws_free(expbuf);

	if ((mqpp.qos == QOS1 || mqpp.qos == QOS2) && buf_len > 0) {
		if (lws_buflist_append_segment(&h->u.mqtt.buflist_unacked,
					       buf, buf_len) < 0) {
			lwsl_notice("%s: failed to store unacked\n", __func__);
			return -1;
		}
	}

	return 0;
}

static int
secstream_mqtt_birth(struct lws *wsi, uint8_t *buf, size_t buflen) {
	lws_strexp_t exp;
	size_t used_in, used_out = 0;
	lws_ss_handle_t *h = (lws_ss_handle_t *)lws_get_opaque_user_data(wsi);

	if (h->policy->u.mqtt.birth_message) {
		lws_strexp_init(&exp, h, lws_ss_exp_cb_metadata,
				(char *)buf, buflen);
		if (lws_strexp_expand(&exp, h->policy->u.mqtt.birth_message,
		    strlen(h->policy->u.mqtt.birth_message),
			&used_in, &used_out) != LSTRX_DONE) {
			return 1;
		}
	}
	wsi->mqtt->inside_birth = 1;
	return secstream_mqtt_publish(wsi, buf,
				      used_out, 0, h->policy->u.mqtt.birth_topic,
				      h->policy->u.mqtt.birth_qos,
				      h->policy->u.mqtt.birth_retain, 0,
				      LWSSS_FLAG_EOM);
}

static int
secstream_mqtt_resend(struct lws *wsi, uint8_t *buf) {
	uint8_t *buffered;
	size_t len;
	int f = 0, r;
	lws_ss_handle_t *h = (lws_ss_handle_t *)lws_get_opaque_user_data(wsi);

	len = lws_buflist_next_segment_len(&h->u.mqtt.buflist_unacked,
					   &buffered);

	if (h->u.mqtt.unacked_size <= len)
		f |= LWSSS_FLAG_EOM;

	if (!len) {
		/* when the message does not have payload */
		buffered = buf;
	} else {
		h->u.mqtt.unacked_size -= (uint32_t)len;
	}

	if (wsi->mqtt->inside_birth) {
		r = secstream_mqtt_publish(wsi, buffered, len, 0,
					   h->policy->u.mqtt.birth_topic,
					   h->policy->u.mqtt.birth_qos,
					   h->policy->u.mqtt.birth_retain,
					   1, f);
	} else {
		r = secstream_mqtt_publish(wsi, buffered, len,
					   (uint32_t)h->writeable_len,
					   h->policy->u.mqtt.topic,
					   h->policy->u.mqtt.qos,
					   h->policy->u.mqtt.retain, 1, f);
	}
	if (len)
		lws_buflist_use_segment(&h->u.mqtt.buflist_unacked, len);

	if (r) {
		lws_buflist_destroy_all_segments(&h->u.mqtt.buflist_unacked);
		h->u.mqtt.retry_count = h->u.mqtt.send_unacked = 0;

		if (wsi->mqtt->inside_birth) {
			lwsl_err("%s: %s: failed to send Birth\n", __func__,
				 lws_ss_tag(h));
			return -1;
		} else {
			r = lws_ss_event_helper(h, LWSSSCS_QOS_NACK_REMOTE);
			if (r != LWSSSSRET_OK)
				return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
		}
	}
	return 0;
}

static char *
expand_metadata(lws_ss_handle_t *h, const char* str, const char* post, size_t max_len)
{
	lws_strexp_t exp;
	char *expbuf = NULL;
	size_t used_in = 0, used_out = 0, post_len = 0;

	memset(&exp, 0, sizeof(exp));

	if (post)
		post_len = strlen(post);

	if (post_len > max_len)
		return NULL;

	lws_strexp_init(&exp, (void *)h, lws_ss_exp_cb_metadata, NULL,
			max_len - post_len);

	if (lws_strexp_expand(&exp, str, strlen(str), &used_in,
			      &used_out) != LSTRX_DONE) {
		lwsl_err("%s, failed to expand %s", __func__, str);

		return NULL;
	}

	expbuf = lws_malloc(used_out + 1 + post_len, __func__);
	if (!expbuf) {
		lwsl_err("%s, failed to allocate str_exp for %s", __func__, str);

		return NULL;
	}

	lws_strexp_init(&exp, (void*)h, lws_ss_exp_cb_metadata, expbuf,
			used_out + 1 + post_len);

	if (lws_strexp_expand(&exp, str, strlen(str), &used_in,
			      &used_out) != LSTRX_DONE) {
		lwsl_err("%s, failed to expand str_exp %s\n", __func__, str);
		lws_free(expbuf);

		return NULL;
	}
	if (post)
		strcat(expbuf, post);

	return expbuf;
}

static lws_mqtt_match_topic_return_t
secstream_mqtt_is_shadow_matched(struct lws *wsi, const char *topic)
{
	lws_ss_handle_t *h = (lws_ss_handle_t *)lws_get_opaque_user_data(wsi);
	const char *match[] = { LWS_MQTT_SHADOW_UNNAMED_TOPIC_MATCH,
				LWS_MQTT_SHADOW_NAMED_TOPIC_MATCH };
	char *expbuf = NULL;
	unsigned int i = 0;
	lws_mqtt_match_topic_return_t ret = LMMTR_TOPIC_NOMATCH;

	if (!topic)
		return LMMTR_TOPIC_MATCH_ERROR;

	expbuf = expand_metadata(h, topic, NULL, LWS_MQTT_MAX_AWSIOT_TOPICLEN);
	if (!expbuf) {
		lwsl_wsi_warn(wsi, "Failed to expand Shadow topic");

		return LMMTR_TOPIC_MATCH_ERROR;
	}
	for (i = 0; i < (sizeof(match) / sizeof(match[0])); i++) {
		if (lws_mqtt_is_topic_matched(
				match[i], expbuf) == LMMTR_TOPIC_MATCH) {
			ret = LMMTR_TOPIC_MATCH;
			break;
		}
	}
	lws_free(expbuf);

	return ret;
}

static void
secstream_mqtt_shadow_cleanup(struct lws *wsi)
{
	lws_ss_handle_t *h = (lws_ss_handle_t *)lws_get_opaque_user_data(wsi);
	uint32_t i = 0;

	for (i = 0; i < h->u.mqtt.shadow_sub.num_topics; i++)
		lws_free((void *)h->u.mqtt.shadow_sub.topic[i].name);

	h->u.mqtt.shadow_sub.num_topics = 0;

	if (h->u.mqtt.shadow_sub.topic) {
		lws_free(h->u.mqtt.shadow_sub.topic);
		h->u.mqtt.shadow_sub.topic = NULL;
	}
}

static lws_ss_state_return_t
secstream_mqtt_shadow_unsubscribe(struct lws *wsi)
{
	lws_ss_handle_t *h = (lws_ss_handle_t *)lws_get_opaque_user_data(wsi);

	if (h->u.mqtt.shadow_sub.num_topics == 0) {
		wsi->mqtt->send_shadow_unsubscribe = 0;
		wsi->mqtt->inside_shadow = 0;
		wsi->mqtt->done_shadow_subscribe = 0;

		return LWSSSSRET_OK;
	}

	if (lws_mqtt_client_send_unsubcribe(wsi, &h->u.mqtt.shadow_sub)) {
		lwsl_wsi_err(wsi, "Failed to send MQTT unsubsribe");

		return LWSSSSRET_DISCONNECT_ME;
	}
	/* Expect a UNSUBACK */
	if (lws_change_pollfd(wsi, 0, LWS_POLLIN)) {
		lwsl_wsi_err(wsi, "Unable to set LWS_POLLIN");

		return LWSSSSRET_DISCONNECT_ME;
	}
	wsi->mqtt->send_shadow_unsubscribe = 0;

	return LWSSSSRET_OK;
}

static int
secstream_mqtt_shadow_subscribe(struct lws *wsi)
{
	lws_ss_handle_t *h = (lws_ss_handle_t *)lws_get_opaque_user_data(wsi);
	char* expbuf = NULL;
	const char *suffixes[] = { LWS_MQTT_SHADOW_RESP_ACCEPTED_STR,
				   LWS_MQTT_SHADOW_RESP_REJECTED_STR };
	unsigned int i, suffixes_len = sizeof(suffixes) / sizeof(suffixes[0]);

	if (!h->policy->u.mqtt.topic || wsi->mqtt->inside_shadow)
		return 0;

	if (h->u.mqtt.shadow_sub.num_topics > 0)
		secstream_mqtt_shadow_cleanup(wsi);

	memset(&h->u.mqtt.shadow_sub, 0, sizeof(lws_mqtt_subscribe_param_t));
	h->u.mqtt.shadow_sub.topic = lws_malloc(
			sizeof(lws_mqtt_topic_elem_t) * suffixes_len, __func__);
	if (!h->u.mqtt.shadow_sub.topic) {
		lwsl_ss_err(h, "Failed to allocate Shadow topics");
		return -1;
	}
	h->u.mqtt.shadow_sub.num_topics = suffixes_len;
	for (i = 0; i < suffixes_len; i++) {
		expbuf = expand_metadata(h, h->policy->u.mqtt.topic, suffixes[i],
					 LWS_MQTT_MAX_AWSIOT_TOPICLEN);
		if (!expbuf) {
			lwsl_ss_err(h, "Failed to allocate Shadow topic");
			secstream_mqtt_shadow_cleanup(wsi);

			return -1;
		}
		h->u.mqtt.shadow_sub.topic[i].name = expbuf;
		h->u.mqtt.shadow_sub.topic[i].qos = h->policy->u.mqtt.qos;
	}
	h->u.mqtt.shadow_sub.packet_id = (uint16_t)(h->txord - 1);

	if (lws_mqtt_client_send_subcribe(wsi, &h->u.mqtt.shadow_sub)) {
		lwsl_wsi_notice(wsi, "Unable to subscribe Shadow topics");

		return 0;
	}

	/* Expect a SUBACK */
	if (lws_change_pollfd(wsi, 0, LWS_POLLIN)) {
		lwsl_err("%s: Unable to set LWS_POLLIN\n", __func__);
		return -1;
	}
	wsi->mqtt->inside_shadow = 1;

	return 0;
}

static int
secstream_mqtt(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	     void *in, size_t len)
{
	lws_ss_handle_t *h = (lws_ss_handle_t *)lws_get_opaque_user_data(wsi);
	size_t used_in = 0, used_out = 0, topic_len = 0;
	lws_mqtt_publish_param_t *pmqpp = NULL;
	lws_ss_state_return_t r = LWSSSSRET_OK;
	uint8_t buf[LWS_PRE + 1400];
	size_t buflen = sizeof(buf) - LWS_PRE;
	lws_ss_metadata_t *omd = NULL;
	char *sub_topic = NULL;
	lws_strexp_t exp;
	int f = 0;

	memset(buf, 0, sizeof(buf));
	memset(&exp, 0, sizeof(exp));

	switch (reason) {

	/* because we are protocols[0] ... */
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_info("%s: CLIENT_CONNECTION_ERROR: %s\n", __func__,
			 in ? (char *)in : "(null)");
		if (!h)
			break;

#if defined(LWS_WITH_CONMON)
		lws_conmon_ss_json(h);
#endif

		r = lws_ss_event_helper(h, LWSSSCS_UNREACHABLE);
		h->wsi = NULL;

		secstream_mqtt_cleanup(h);

		if (r == LWSSSSRET_DESTROY_ME)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);

		r = lws_ss_backoff(h);
		if (r != LWSSSSRET_OK)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);

		break;

	case LWS_CALLBACK_MQTT_CLIENT_CLOSED:
		if (!h)
			break;
		lws_sul_cancel(&h->sul_timeout);
#if defined(LWS_WITH_CONMON)
		lws_conmon_ss_json(h);
#endif
		if (h->ss_dangling_connected)
			r = lws_ss_event_helper(h, LWSSSCS_DISCONNECTED);
		else
			r = lws_ss_event_helper(h, LWSSSCS_UNREACHABLE);
		if (h->wsi)
			lws_set_opaque_user_data(h->wsi, NULL);
		h->wsi = NULL;

		secstream_mqtt_cleanup(h);

		if (r)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);

		if (h->policy && !(h->policy->flags & LWSSSPOLF_OPPORTUNISTIC) &&
		    !h->txn_ok && !wsi->a.context->being_destroyed) {
			r = lws_ss_backoff(h);
			if (r != LWSSSSRET_OK)
				return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
		}
		break;

	case LWS_CALLBACK_MQTT_CLIENT_ESTABLISHED:
		/*
		 * Make sure the handle wsi points to the stream wsi not the
		 * original nwsi, in the case it was migrated
		 */
		h->wsi = wsi;
		h->retry = 0;
		h->seqstate = SSSEQ_CONNECTED;

		if (h->policy->u.mqtt.birth_topic &&
		    !wsi->mqtt->done_birth) {
			struct lws *nwsi = lws_get_network_wsi(wsi);
			lws_start_foreach_ll(struct lws *, w, nwsi->mux.child_list) {
				if (w != wsi &&
					(w->mqtt->done_birth || w->mqtt->inside_birth)) {
					/*
					 * If any Birth was sent out or
					 * is pending on other stream,
					 * skip sending Birth.
					 */
					wsi->mqtt->done_birth = 1;
					break;
				}
			} lws_end_foreach_ll(w, mux.sibling_list);
		}

		if (!h->policy->u.mqtt.subscribe ||
		    !h->policy->u.mqtt.subscribe[0]) {
			/*
			 * If subscribe is empty in the policy, then,
			 * skip sending SUBSCRIBE and signal the user
			 * application.
			 */
			wsi->mqtt->done_subscribe = 1;
		} else if (!h->policy->u.mqtt.clean_start &&
			   wsi->mqtt->session_resumed) {
			wsi->mqtt->inside_resume_session = 1;
			/*
			 * If the previous session is resumed and Server has
			 * stored session, then, do not subscribe.
			 */
			if (!secstream_mqtt_subscribe(wsi))
				wsi->mqtt->done_subscribe = 1;
			wsi->mqtt->inside_resume_session = 0;
		} else if (h->policy->u.mqtt.subscribe &&
			   !wsi->mqtt->done_subscribe) {
			/*
			 * If a subscribe is pending on the stream, then make
			 * sure the SUBSCRIBE is done before signaling the
			 * user application.
			 */
			lws_callback_on_writable(wsi);
			break;
		}

		if (h->policy->u.mqtt.birth_topic &&
		    !wsi->mqtt->done_birth) {
			/*
			 * If a Birth is pending on the stream, then make
			 * sure the Birth is done before signaling the
			 * user application.
			 */
			lws_callback_on_writable(wsi);
			break;
		}
		lws_sul_cancel(&h->sul);
#if defined(LWS_WITH_SYS_METRICS)
		/*
		 * If any hanging caliper measurement, dump it, and free any tags
		 */
		lws_metrics_caliper_report_hist(h->cal_txn, (struct lws *)NULL);
#endif
		r = lws_ss_event_helper(h, LWSSSCS_CONNECTED);
		if (r != LWSSSSRET_OK)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
		if (h->policy->u.mqtt.topic)
			lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_MQTT_CLIENT_RX:
		// lwsl_user("LWS_CALLBACK_CLIENT_RECEIVE: read %d\n", (int)len);
		if (!h || !h->info.rx)
			return 0;

		pmqpp = (lws_mqtt_publish_param_t *)in;

		f = 0;
		if (!pmqpp->payload_pos)
			f |= LWSSS_FLAG_SOM;
		if (pmqpp->payload_pos + len == pmqpp->payload_len)
			f |= LWSSS_FLAG_EOM;

		h->subseq = 1;

		if (wsi->mqtt->inside_shadow) {
			/*
			 * When Shadow is used, the stream receives multiple
			 * topics including Shadow response, set received
			 * topic on the metadata
			 */
			lws_strexp_init(&exp, (void*)h, lws_ss_exp_cb_metadata,
					NULL, (size_t)-1);

			if (lws_strexp_expand(&exp, h->policy->u.mqtt.subscribe,
					strlen(h->policy->u.mqtt.subscribe),
					&used_in, &used_out) != LSTRX_DONE) {
				lwsl_err("%s, failed to expand subscribe topic",
					 __func__);
				return -1;
			}
			omd = lws_ss_get_handle_metadata(h, exp.name);

			if (!omd) {
				lwsl_err("%s, failed to find metadata for subscribe",
					 __func__);
				return -1;
			}
			sub_topic = omd->value__may_own_heap;
			topic_len = omd->length;

			_lws_ss_set_metadata(omd, exp.name,
					     (const void *)pmqpp->topic,
					     pmqpp->topic_len);
		}

		r = h->info.rx(ss_to_userobj(h), (const uint8_t *)pmqpp->payload,
			   len, f);

		if (wsi->mqtt->inside_shadow)
			_lws_ss_set_metadata(omd, exp.name, &sub_topic,
					     topic_len);

		if (r != LWSSSSRET_OK)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);

		if (wsi->mqtt->inside_shadow) {
			size_t acc_n = strlen(LWS_MQTT_SHADOW_RESP_ACCEPTED_STR);
			size_t rej_n = strlen(LWS_MQTT_SHADOW_RESP_REJECTED_STR);
			uint32_t i;

			for (i = 0; i < h->u.mqtt.shadow_sub.num_topics; i++) {
				/*
				 * received response ('/accepted' or 'rejected')
				 * and clean up Shadow operation
				 */
				if (strncmp(h->u.mqtt.shadow_sub.topic[i].name,
					    pmqpp->topic, pmqpp->topic_len) ||
				    (strlen(pmqpp->topic) < acc_n ||
				     strlen(pmqpp->topic) < rej_n))
					continue;

				if (!strcmp(pmqpp->topic +
						(strlen(pmqpp->topic) - acc_n),
					 	LWS_MQTT_SHADOW_RESP_ACCEPTED_STR) ||
				    !strcmp(pmqpp->topic +
						(strlen(pmqpp->topic) - rej_n),
						 LWS_MQTT_SHADOW_RESP_REJECTED_STR)) {
					lws_sul_cancel(&wsi->mqtt->sul_shadow_wait);
					wsi->mqtt->send_shadow_unsubscribe = 1;
					lws_callback_on_writable(wsi);

					return 0;
				}
			}
		}
		return 0; /* don't passthru */

	case LWS_CALLBACK_MQTT_SUBSCRIBED:
		if (wsi->mqtt->inside_shadow) {
			wsi->mqtt->done_shadow_subscribe = 1;
			lws_callback_on_writable(wsi);

			return 0;
		}
		/*
		 * Stream demanded a subscribe without a Birth while connecting, once
		 * done notify CONNECTED event to the application.
		 */
		if (!wsi->mqtt->done_subscribe && !h->policy->u.mqtt.birth_topic) {
			lws_sul_cancel(&h->sul);
			r = lws_ss_event_helper(h, LWSSSCS_CONNECTED);
			if (r != LWSSSSRET_OK)
				return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
		}
		wsi->mqtt->done_subscribe = 1;
		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_MQTT_ACK:
		lws_sul_cancel(&h->sul_timeout);
		if (h->u.mqtt.send_unacked) {
			lws_buflist_destroy_all_segments(&h->u.mqtt.buflist_unacked);
			h->u.mqtt.retry_count = h->u.mqtt.send_unacked = 0;
		}

		if (wsi->mqtt->inside_birth) {
			/*
			 * Skip LWSSSCS_QOS_ACK_REMOTE for a Birth, notify
			 * CONNECTED event to the application.
			 */
			wsi->mqtt->inside_birth = 0;
			wsi->mqtt->done_birth = 1;
			r = lws_ss_event_helper(h, LWSSSCS_CONNECTED);
			if (r != LWSSSSRET_OK)
				return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
			lws_callback_on_writable(wsi);
			break;
		}
		r = lws_ss_event_helper(h, LWSSSCS_QOS_ACK_REMOTE);
		if (r != LWSSSSRET_OK)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
		break;

	case LWS_CALLBACK_MQTT_RESEND:
		lws_sul_cancel(&h->sul_timeout);
		if (h->u.mqtt.retry_count++ < LWS_MQTT_MAX_PUBLISH_RETRY) {
			h->u.mqtt.unacked_size =
				(uint32_t)lws_buflist_total_len(&h->u.mqtt.buflist_unacked);
			if (h->u.mqtt.unacked_size) {
				lwsl_notice("%s: %s: resend unacked message (%d/%d) \n",
					    __func__, lws_ss_tag(h),
					    h->u.mqtt.retry_count,
					    LWS_MQTT_MAX_PUBLISH_RETRY);
				h->u.mqtt.send_unacked = 1;
				lws_callback_on_writable(wsi);
				break;
			}
		}

		lws_buflist_destroy_all_segments(&h->u.mqtt.buflist_unacked);
		h->u.mqtt.retry_count = h->u.mqtt.send_unacked = 0;

		if (wsi->mqtt->inside_birth) {
			lwsl_err("%s: %s: failed to send Birth\n", __func__,
				 lws_ss_tag(h));
			return -1;
		}

		r = lws_ss_event_helper(h, LWSSSCS_QOS_NACK_REMOTE);
		if (r != LWSSSSRET_OK)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
		break;

	case LWS_CALLBACK_MQTT_CLIENT_WRITEABLE:
	{
		if (!h || !h->info.tx)
			return 0;
		lwsl_notice("%s: %s: WRITEABLE\n", __func__, lws_ss_tag(h));

		if (h->seqstate != SSSEQ_CONNECTED) {
			lwsl_warn("%s: seqstate %d\n", __func__, h->seqstate);
			break;
		}

		if (!wsi->mqtt->done_subscribe && h->policy->u.mqtt.subscribe)
			return secstream_mqtt_subscribe(wsi);

		if (h->u.mqtt.send_unacked)
			return secstream_mqtt_resend(wsi, buf + LWS_PRE);

		if (!wsi->mqtt->done_birth && h->policy->u.mqtt.birth_topic)
			return secstream_mqtt_birth(wsi, buf + LWS_PRE, buflen);

		if (h->policy->u.mqtt.aws_iot) {
			if (secstream_mqtt_is_shadow_matched(wsi,
			    h->policy->u.mqtt.topic) == LMMTR_TOPIC_MATCH) {
				if (!wsi->mqtt->done_shadow_subscribe)
					return secstream_mqtt_shadow_subscribe(wsi);
				if (wsi->mqtt->send_shadow_unsubscribe)
					return secstream_mqtt_shadow_unsubscribe(wsi);
			}
		}

		r = h->info.tx(ss_to_userobj(h),  h->txord++,  buf + LWS_PRE,
			       &buflen, &f);

		if (r == LWSSSSRET_TX_DONT_SEND) {
			if (wsi->mqtt->done_shadow_subscribe) {
				return secstream_mqtt_shadow_unsubscribe(wsi);
			}
			return 0;
		}

		if (r == LWSSSSRET_DISCONNECT_ME) {
			lws_mqtt_subscribe_param_t lmsp;
			if (h->u.mqtt.sub_info.num_topics) {
				lmsp.num_topics = h->u.mqtt.sub_info.num_topics;
				lmsp.topic = h->u.mqtt.sub_info.topic;
				lmsp.packet_id = (uint16_t)(h->txord - 1);
				if (lws_mqtt_client_send_unsubcribe(wsi,
								    &lmsp)) {
					lwsl_err("%s, failed to send"
					         " MQTT unsubsribe", __func__);
					return -1;
				}
				return 0;
			}
		}

		if (r < 0)
			return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);

		if (secstream_mqtt_publish(wsi, buf + LWS_PRE, buflen,
					   (uint32_t)h->writeable_len,
					   h->policy->u.mqtt.topic,
					   h->policy->u.mqtt.qos,
					   h->policy->u.mqtt.retain, 0, f) != 0) {
			r = lws_ss_event_helper(h, LWSSSCS_QOS_NACK_REMOTE);
			if (r != LWSSSSRET_OK)
				return _lws_ss_handle_state_ret_CAN_DESTROY_HANDLE(r, wsi, &h);
		}
		return 0;
	}

	case LWS_CALLBACK_MQTT_UNSUBSCRIBED:
	{
		struct lws *nwsi = lws_get_network_wsi(wsi);

		if (wsi->mqtt->inside_shadow) {
			secstream_mqtt_shadow_cleanup(wsi);
			wsi->mqtt->inside_shadow = 0;
			wsi->mqtt->done_shadow_subscribe = 0;
			break;
		}
		if (nwsi && (nwsi->mux.child_count == 1))
			lws_mqtt_client_send_disconnect(nwsi);
		return -1;
	}

	case LWS_CALLBACK_MQTT_UNSUBSCRIBE_TIMEOUT:
		if (!wsi->mqtt)
			return -1;

		if (wsi->mqtt->inside_shadow) {
			secstream_mqtt_shadow_cleanup(wsi);
			wsi->mqtt->inside_shadow = 0;
			wsi->mqtt->done_shadow_subscribe = 0;
			lwsl_warn("%s: %s: Unsubscribe (Shadow) timeout.\n",
				  __func__, lws_ss_tag(h));
			break;
		}

		if (wsi->mqtt->inside_unsubscribe) {
			lwsl_warn("%s: %s: Unsubscribe timeout.\n", __func__,
				  lws_ss_tag(h));
			return -1;
		}
		break;

	case LWS_CALLBACK_MQTT_SHADOW_TIMEOUT:
		if (!wsi->mqtt)
			return -1;

		if (wsi->mqtt->inside_shadow) {
			lwsl_warn("%s: %s: Shadow timeout.\n", __func__,
				  lws_ss_tag(h));
			wsi->mqtt->send_shadow_unsubscribe = 1;
			lws_callback_on_writable(wsi);
		}
		break;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

const struct lws_protocols protocol_secstream_mqtt = {
	"lws-secstream-mqtt",
	secstream_mqtt,
	0, 0, 0, NULL, 0
};
/*
 * Munge connect info according to protocol-specific considerations... this
 * usually means interpreting aux in a protocol-specific way and using the
 * pieces at connection setup time, eg, http url pieces.
 *
 * len bytes of buf can be used for things with scope until after the actual
 * connect.
 *
 * For ws, protocol aux is <url path>;<ws subprotocol name>
 */

enum {
	SSCMM_STRSUB_WILL_TOPIC,
	SSCMM_STRSUB_WILL_MESSAGE,
	SSCMM_STRSUB_SUBSCRIBE,
	SSCMM_STRSUB_TOPIC,
	SSCMM_STRSUB_BIRTH_TOPIC,
	SSCMM_STRSUB_BIRTH_MESSAGE
};

static int
secstream_connect_munge_mqtt(lws_ss_handle_t *h, char *buf, size_t len,
			     struct lws_client_connect_info *i,
			     union lws_ss_contemp *ct)
{
	const char *sources[6] = {
		/* we're going to string-substitute these before use */
		h->policy->u.mqtt.will_topic,
		h->policy->u.mqtt.will_message,
		h->policy->u.mqtt.subscribe,
		h->policy->u.mqtt.topic,
		h->policy->u.mqtt.birth_topic,
		h->policy->u.mqtt.birth_message
	};
	size_t used_in, olen[6] = { 0, 0, 0, 0, 0, 0 }, tot = 0;
	lws_strexp_t exp;
	char *ps[6];
	uint8_t *p = NULL;
	int n = -1;
	size_t blen;
	lws_system_blob_t *b = NULL;

	memset(&ct->ccp, 0, sizeof(ct->ccp));
	b = lws_system_get_blob(i->context,
				LWS_SYSBLOB_TYPE_MQTT_CLIENT_ID, 0);

	/* If LWS_SYSBLOB_TYPE_MQTT_CLIENT_ID is set */
	if (b && (blen = lws_system_blob_get_size(b))) {
		if (blen > LWS_MQTT_MAX_CIDLEN) {
			lwsl_err("%s - Client ID too long.\n",
				 __func__);
			return -1;
		}
		p = (uint8_t *)lws_zalloc(blen+1, __func__);
		if (!p)
			return -1;
		n = lws_system_blob_get(b, p, &blen, 0);
		if (n) {
			ct->ccp.client_id = NULL;
		} else {
			ct->ccp.client_id = (const char *)p;
			lwsl_notice("%s - Client ID = %s\n",
				    __func__, ct->ccp.client_id);
		}
	} else {
		/* Default (Random) client ID */
		ct->ccp.client_id = NULL;
	}

	b = lws_system_get_blob(i->context,
				LWS_SYSBLOB_TYPE_MQTT_USERNAME, 0);

	/* If LWS_SYSBLOB_TYPE_MQTT_USERNAME is set */
	if (b && (blen = lws_system_blob_get_size(b))) {
		p = (uint8_t *)lws_zalloc(blen+1, __func__);
		if (!p)
			return -1;
		n = lws_system_blob_get(b, p, &blen, 0);
		if (n) {
			ct->ccp.username = NULL;
		} else {
			ct->ccp.username = (const char *)p;
			lwsl_notice("%s - Username ID = %s\n",
				    __func__, ct->ccp.username);
		}
	}

	b = lws_system_get_blob(i->context,
				LWS_SYSBLOB_TYPE_MQTT_PASSWORD, 0);

	/* If LWS_SYSBLOB_TYPE_MQTT_PASSWORD is set */
	if (b && (blen = lws_system_blob_get_size(b))) {
		p = (uint8_t *)lws_zalloc(blen+1, __func__);
		if (!p)
			return -1;
		n = lws_system_blob_get(b, p, &blen, 0);
		if (n) {
			ct->ccp.password = NULL;
		} else {
			ct->ccp.password = (const char *)p;
			lwsl_notice("%s - Password ID = %s\n",
				    __func__, ct->ccp.password);
		}
	}

	ct->ccp.keep_alive		= h->policy->u.mqtt.keep_alive;
	ct->ccp.clean_start		= (h->policy->u.mqtt.clean_start & 1u);
	ct->ccp.will_param.qos		= h->policy->u.mqtt.will_qos;
	ct->ccp.will_param.retain	= h->policy->u.mqtt.will_retain;
	ct->ccp.birth_param.qos		= h->policy->u.mqtt.birth_qos;
	ct->ccp.birth_param.retain	= h->policy->u.mqtt.birth_retain;
	ct->ccp.aws_iot			= h->policy->u.mqtt.aws_iot;
	h->u.mqtt.topic_qos.qos		= h->policy->u.mqtt.qos;

	/*
	 * We're going to string-substitute several of these parameters, which
	 * have unknown, possibly large size.  And, as their usage is deferred
	 * inside the asynchronous lifetime of the MQTT connection, they need
	 * to live on the heap.
	 *
	 * Notice these allocations at h->u.mqtt.heap_baggage belong to the
	 * underlying MQTT stream lifetime, not the logical SS lifetime, and
	 * are destroyed if present at connection error or close of the
	 * underlying connection.
	 *
	 *
	 * First, compute the length of each without producing strsubst output,
	 * and keep a running total.
	 */

	for (n = 0; n < (int)LWS_ARRAY_SIZE(sources); n++) {
		if (!sources[n])
			continue;

		lws_strexp_init(&exp, (void *)h, lws_ss_exp_cb_metadata,
				NULL, (size_t)-1);
		if (lws_strexp_expand(&exp, sources[n], strlen(sources[n]),
				      &used_in, &olen[n]) != LSTRX_DONE) {
			lwsl_err("%s: failed to subsitute %s\n", __func__,
					sources[n]);
			return 1;
		}
		tot += olen[n] + 1;
	}

	/*
	 * Then, allocate enough space on the heap for the total of the
	 * substituted results
	 */

	h->u.mqtt.heap_baggage = lws_malloc(tot, __func__);
	if (!h->u.mqtt.heap_baggage)
		return 1;

	/*
	 * Finally, issue the subsitutions one after the other into the single
	 * allocated result buffer and prepare pointers into them
	 */

	p = h->u.mqtt.heap_baggage;
	for (n = 0; n < (int)LWS_ARRAY_SIZE(sources); n++) {
		lws_strexp_init(&exp, (void *)h, lws_ss_exp_cb_metadata,
				(char *)p, (size_t)-1);
		if (!sources[n]) {
			ps[n] = NULL;
			continue;
		}
		ps[n] = (char *)p;
		if (lws_strexp_expand(&exp, sources[n], strlen(sources[n]),
				      &used_in, &olen[n]) != LSTRX_DONE)
			return 1;

		p += olen[n] + 1;
	}

	/*
	 * Point the guys who want the substituted content at the substituted
	 * strings
	 */

	ct->ccp.will_param.topic	= ps[SSCMM_STRSUB_WILL_TOPIC];
	ct->ccp.will_param.message	= ps[SSCMM_STRSUB_WILL_MESSAGE];
	h->u.mqtt.subscribe_to		= ps[SSCMM_STRSUB_SUBSCRIBE];
	h->u.mqtt.subscribe_to_len	= olen[SSCMM_STRSUB_SUBSCRIBE];
	h->u.mqtt.topic_qos.name	= ps[SSCMM_STRSUB_TOPIC];
	ct->ccp.birth_param.topic	= ps[SSCMM_STRSUB_BIRTH_TOPIC];
	ct->ccp.birth_param.message	= ps[SSCMM_STRSUB_BIRTH_MESSAGE];

	i->method = "MQTT";
	i->mqtt_cp = &ct->ccp;

	i->alpn = "x-amzn-mqtt-ca";

	/* share connections where possible */
	i->ssl_connection |= LCCSCF_PIPELINE;

	return 0;
}

const struct ss_pcols ss_pcol_mqtt = {
	"MQTT",
	"x-amzn-mqtt-ca", //"mqtt/3.1.1",
	&protocol_secstream_mqtt,
	secstream_connect_munge_mqtt,
	NULL, NULL
};
