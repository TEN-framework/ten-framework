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
 *
 * This file contains the stuff related to JSON-provided policy, it's not built
 * if LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY enabled.
 */

#include <private-lib-core.h>

static const char * const lejp_tokens_policy[] = {
	"release",
	"product",
	"schema-version",
	"via-socks5",
	"retry[].*.backoff",
	"retry[].*.conceal",
	"retry[].*.jitterpc",
	"retry[].*.svalidping",
	"retry[].*.svalidhup",
	"retry[].*",
	"certs[].*",
	"trust_stores[].name",
	"trust_stores[].stack",
	"metrics[].name",
	"metrics[].us_schedule",
	"metrics[].us_halflife",
	"metrics[].min_outlier",
	"metrics[].report",
	"s[].*.endpoint",
	"s[].*.via-socks5",
	"s[].*.protocol",
	"s[].*.port",
	"s[].*.plugins",
	"s[].*.tls",
	"s[].*.client_cert",
	"s[].*.opportunistic",
	"s[].*.nailed_up",
	"s[].*.allow_redirects",
	"s[].*.urgent_tx",
	"s[].*.urgent_rx",
	"s[].*.attr_priority",
	"s[].*.attr_low_latency",
	"s[].*.attr_high_throughput",
	"s[].*.attr_high_reliability",
	"s[].*.attr_low_cost",
	"s[].*.long_poll",
	"s[].*.ws_prioritize_reads",
	"s[].*.retry",
	"s[].*.timeout_ms",
	"s[].*.perf",
	"s[].*.tls_trust_store",
	"s[].*.proxy_buflen",
	"s[].*.proxy_buflen_rxflow_on_above",
	"s[].*.proxy_buflen_rxflow_off_below",
	"s[].*.client_buflen",
	"s[].*.client_buflen_rxflow_on_above",
	"s[].*.client_buflen_rxflow_off_below",
	"s[].*.metadata",
	"s[].*.metadata[].*",
	"s[].*.http_resp_map",
	"s[].*.http_resp_map[].*",

	"s[].*.http_auth_header",
	"s[].*.http_dsn_header",
	"s[].*.http_fwv_header",
	"s[].*.http_devtype_header",

	"s[].*.http_auth_preamble",

	"s[].*.http_no_content_length",
	"s[].*.rideshare",	/* streamtype name this rides shotgun with */
	"s[].*.payload_fmt",
	"s[].*.http_method",
	"s[].*.http_url",
	"s[].*.nghttp2_quirk_end_stream",
	"s[].*.h2q_oflow_txcr",
	"s[].*.http_multipart_name",
	"s[].*.http_multipart_filename",
	"s[].*.http_mime_content_type",
	"s[].*.http_www_form_urlencoded",
	"s[].*.http_expect",
	"s[].*.http_cookies",
	"s[].*.http_fail_redirect",
	"s[].*.http_multipart_ss_in",
	"s[].*.ws_subprotocol",
	"s[].*.ws_binary",
	"s[].*.local_sink",
	"s[].*.server",
	"s[].*.server_cert",
	"s[].*.server_key",
	"s[].*.mqtt_topic",
	"s[].*.mqtt_subscribe",
	"s[].*.mqtt_qos",
	"s[].*.mqtt_retain",
	"s[].*.mqtt_keep_alive",
	"s[].*.mqtt_clean_start",
	"s[].*.mqtt_will_topic",
	"s[].*.mqtt_will_message",
	"s[].*.mqtt_will_qos",
	"s[].*.mqtt_will_retain",
	"s[].*.mqtt_birth_topic",
	"s[].*.mqtt_birth_message",
	"s[].*.mqtt_birth_qos",
	"s[].*.mqtt_birth_retain",
	"s[].*.aws_iot",
	"s[].*.swake_validity",
	"s[].*.use_auth",
	"s[].*.aws_region",
	"s[].*.aws_service",
	"s[].*.direct_proto_str",
	"s[].*",
	"auth[].name",
	"auth[].type",
	"auth[].streamtype",
	"auth[].blob",
	"auth[]",
};

typedef enum {
	LSSPPT_RELEASE,
	LSSPPT_PRODUCT,
	LSSPPT_SCHEMA_VERSION,
	LSSPPT_VIA_SOCKS5,
	LSSPPT_BACKOFF,
	LSSPPT_CONCEAL,
	LSSPPT_JITTERPC,
	LSSPPT_VALIDPING_S,
	LSSPPT_VALIDHUP_S,
	LSSPPT_RETRY,
	LSSPPT_CERTS,
	LSSPPT_TRUST_STORES_NAME,
	LSSPPT_TRUST_STORES_STACK,
	LSSPPT_METRICS_NAME,
	LSSPPT_METRICS_US_SCHEDULE,
	LSSPPT_METRICS_US_HALFLIFE,
	LSSPPT_METRICS_MIN_OUTLIER,
	LSSPPT_METRICS_REPORT,
	LSSPPT_ENDPOINT,
	LSSPPT_VH_VIA_SOCKS5,
	LSSPPT_PROTOCOL,
	LSSPPT_PORT,
	LSSPPT_PLUGINS,
	LSSPPT_TLS,
	LSSPPT_TLS_CLIENT_CERT,
	LSSPPT_OPPORTUNISTIC,
	LSSPPT_NAILED_UP,
	LSSPPT_ALLOW_REDIRECTS,
	LSSPPT_URGENT_TX,
	LSSPPT_URGENT_RX,
	LSSPPT_ATTR_PRIORITY,
	LSSPPT_ATTR_LOW_LATENCY,
	LSSPPT_ATTR_HIGH_THROUGHPUT,
	LSSPPT_ATTR_HIGH_RELIABILITY,
	LSSPPT_ATTR_LOW_COST,
	LSSPPT_LONG_POLL,
	LSSPPT_PRIORITIZE_READS,
	LSSPPT_RETRYPTR,
	LSSPPT_DEFAULT_TIMEOUT_MS,
	LSSPPT_PERF,
	LSSPPT_TRUST,
	LSSPPT_PROXY_BUFLEN,
	LSSPPT_PROXY_BUFLEN_RXFLOW_ON_ABOVE,
	LSSPPT_PROXY_BUFLEN_RXFLOW_OFF_BELOW,
	LSSPPT_CLIENT_BUFLEN,
	LSSPPT_CLIENT_BUFLEN_RXFLOW_ON_ABOVE,
	LSSPPT_CLIENT_BUFLEN_RXFLOW_OFF_BELOW,
	LSSPPT_METADATA,
	LSSPPT_METADATA_ITEM,
	LSSPPT_HTTPRESPMAP,
	LSSPPT_HTTPRESPMAP_ITEM,

	LSSPPT_HTTP_AUTH_HEADER,
	LSSPPT_HTTP_DSN_HEADER,
	LSSPPT_HTTP_FWV_HEADER,
	LSSPPT_HTTP_TYPE_HEADER,

	LSSPPT_HTTP_AUTH_PREAMBLE,
	LSSPPT_HTTP_NO_CONTENT_LENGTH,
	LSSPPT_RIDESHARE,
	LSSPPT_PAYLOAD_FORMAT,
	LSSPPT_HTTP_METHOD,
	LSSPPT_HTTP_URL,
	LSSPPT_NGHTTP2_QUIRK_END_STREAM,
	LSSPPT_H2_QUIRK_OVERFLOWS_TXCR,
	LSSPPT_HTTP_MULTIPART_NAME,
	LSSPPT_HTTP_MULTIPART_FILENAME,
	LSSPPT_HTTP_MULTIPART_CONTENT_TYPE,
	LSSPPT_HTTP_WWW_FORM_URLENCODED,
	LSSPPT_HTTP_EXPECT,
	LSSPPT_HTTP_COOKIES,
	LSSPPT_HTTP_FAIL_REDIRECT,
	LSSPPT_HTTP_MULTIPART_SS_IN,
	LSSPPT_WS_SUBPROTOCOL,
	LSSPPT_WS_BINARY,
	LSSPPT_LOCAL_SINK,
	LSSPPT_SERVER,
	LSSPPT_SERVER_CERT,
	LSSPPT_SERVER_KEY,
	LSSPPT_MQTT_TOPIC,
	LSSPPT_MQTT_SUBSCRIBE,
	LSSPPT_MQTT_QOS,
	LSSPPT_MQTT_RETAIN,
	LSSPPT_MQTT_KEEPALIVE,
	LSSPPT_MQTT_CLEAN_START,
	LSSPPT_MQTT_WILL_TOPIC,
	LSSPPT_MQTT_WILL_MESSAGE,
	LSSPPT_MQTT_WILL_QOS,
	LSSPPT_MQTT_WILL_RETAIN,
	LSSPPT_MQTT_BIRTH_TOPIC,
	LSSPPT_MQTT_BIRTH_MESSAGE,
	LSSPPT_MQTT_BIRTH_QOS,
	LSSPPT_MQTT_BIRTH_RETAIN,
	LSSPPT_MQTT_AWS_IOT,
	LSSPPT_SWAKE_VALIDITY,
	LSSPPT_USE_AUTH,
	LSSPPT_AWS_REGION,
	LSSPPT_AWS_SERVICE,
	LSSPPT_DIRECT_PROTO_STR,
	LSSPPT_STREAMTYPES,
	LSSPPT_AUTH_NAME,
	LSSPPT_AUTH_TYPE,
	LSSPPT_AUTH_STREAMTYPE,
	LSSPPT_AUTH_BLOB,
	LSSPPT_AUTH,

} policy_token_t;

#define POL_AC_INITIAL	2048
#define POL_AC_GRAIN	800
#define MAX_CERT_TEMP	3072 /* used to discover actual cert size for realloc */

static uint16_t sizes[] = {
	sizeof(backoff_t),
	sizeof(lws_ss_x509_t),
	sizeof(lws_ss_trust_store_t),
	sizeof(lws_ss_policy_t),
	sizeof(lws_ss_auth_t),
	sizeof(lws_metric_policy_t),
};

static const char * const protonames[] = {
	"h1",		/* LWSSSP_H1 */
	"h2",		/* LWSSSP_H2 */
	"ws",		/* LWSSSP_WS */
	"mqtt",		/* LWSSSP_MQTT */
	"raw",		/* LWSSSP_RAW */
};

static const lws_ss_auth_t *
lws_ss_policy_find_auth_by_name(struct policy_cb_args *a,
				const char *name, size_t len)
{
	const lws_ss_auth_t *auth = a->heads[LTY_AUTH].a;

	while (auth) {
		if (auth->name &&
		    len == strlen(auth->name) &&
		    !strncmp(auth->name, name, len))
			return auth;

		auth = auth->next;
	}

	return NULL;
}

static int
lws_ss_policy_alloc_helper(struct policy_cb_args *a, int type)
{
	/*
	 * We do the pointers always as .b union member, all of the
	 * participating structs begin with .next and .name the same
	 */

	a->curr[type].b = lwsac_use_zero(&a->ac,
				sizes[type], POL_AC_GRAIN);
	if (!a->curr[type].b)
		return 1;

	a->curr[type].b->next = a->heads[type].b;
	a->heads[type].b = a->curr[type].b;

	return 0;
}

static signed char
lws_ss_policy_parser_cb(struct lejp_ctx *ctx, char reason)
{
	struct policy_cb_args *a = (struct policy_cb_args *)ctx->user;
#if defined(LWS_WITH_SSPLUGINS)
	const lws_ss_plugin_t **pin;
#endif
	char **pp, dotstar[32], *q;
	lws_ss_trust_store_t *ts;
	lws_ss_metadata_t *pmd;
	lws_ss_x509_t *x, **py;
	lws_ss_policy_t *p2;
	lws_retry_bo_t *b;
	size_t inl, outl;
	uint8_t *extant;
	backoff_t *bot;
	int n = -1;

//	lwsl_debug("%s: %d %d %s\n", __func__, reason, ctx->path_match - 1,
//		   ctx->path);

	switch (ctx->path_match - 1) {
	case LSSPPT_RETRY:
		n = LTY_BACKOFF;
		break;
	case LSSPPT_CERTS:
		n = LTY_X509;
		break;
	case LSSPPT_TRUST_STORES_NAME:
	case LSSPPT_TRUST_STORES_STACK:
		n = LTY_TRUSTSTORE;
		break;
	case LSSPPT_STREAMTYPES:
		n = LTY_POLICY;
		break;
	case LSSPPT_AUTH:
		n = LTY_AUTH;
		break;
	case LSSPPT_METRICS_NAME:
	case LSSPPT_METRICS_US_SCHEDULE:
	case LSSPPT_METRICS_US_HALFLIFE:
	case LSSPPT_METRICS_MIN_OUTLIER:
	case LSSPPT_METRICS_REPORT:
		n = LTY_METRICS;
		break;
	}

	if (reason == LEJPCB_ARRAY_START &&
	    (ctx->path_match - 1 == LSSPPT_PLUGINS ||
	     ctx->path_match - 1 == LSSPPT_METADATA ||
	     ctx->path_match - 1 == LSSPPT_HTTPRESPMAP))
		a->count = 0;

	if (reason == LEJPCB_OBJECT_START && n == LTY_AUTH) {
		if (lws_ss_policy_alloc_helper(a, LTY_AUTH))
			goto oom;
		return 0;
	}

	if (reason == LEJPCB_ARRAY_END &&
	    ctx->path_match - 1 == LSSPPT_TRUST_STORES_STACK && !a->count) {
		lwsl_err("%s: at least one cert required in trust store\n",
				__func__);
		goto oom;
	}

	if (reason == LEJPCB_ARRAY_END && a->count && a->pending_respmap) {

		// lwsl_notice("%s: allocating respmap %d\n", __func__, a->count);

		a->curr[LTY_POLICY].p->u.http.respmap = lwsac_use_zero(&a->ac,
			sizeof(lws_ss_http_respmap_t) * (unsigned int)a->count, POL_AC_GRAIN);

		if (!a->curr[LTY_POLICY].p->u.http.respmap)
			goto oom;

		memcpy((void *)a->curr[LTY_POLICY].p->u.http.respmap,
		       a->respmap, sizeof(lws_ss_http_respmap_t) * (unsigned int)a->count);
		a->curr[LTY_POLICY].p->u.http.count_respmap = (uint8_t)a->count;
		a->count = 0;
		a->pending_respmap = 0;

		return 0;
	}

	if (reason == LEJPCB_OBJECT_END && a->p) {
		/*
		 * Allocate a just-the-right-size buf for the cert DER now
		 * we decoded it into the a->p temp buffer and know the exact
		 * size.
		 *
		 * The struct *x is in the lwsac... the ca_der it points to
		 * is individually allocated from the heap
		 */
		a->curr[LTY_X509].x->ca_der = lws_malloc((unsigned int)a->count, "ssx509");
		if (!a->curr[LTY_X509].x->ca_der)
			goto oom;
		memcpy((uint8_t *)a->curr[LTY_X509].x->ca_der, a->p, (unsigned int)a->count);
		a->curr[LTY_X509].x->ca_der_len = (unsigned int)a->count;

		/*
		 * ... and then we can free the temp buffer
		 */
		lws_free_set_NULL(a->p);

		return 0;
	}

	if (reason == LEJPCB_PAIR_NAME && n != -1 &&
	    (n != LTY_TRUSTSTORE && n != LTY_AUTH && n != LTY_METRICS)) {

		p2 = NULL;
		if (n == LTY_POLICY) {
			/*
			 * We want to allow for the possibility of overlays...
			 * eg, we come later with a JSON snippet that overrides
			 * select streamtype members of a streamtype that was
			 * already defined
			 */
			p2 = (lws_ss_policy_t *)a->context->pss_policies;

			while (p2) {
				if (!strncmp(p2->streamtype,
					     ctx->path + ctx->st[ctx->sp].p,
					     (unsigned int)(ctx->path_match_len -
						          ctx->st[ctx->sp].p))) {
					lwsl_info("%s: overriding s[] %s\n",
						  __func__, p2->streamtype);
					break;
				}

				p2 = p2->next;
			}
		}

		/*
		 * We do the pointers always as .b union member, all of the
		 * participating structs begin with .next and .name the same
		 */
		if (p2) /* we may be overriding existing streamtype... */
			a->curr[n].b = (backoff_t *)p2;
		else
			a->curr[n].b = lwsac_use_zero(&a->ac, sizes[n],
							POL_AC_GRAIN);
		if (!a->curr[n].b)
			goto oom;

		if (n == LTY_X509) {
			a->p = lws_malloc(MAX_CERT_TEMP, "cert temp");
			if (!a->p)
				goto oom;
			memset(&a->b64, 0, sizeof(a->b64));
		}

		a->count = 0;
		if (!p2) {
			a->curr[n].b->next = a->heads[n].b;
			a->heads[n].b = a->curr[n].b;
			pp = (char **)&a->curr[n].b->name;

			goto string1;
		}

		return 0; /* overriding */
	}

	if (!(reason & LEJP_FLAG_CB_IS_VALUE) || !ctx->path_match)
		return 0;

	switch (ctx->path_match - 1) {

	/* strings */

	case LSSPPT_RELEASE:
		break;

	case LSSPPT_PRODUCT:
		break;

	case LSSPPT_SCHEMA_VERSION:
		break;

	case LSSPPT_VIA_SOCKS5:
		/* the global / default proxy */
		pp = (char **)&a->socks5_proxy;
		goto string2;

	case LSSPPT_BACKOFF:
		b = &a->curr[LTY_BACKOFF].b->r;
		if (b->retry_ms_table_count == 8) {
			lwsl_err("%s: > 8 backoff levels\n", __func__);
			return 1;
		}
		if (!b->retry_ms_table_count) {
			b->retry_ms_table = (uint32_t *)lwsac_use_zero(&a->ac,
					   sizeof(uint32_t) * 8, POL_AC_GRAIN);
			if (!b->retry_ms_table)
				goto oom;
		}

		((uint32_t *)b->retry_ms_table)
				[b->retry_ms_table_count++] = (uint32_t)atoi(ctx->buf);
		break;

	case LSSPPT_CONCEAL:
		a->curr[LTY_BACKOFF].b->r.conceal_count = (uint16_t)atoi(ctx->buf);
		break;

	case LSSPPT_JITTERPC:
		a->curr[LTY_BACKOFF].b->r.jitter_percent = (uint8_t)atoi(ctx->buf);
		break;

	case LSSPPT_VALIDPING_S:
		a->curr[LTY_BACKOFF].b->r.secs_since_valid_ping = (uint16_t)atoi(ctx->buf);
		break;

	case LSSPPT_VALIDHUP_S:
		a->curr[LTY_BACKOFF].b->r.secs_since_valid_hangup = (uint16_t)atoi(ctx->buf);
		break;

	case LSSPPT_CERTS:
		if (a->count + ctx->npos >= MAX_CERT_TEMP) {
			lwsl_err("%s: cert too big\n", __func__);
			goto oom;
		}
		inl = ctx->npos;
		outl = MAX_CERT_TEMP - (unsigned int)a->count;

		lws_b64_decode_stateful(&a->b64, ctx->buf, &inl,
					a->p + a->count, &outl,
					reason == LEJPCB_VAL_STR_END);
		a->count += (int)outl;
		if (inl != ctx->npos) {
			lwsl_err("%s: b64 decode fail\n", __func__);
			goto oom;
		}
		break;

	case LSSPPT_TRUST_STORES_NAME:
		if (lws_ss_policy_alloc_helper(a, LTY_TRUSTSTORE))
			goto oom;

		a->count = 0;
		pp = (char **)&a->curr[LTY_TRUSTSTORE].b->name;

		goto string2;

	case LSSPPT_TRUST_STORES_STACK:
		if (a->count >= (int)LWS_ARRAY_SIZE(
					a->curr[LTY_TRUSTSTORE].t->ssx509)) {
			lwsl_err("%s: trust store too big\n", __func__);
			goto oom;
		}
		lwsl_debug("%s: trust stores stack %.*s\n", __func__,
			   ctx->npos, ctx->buf);
		x = a->heads[LTY_X509].x;
		while (x) {
			if (!strncmp(x->vhost_name, ctx->buf, ctx->npos)) {
				a->curr[LTY_TRUSTSTORE].t->ssx509[a->count++] = x;
				a->curr[LTY_TRUSTSTORE].t->count++;

				return 0;
			}
			x = x->next;
		}
		lws_strnncpy(dotstar, ctx->buf, ctx->npos, sizeof(dotstar));
		lwsl_err("%s: unknown trust store entry %s\n", __func__,
			 dotstar);
		goto oom;
#if defined(LWS_WITH_SYS_METRICS)
	case LSSPPT_METRICS_NAME:
		if (lws_ss_policy_alloc_helper(a, LTY_METRICS))
			goto oom;

		pp = (char **)&a->curr[LTY_METRICS].b->name;

		goto string2;

	case LSSPPT_METRICS_US_SCHEDULE:
		a->curr[LTY_METRICS].m->us_schedule = (uint64_t)atoll(ctx->buf);
		break;

	case LSSPPT_METRICS_US_HALFLIFE:
		a->curr[LTY_METRICS].m->us_decay_unit = (uint32_t)atol(ctx->buf);
		break;

	case LSSPPT_METRICS_MIN_OUTLIER:
		a->curr[LTY_METRICS].m->min_contributors = (uint8_t)atoi(ctx->buf);
		break;

	case LSSPPT_METRICS_REPORT:
		pp = (char **)&a->curr[LTY_METRICS].m->report;
		goto string2;
#endif

	case LSSPPT_SERVER_CERT:
	case LSSPPT_SERVER_KEY:

		/* iterate through the certs */

		py = &a->heads[LTY_X509].x;
		x = a->heads[LTY_X509].x;
		while (x) {
			if (!strncmp(x->vhost_name, ctx->buf, ctx->npos) &&
					!x->vhost_name[ctx->npos]) {
				if ((ctx->path_match - 1) == LSSPPT_SERVER_CERT)
					a->curr[LTY_POLICY].p->trust.server.cert = x;
				else
					a->curr[LTY_POLICY].p->trust.server.key = x;
				/*
				 * Certs that are for servers need to stick
				 * around in DER form, so the vhost can be
				 * instantiated when the server is brought up
				 */
				x->keep = 1;
				lwsl_notice("%s: server '%s' keep %d %p\n",
					    __func__, x->vhost_name,
						ctx->path_match - 1, x);

				/*
				 * Server DER we need to move it to another
				 * list just for destroying it when the context
				 * is destroyed... snip us out of the live
				 * X.509 list
				 */

				*py = x->next;

				/*
				 * ... and instead put us on the list of things
				 * to keep hold of for context destruction
				 */

				x->next = a->context->server_der_list;
				a->context->server_der_list = x;

				return 0;
			}
			py = &x->next;
			x = x->next;
		}
		lws_strnncpy(dotstar, ctx->buf, ctx->npos, sizeof(dotstar));
		lwsl_err("%s: unknown cert / key %s\n", __func__, dotstar);
		goto oom;

	case LSSPPT_ENDPOINT:
		pp = (char **)&a->curr[LTY_POLICY].p->endpoint;
		goto string2;

	case LSSPPT_VH_VIA_SOCKS5:
		pp = (char **)&a->curr[LTY_POLICY].p->socks5_proxy;
		goto string2;

	case LSSPPT_PORT:
		a->curr[LTY_POLICY].p->port = (uint16_t)atoi(ctx->buf);
		break;

	case LSSPPT_PROXY_BUFLEN:
		a->curr[LTY_POLICY].p->proxy_buflen = (uint32_t)atol(ctx->buf);
		break;

	case LSSPPT_PROXY_BUFLEN_RXFLOW_ON_ABOVE:
		a->curr[LTY_POLICY].p->proxy_buflen_rxflow_on_above =
						(uint32_t)atol(ctx->buf);
		break;
	case LSSPPT_PROXY_BUFLEN_RXFLOW_OFF_BELOW:
		a->curr[LTY_POLICY].p->proxy_buflen_rxflow_off_below =
						(uint32_t)atol(ctx->buf);
		break;

	case LSSPPT_CLIENT_BUFLEN:
		a->curr[LTY_POLICY].p->client_buflen = (uint32_t)atol(ctx->buf);
		break;

	case LSSPPT_CLIENT_BUFLEN_RXFLOW_ON_ABOVE:
		a->curr[LTY_POLICY].p->client_buflen_rxflow_on_above =
						(uint32_t)atol(ctx->buf);
		break;
	case LSSPPT_CLIENT_BUFLEN_RXFLOW_OFF_BELOW:
		a->curr[LTY_POLICY].p->client_buflen_rxflow_off_below =
						(uint32_t)atol(ctx->buf);
		break;

	case LSSPPT_HTTP_METHOD:
		pp = (char **)&a->curr[LTY_POLICY].p->u.http.method;
		goto string2;

	case LSSPPT_HTTP_URL:
		pp = (char **)&a->curr[LTY_POLICY].p->u.http.url;
		goto string2;

	case LSSPPT_RIDESHARE:
		pp = (char **)&a->curr[LTY_POLICY].p->rideshare_streamtype;
		goto string2;

	case LSSPPT_PAYLOAD_FORMAT:
		pp = (char **)&a->curr[LTY_POLICY].p->payload_fmt;
		goto string2;

	case LSSPPT_PLUGINS:
#if defined(LWS_WITH_SSPLUGINS)
		pin = a->context->pss_plugins;
		if (a->count ==
			  (int)LWS_ARRAY_SIZE(a->curr[LTY_POLICY].p->plugins)) {
			lwsl_err("%s: too many plugins\n", __func__);

			goto oom;
		}
		if (!pin)
			break;
		while (*pin) {
			if (!strncmp((*pin)->name, ctx->buf, ctx->npos)) {
				a->curr[LTY_POLICY].p->plugins[a->count++] = *pin;
				return 0;
			}
			pin++;
		}
		lwsl_err("%s: unknown plugin\n", __func__);
		goto oom;
#else
		break;
#endif

	case LSSPPT_TLS:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_TLS;
		break;

	case LSSPPT_TLS_CLIENT_CERT:
		a->curr[LTY_POLICY].p->client_cert = (uint8_t)(atoi(ctx->buf) + 1);
		break;

	case LSSPPT_AUTH_BLOB:
		a->curr[LTY_AUTH].a->blob_index = (uint8_t)atoi(ctx->buf);
		break;
	case LSSPPT_HTTP_EXPECT:
		a->curr[LTY_POLICY].p->u.http.resp_expect = (uint16_t)atoi(ctx->buf);
		break;

	case LSSPPT_DEFAULT_TIMEOUT_MS:
		a->curr[LTY_POLICY].p->timeout_ms = (uint32_t)atoi(ctx->buf);
		break;

	case LSSPPT_ATTR_PRIORITY:
		a->curr[LTY_POLICY].p->priority = (uint8_t)atoi(ctx->buf);
		break;

	case LSSPPT_OPPORTUNISTIC:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_OPPORTUNISTIC;
		break;
	case LSSPPT_NAILED_UP:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_NAILED_UP;
		break;
	case LSSPPT_URGENT_TX:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_URGENT_TX;
		break;
	case LSSPPT_URGENT_RX:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_URGENT_RX;
		break;
	case LSSPPT_LONG_POLL:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_LONG_POLL;
		break;
	case LSSPPT_PRIORITIZE_READS:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_PRIORITIZE_READS;
		break;

	case LSSPPT_HTTP_WWW_FORM_URLENCODED:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |=
					LWSSSPOLF_HTTP_X_WWW_FORM_URLENCODED;
		break;
	case LSSPPT_SWAKE_VALIDITY:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |=
					LWSSSPOLF_WAKE_SUSPEND__VALIDITY;
		break;
	case LSSPPT_ALLOW_REDIRECTS:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |=
						LWSSSPOLF_ALLOW_REDIRECTS;
		break;
	case LSSPPT_HTTP_COOKIES:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |=
						LWSSSPOLF_HTTP_CACHE_COOKIES;
		break;
	case LSSPPT_HTTP_MULTIPART_SS_IN:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |=
						LWSSSPOLF_HTTP_MULTIPART_IN;
		return 0;

	case LSSPPT_ATTR_LOW_LATENCY:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |=
						LWSSSPOLF_ATTR_LOW_LATENCY;
		return 0;

	case LSSPPT_ATTR_HIGH_THROUGHPUT:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |=
						LWSSSPOLF_ATTR_HIGH_THROUGHPUT;
		return 0;

	case LSSPPT_ATTR_HIGH_RELIABILITY:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |=
						LWSSSPOLF_ATTR_HIGH_RELIABILITY;
		return 0;

	case LSSPPT_ATTR_LOW_COST:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_ATTR_LOW_COST;
		return 0;

	case LSSPPT_PERF:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_PERF;
		return 0;

	case LSSPPT_RETRYPTR:
		bot = a->heads[LTY_BACKOFF].b;
		while (bot) {
			if (!strncmp(ctx->buf, bot->name, ctx->npos)) {
				a->curr[LTY_POLICY].p->retry_bo = &bot->r;

				return 0;
			}
			bot = bot->next;
		}
		lwsl_err("%s: unknown backoff scheme\n", __func__);

		return -1;

	case LSSPPT_TRUST:
		ts = a->heads[LTY_TRUSTSTORE].t;
		while (ts) {
			if (!strncmp(ctx->buf, ts->name, ctx->npos)) {
				a->curr[LTY_POLICY].p->trust.store = ts;
				return 0;
			}
			ts = ts->next;
		}
		lws_strnncpy(dotstar, ctx->buf, ctx->npos, sizeof(dotstar));
		lwsl_err("%s: unknown trust store name %s\n", __func__,
			 dotstar);

		return -1;

	case LSSPPT_METADATA:
		break;

	case LSSPPT_USE_AUTH:
		a->curr[LTY_POLICY].p->auth =
			lws_ss_policy_find_auth_by_name(a, ctx->buf, ctx->npos);
		if (!a->curr[LTY_POLICY].p->auth) {
			lws_strnncpy(dotstar, ctx->buf, ctx->npos, sizeof(dotstar));
			lwsl_err("%s: unknown auth '%s'\n", __func__, dotstar);
			return -1;
		}
		break;


	case LSSPPT_METADATA_ITEM:
		pmd = a->curr[LTY_POLICY].p->metadata;
		a->curr[LTY_POLICY].p->metadata = lwsac_use_zero(&a->ac,
			sizeof(lws_ss_metadata_t) + ctx->npos +
			(unsigned int)(ctx->path_match_len - ctx->st[ctx->sp - 2].p + 1) + 2,
			POL_AC_GRAIN);
		a->curr[LTY_POLICY].p->metadata->next = pmd;

		q = (char *)a->curr[LTY_POLICY].p->metadata +
				sizeof(lws_ss_metadata_t);
		a->curr[LTY_POLICY].p->metadata->name = q;
		memcpy(q, ctx->path + ctx->st[ctx->sp - 2].p + 1,
		       (unsigned int)(ctx->path_match_len - ctx->st[ctx->sp - 2].p));

		q += ctx->path_match_len - ctx->st[ctx->sp - 2].p;
		a->curr[LTY_POLICY].p->metadata->value__may_own_heap = q;
		memcpy(q, ctx->buf, ctx->npos);

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
		/*
		 * Check the metadata value part to see if it's a well-known
		 * http header... if so, LWS_HTTP_NO_KNOWN_HEADER (0xff) means
		 * no header string match else it's the well-known header index
		 */
		a->curr[LTY_POLICY].p->metadata->value_is_http_token = (uint8_t)
			lws_http_string_to_known_header(ctx->buf, ctx->npos);
#endif

		a->curr[LTY_POLICY].p->metadata->length = /* the index in handle->metadata */
				a->curr[LTY_POLICY].p->metadata_count++;

		a->curr[LTY_POLICY].p->metadata->value_length = ctx->npos;
		break;

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)

	case LSSPPT_HTTPRESPMAP_ITEM:
		if (a->count >= (int)LWS_ARRAY_SIZE(a->respmap)) {
			lwsl_err("%s: respmap too big\n", __func__);
			return -1;
		}
		a->respmap[a->count].resp = (uint16_t)
				atoi(ctx->path + ctx->st[ctx->sp - 2].p + 1);
		a->respmap[a->count].state = (uint16_t)atoi(ctx->buf);
		a->pending_respmap = 1;
		a->count++;
		break;

	case LSSPPT_HTTP_AUTH_HEADER:
	case LSSPPT_HTTP_DSN_HEADER:
	case LSSPPT_HTTP_FWV_HEADER:
	case LSSPPT_HTTP_TYPE_HEADER:
		pp = (char **)&a->curr[LTY_POLICY].p->u.http.blob_header[
		               (ctx->path_match - 1) - LSSPPT_HTTP_AUTH_HEADER];
		goto string2;

	case LSSPPT_HTTP_AUTH_PREAMBLE:
		pp = (char **)&a->curr[LTY_POLICY].p->u.http.auth_preamble;
		goto string2;

	case LSSPPT_HTTP_NO_CONTENT_LENGTH:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |=
					LWSSSPOLF_HTTP_NO_CONTENT_LENGTH;
		break;

	case LSSPPT_NGHTTP2_QUIRK_END_STREAM:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |=
					LWSSSPOLF_QUIRK_NGHTTP2_END_STREAM;
		break;
	case LSSPPT_H2_QUIRK_OVERFLOWS_TXCR:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |=
					LWSSSPOLF_H2_QUIRK_OVERFLOWS_TXCR;
		break;
	case LSSPPT_HTTP_MULTIPART_NAME:
		a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_HTTP_MULTIPART;
		pp = (char **)&a->curr[LTY_POLICY].p->u.http.multipart_name;
		goto string2;
	case LSSPPT_HTTP_MULTIPART_FILENAME:
		a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_HTTP_MULTIPART;
		pp = (char **)&a->curr[LTY_POLICY].p->u.http.multipart_filename;
		goto string2;
	case LSSPPT_HTTP_MULTIPART_CONTENT_TYPE:
		a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_HTTP_MULTIPART;
		pp = (char **)&a->curr[LTY_POLICY].p->u.http.multipart_content_type;
		goto string2;

	case LSSPPT_AUTH_NAME:
		pp = (char **)&a->curr[LTY_AUTH].a->name;
		goto string2;

	case LSSPPT_AUTH_STREAMTYPE:
		pp = (char **)&a->curr[LTY_AUTH].a->streamtype;
		goto string2;
	case LSSPPT_AUTH_TYPE:
		pp = (char **)&a->curr[LTY_AUTH].a->type;
		goto string2;
	case LSSPPT_HTTP_FAIL_REDIRECT:
		a->curr[LTY_POLICY].p->u.http.fail_redirect =
						reason == LEJPCB_VAL_TRUE;
		break;
#if defined(LWS_WITH_SECURE_STREAMS_AUTH_SIGV4)
	case LSSPPT_AWS_REGION:
		pp = (char **)&a->curr[LTY_POLICY].p->aws_region;
		goto string2;

	case LSSPPT_AWS_SERVICE:
		pp = (char **)&a->curr[LTY_POLICY].p->aws_service;
		goto string2;
#endif

#endif

#if defined(LWS_ROLE_WS)

	case LSSPPT_WS_SUBPROTOCOL:
		pp = (char **)&a->curr[LTY_POLICY].p->u.http.u.ws.subprotocol;
		goto string2;

	case LSSPPT_WS_BINARY:
		a->curr[LTY_POLICY].p->u.http.u.ws.binary =
						reason == LEJPCB_VAL_TRUE;
		break;
#endif

	case LSSPPT_LOCAL_SINK:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_LOCAL_SINK;
		break;

	case LSSPPT_SERVER:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |= LWSSSPOLF_SERVER;
		break;

#if defined(LWS_ROLE_MQTT)
	case LSSPPT_MQTT_TOPIC:
		pp = (char **)&a->curr[LTY_POLICY].p->u.mqtt.topic;
		goto string2;

	case LSSPPT_MQTT_SUBSCRIBE:
		pp = (char **)&a->curr[LTY_POLICY].p->u.mqtt.subscribe;
		goto string2;

	case LSSPPT_MQTT_QOS:
		a->curr[LTY_POLICY].p->u.mqtt.qos = (uint8_t)atoi(ctx->buf);
		break;

	case LSSPPT_MQTT_RETAIN:
		a->curr[LTY_POLICY].p->u.mqtt.retain =
						reason == LEJPCB_VAL_TRUE;
		break;

	case LSSPPT_MQTT_KEEPALIVE:
		a->curr[LTY_POLICY].p->u.mqtt.keep_alive = (uint16_t)atoi(ctx->buf);
		break;

	case LSSPPT_MQTT_CLEAN_START:
		a->curr[LTY_POLICY].p->u.mqtt.clean_start =
						reason == LEJPCB_VAL_TRUE;
		break;
	case LSSPPT_MQTT_WILL_TOPIC:
		pp = (char **)&a->curr[LTY_POLICY].p->u.mqtt.will_topic;
		goto string2;

	case LSSPPT_MQTT_WILL_MESSAGE:
		pp = (char **)&a->curr[LTY_POLICY].p->u.mqtt.will_message;
		goto string2;

	case LSSPPT_MQTT_WILL_QOS:
		a->curr[LTY_POLICY].p->u.mqtt.will_qos = (uint8_t)atoi(ctx->buf);
		break;
	case LSSPPT_MQTT_WILL_RETAIN:
		a->curr[LTY_POLICY].p->u.mqtt.will_retain =
						reason == LEJPCB_VAL_TRUE;
		break;
	case LSSPPT_MQTT_BIRTH_TOPIC:
		pp = (char **)&a->curr[LTY_POLICY].p->u.mqtt.birth_topic;
		goto string2;

	case LSSPPT_MQTT_BIRTH_MESSAGE:
		pp = (char **)&a->curr[LTY_POLICY].p->u.mqtt.birth_message;
		goto string2;

	case LSSPPT_MQTT_BIRTH_QOS:
		a->curr[LTY_POLICY].p->u.mqtt.birth_qos = (uint8_t)atoi(ctx->buf);
		break;
	case LSSPPT_MQTT_BIRTH_RETAIN:
		a->curr[LTY_POLICY].p->u.mqtt.birth_retain =
						reason == LEJPCB_VAL_TRUE;
		break;
	case LSSPPT_MQTT_AWS_IOT:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->u.mqtt.aws_iot =
						reason == LEJPCB_VAL_TRUE;
		break;
#endif
	case LSSPPT_DIRECT_PROTO_STR:
		if (reason == LEJPCB_VAL_TRUE)
			a->curr[LTY_POLICY].p->flags |=
					LWSSSPOLF_DIRECT_PROTO_STR;
		break;


	case LSSPPT_PROTOCOL:
		a->curr[LTY_POLICY].p->protocol = 0xff;
		for (n = 0; n < (int)LWS_ARRAY_SIZE(protonames); n++)
			if (strlen(protonames[n]) == ctx->npos &&
			    !strncmp(ctx->buf, protonames[n], ctx->npos))
				a->curr[LTY_POLICY].p->protocol = (uint8_t)n;

		if (a->curr[LTY_POLICY].p->protocol != 0xff)
			break;
		lws_strnncpy(dotstar, ctx->buf, ctx->npos, sizeof(dotstar));
		lwsl_err("%s: unknown protocol name %s\n", __func__, dotstar);
		return -1;

	default:
		break;
	}

	return 0;

string2:
	/*
	 * If we can do const string folding, reuse the existing string rather
	 * than make a new entry
	 */
	extant = lwsac_scan_extant(a->ac, (uint8_t *)ctx->buf, (size_t)ctx->npos, 1);
	if (extant) {
		*pp = (char *)extant;

		return 0;
	}
	*pp = lwsac_use_backfill(&a->ac, (size_t)(ctx->npos + 1), POL_AC_GRAIN);
	if (!*pp)
		goto oom;
	memcpy(*pp, ctx->buf, ctx->npos);
	(*pp)[ctx->npos] = '\0';

	return 0;

string1:
	n = ctx->st[ctx->sp].p;
	*pp = lwsac_use_backfill(&a->ac, (size_t)ctx->path_match_len + (size_t)1 - (size_t)n,
				 POL_AC_GRAIN);
	if (!*pp)
		goto oom;
	memcpy(*pp, ctx->path + n, ctx->path_match_len - (unsigned int)n);
	(*pp)[ctx->path_match_len - n] = '\0';

	return 0;

oom:
	lwsl_err("%s: OOM\n", __func__);
	lws_free_set_NULL(a->p);
	lwsac_free(&a->ac);

	return -1;
}

int
lws_ss_policy_parse_begin(struct lws_context *context, int overlay)
{
	struct policy_cb_args *args;
	char *p;

	args = lws_zalloc(sizeof(struct policy_cb_args), __func__);
	if (!args) {
		lwsl_err("%s: OOM\n", __func__);

		return 1;
	}
	if (overlay)
		/* continue to use the existing lwsac */
		args->ac = context->ac_policy;
	else
		/* we don't want to see any old policy */
		context->pss_policies = NULL;

	context->pol_args = args;
	args->context = context;
	p = lwsac_use(&args->ac, 1, POL_AC_INITIAL);
	if (!p) {
		lwsl_err("%s: OOM\n", __func__);
		lws_free_set_NULL(context->pol_args);

		return -1;
	}
	*p = 0;
	lejp_construct(&args->jctx, lws_ss_policy_parser_cb, args,
		       lejp_tokens_policy, LWS_ARRAY_SIZE(lejp_tokens_policy));

	return 0;
}

int
lws_ss_policy_parse_abandon(struct lws_context *context)
{
	struct policy_cb_args *args = (struct policy_cb_args *)context->pol_args;
	lws_ss_x509_t *x;

	x = args->heads[LTY_X509].x;
	while (x) {
		/*
		 * Free all the client DER buffers now they have been parsed
		 * into tls library X.509 objects
		 */
		lws_free((void *)x->ca_der);
		x->ca_der = NULL;

		x = x->next;
	}

	x = context->server_der_list;
	while (x) {
		lws_free((void *)x->ca_der);
		x->ca_der = NULL;

		x = x->next;
	}

	lejp_destruct(&args->jctx);
	lwsac_free(&args->ac);
	lws_free_set_NULL(context->pol_args);

	context->server_der_list = NULL;

	return 0;
}

#if !defined(LWS_PLAT_FREERTOS) && !defined(LWS_PLAT_OPTEE)
int
lws_ss_policy_parse_file(struct lws_context *cx, const char *filepath)
{
	struct policy_cb_args *args = (struct policy_cb_args *)cx->pol_args;
	uint8_t buf[512];
	int n, m, fd = lws_open(filepath, LWS_O_RDONLY);

	if (fd < 0)
		return LEJP_REJECT_UNKNOWN;

	do {
		n = (int)read(fd, buf, sizeof(buf));
		if (n < 0) {
			m = -1;
			goto bail;
		}

		m = lejp_parse(&args->jctx, buf, n);
		if (m != LEJP_CONTINUE && m < 0) {
			lwsl_err("%s: parse failed line %u: %d: %s\n", __func__,
				 (unsigned int)args->jctx.line, m,
				 lejp_error_to_string(m));
			lws_ss_policy_parse_abandon(cx);

			m = -1;
			goto bail;
		}

		if (m != LEJP_CONTINUE)
			break;
	} while (n);

	m = 0;
bail:
	close(fd);

	return m;
}
#endif

int
lws_ss_policy_parse(struct lws_context *context, const uint8_t *buf, size_t len)
{
	struct policy_cb_args *args = (struct policy_cb_args *)context->pol_args;
	int m;

#if !defined(LWS_PLAT_FREERTOS) && !defined(LWS_PLAT_OPTEE)
	if (args->jctx.line < 2 && buf[0] != '{' && !args->parse_data)
		return lws_ss_policy_parse_file(context, (const char *)buf);
#endif

	args->parse_data = 1;
	m = lejp_parse(&args->jctx, buf, (int)len);
	if (m == LEJP_CONTINUE || m >= 0)
		return m;

	lwsl_err("%s: parse failed line %u: %d: %s\n", __func__,
		 (unsigned int)args->jctx.line, m, lejp_error_to_string(m));
	lws_ss_policy_parse_abandon(context);
	assert(0);

	return m;
}

int
lws_ss_policy_overlay(struct lws_context *context, const char *overlay)
{
	lws_ss_policy_parse_begin(context, 1);
	return lws_ss_policy_parse(context, (const uint8_t *)overlay,
				   strlen(overlay));
}

const lws_ss_policy_t *
lws_ss_policy_get(struct lws_context *context)
{
	struct policy_cb_args *args = (struct policy_cb_args *)context->pol_args;

	if (!args)
		return NULL;

	return args->heads[LTY_POLICY].p;
}

const lws_ss_auth_t *
lws_ss_auth_get(struct lws_context *context)
{
	struct policy_cb_args *args = (struct policy_cb_args *)context->pol_args;

	if (!args)
		return NULL;

	return args->heads[LTY_AUTH].a;
}
