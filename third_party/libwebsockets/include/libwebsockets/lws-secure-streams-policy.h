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
 * included from libwebsockets.h
 */

typedef int (*plugin_auth_status_cb)(struct lws_ss_handle *ss, int status);

/**
 * lws_ss_plugin_auth_t - api for an auth plugin
 *
 * Auth plugins create and sequence authenticated connections that can carry one
 * or more streams to an endpoint.  That may involve other connections to other
 * places to eg, gather authenticated tokens and then make the real connection
 * using the tokens.
 *
 * The secure stream object contains members to record which auth plugin the
 * stream is bound to and an over-allocation of the secure stream object to
 * contain the plugin auth private data.
 *
 * The auth plugin controls the state of the stream connection via the status
 * callback, and handles retries.
 *
 * Network connections may require one kind of auth sequencing, and streams
 * inside those connections another kind of auth sequencing depending on their
 * role.  So the secure stream object allows defining plugins for both kinds.
 *
 * Streams may disappear at any time and require reauth to bring a new one up.
 * The auth plugin sequencer will connect / reconnect either on demand, or from
 * the start and after any connectivity loss if any stream using the connection
 * has the LWSSSPOLF_NAILED_UP flag.
 */

#if defined(LWS_WITH_SSPLUGINS)
typedef struct lws_ss_plugin {
	struct lws_ss_plugin	*next;
	const char		*name;	/**< auth plugin name */
	size_t			alloc;	/**< size of private allocation */

	int			(*create)(struct lws_ss_handle *ss, void *info,
					  plugin_auth_status_cb status);
				/**< called when the auth plugin is instantiated
				     and bound to the secure stream.  status is
				     called back with advisory information about
				     the authenticated stream state as it
				     proceeds */
	int			(*destroy)(struct lws_ss_handle *ss);
				/**< called when the related secure stream is
				     being destroyed, and anything the auth
				     plugin is doing should also be destroyed */
	int			(*munge)(struct lws_ss_handle *ss, char *path,
					 size_t path_len);
				/**< if the plugin needs to munge transactions
				     that have metadata outside the payload (eg,
				     add http headers) this callback will give
				     it the opportunity to do so */
} lws_ss_plugin_t;
#endif

/* the public, const metrics policy definition */

typedef struct lws_metric_policy {
	/* order of first two mandated by JSON policy parsing scope union */
	const struct lws_metric_policy	*next;
	const char			*name;

	const char			*report;

	/**< the metrics policy name in the policy, used to bind to it */
	uint64_t			us_schedule;
	/**< us interval between lws_system metrics api reports */

	uint32_t			us_decay_unit;
	/**< how many us to decay avg by half, 0 = no decay */
	uint8_t				min_contributors;
	/**< before we can judge something is an outlier */
} lws_metric_policy_t;

typedef struct lws_ss_x509 {
	struct lws_ss_x509	*next;
	const char		*vhost_name; /**< vhost name using cert ctx */
	const uint8_t		*ca_der;	/**< DER x.509 cert */
	size_t			ca_der_len;	/**< length of DER cert */
	uint8_t			keep:1; /**< ie, if used in server tls */
} lws_ss_x509_t;

enum {
	LWSSSPOLF_OPPORTUNISTIC					= (1 << 0),
	/**< the connection doesn't exist unless client asks to write */
	LWSSSPOLF_NAILED_UP					= (1 << 1),
	/**< the connection tries to be connected the whole life of the ss */
	LWSSSPOLF_URGENT_TX					= (1 << 2),
	/**< this connection carries critical tx data */
	LWSSSPOLF_URGENT_RX					= (1 << 3),
	/**< this connection carries critical rx data */
	LWSSSPOLF_TLS						= (1 << 4),
	/**< stream must be connected via a tls tunnel */
	LWSSSPOLF_LONG_POLL					= (1 << 5),
	/**< stream used to receive async rx at arbitrary intervals */
	LWSSSPOLF_AUTH_BEARER					= (1 << 6),
	/**< for http, use lws_system auth token 0 in authentication: bearer */
	LWSSSPOLF_HTTP_NO_CONTENT_LENGTH			= (1 << 7),
	/**< don't add any content length even if we have it */
	LWSSSPOLF_QUIRK_NGHTTP2_END_STREAM			= (1 << 8),
	/**< set the client flag LCCSCF_H2_QUIRK_NGHTTP2_END_STREAM */
	LWSSSPOLF_H2_QUIRK_OVERFLOWS_TXCR			= (1 << 9),
	/**< set the client flag LCCSCF_H2_QUIRK_OVERFLOWS_TXCR */
	LWSSSPOLF_H2_QUIRK_UNCLEAN_HPACK_STATE			= (1 << 10),
	/**< HPACK decoder state does not end cleanly */
	LWSSSPOLF_HTTP_MULTIPART				= (1 << 11),
	/**< indicates stream goes out as specifically a multipart mime POST
	 * section... if the tx has LWSSS_FLAG_COALESCE_CONTINUES flag then more
	 * multipart sections are expected.  Without it, the multipart wrapper
	 * is closed and the http transaction issue completed when this message
	 * finishes. */
	LWSSSPOLF_HTTP_X_WWW_FORM_URLENCODED			= (1 << 12),
	/**< set up lws_system client cert */
	LWSSSPOLF_LOCAL_SINK					= (1 << 13),
	/**< expected to bind to a local sink only */
	LWSSSPOLF_WAKE_SUSPEND__VALIDITY			= (1 << 14),
	/**< this stream's idle validity checks are critical enough we
	 * should arrange to wake from suspend to perform them
	 */
	LWSSSPOLF_SERVER					= (1 << 15),
	/**< we listen on a socket as a server */
	LWSSSPOLF_ALLOW_REDIRECTS				= (1 << 16),
	/**< follow redirects */
	LWSSSPOLF_HTTP_MULTIPART_IN				= (1 << 17),
	/**< handle inbound multipart mime at SS level */

	LWSSSPOLF_ATTR_LOW_LATENCY				= (1 << 18),
	/**< stream requires low latency */
	LWSSSPOLF_ATTR_HIGH_THROUGHPUT				= (1 << 19),
	/**< stream requires high throughput */
	LWSSSPOLF_ATTR_HIGH_RELIABILITY				= (1 << 20),
	/**< stream requires high reliability */
	LWSSSPOLF_ATTR_LOW_COST					= (1 << 21),
	/**< stream is not critical and should be handled as cheap as poss */
	LWSSSPOLF_PERF						= (1 << 22),
	/**< capture and report performace information */
	LWSSSPOLF_DIRECT_PROTO_STR				= (1 << 23),
	/**< metadata as direct protocol string, e.g. http header */
	LWSSSPOLF_HTTP_CACHE_COOKIES				= (1 << 24),
	/**< Record http cookies and pass them back on future requests */
	LWSSSPOLF_PRIORITIZE_READS				= (1 << 25),
	/**< prioritize clearing reads at expense of writes */

};

typedef struct lws_ss_trust_store {
	struct lws_ss_trust_store	*next;
	const char			*name;

	const lws_ss_x509_t		*ssx509[6];
	int				count;
} lws_ss_trust_store_t;

enum {
	LWSSSP_H1,
	LWSSSP_H2,
	LWSSSP_WS,
	LWSSSP_MQTT,
	LWSSSP_RAW,


	LWSSS_HBI_AUTH = 0,
	LWSSS_HBI_DSN,
	LWSSS_HBI_FWV,
	LWSSS_HBI_TYPE,

	_LWSSS_HBI_COUNT /* always last */
};

/*
 * This does for both the static policy metadata entry, and the runtime metadata
 * handling object.
 */

typedef struct lws_ss_metadata {
	struct lws_ss_metadata	*next;
	const char		*name;
	void			*value__may_own_heap;
	size_t			length;

	uint8_t			value_length; /* only valid if set by policy */
	uint8_t			value_is_http_token; /* valid if set by policy */
#if defined(LWS_WITH_SS_DIRECT_PROTOCOL_STR)
	uint8_t			name_on_lws_heap:1;  /* proxy metatadata does this */
#endif
	uint8_t			value_on_lws_heap:1; /* proxy + rx metadata does this */
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
	uint8_t			pending_onward:1;
#endif
} lws_ss_metadata_t;

typedef struct lws_ss_http_respmap {
	uint16_t		resp;	/* the http response code */
	uint16_t		state;	/* low 16-bits of associated state */
} lws_ss_http_respmap_t;

/*
 * This is a mapping between an auth streamtype and a name and other information
 * that can be independently instantiated.  Other streamtypes can indicate they
 * require this authentication on their connection.
 */

typedef struct lws_ss_auth {
	struct lws_ss_auth	*next;
	const char		*name;

	const char		*type;
	const char		*streamtype;
	uint8_t			blob_index;
} lws_ss_auth_t;

/**
 * lws_ss_policy_t: policy database entry for a stream type
 *
 * Decides the system policy for how to implement connections of name
 * .streamtype.
 *
 * Streams may need one kind of auth sequencing for the network connection and
 * another kind of auth sequencing for the streams that are carried inside it,
 * this is the purpose of .nauth and .sauth.  Both are optional and may be NULL.
 *
 * An array of these is set at context creation time, ending with one with a
 * NULL streamtype.
 */
typedef struct lws_ss_policy {
	struct lws_ss_policy	*next;
	const char		*streamtype; /**< stream type lhs to match on */

	const char		*endpoint;   /**< DNS address to connect to */
	const char		*rideshare_streamtype; /**< optional transport
					* on another, preexisting stream of this
					* streamtype name */
	const char		*payload_fmt;
	const char		*socks5_proxy;
	lws_ss_metadata_t	*metadata; /* linked-list of metadata */
	const lws_metric_policy_t *metrics; /* linked-list of metric policies */
	const lws_ss_auth_t	*auth; /* NULL or auth object we bind to */

	/* protocol-specific connection policy details */

	union {

#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2) || defined(LWS_ROLE_WS)

		/* details for http-related protocols... */

		struct {

			/* common to all http-related protocols */

			const char	*method;
			const char	*url;

			const char	*multipart_name;
			const char	*multipart_filename;
			const char	*multipart_content_type;

			const char	*blob_header[_LWSSS_HBI_COUNT];
			const char	*auth_preamble;

			const lws_ss_http_respmap_t *respmap;

			union {
//				struct { /* LWSSSP_H1 */
//				} h1;
//				struct { /* LWSSSP_H2 */
//				} h2;
				struct { /* LWSSSP_WS */
					const char	*subprotocol;
					uint8_t		binary;
					/* false = TEXT, true = BINARY */
				} ws;
			} u;

			uint16_t	resp_expect;
			uint8_t		count_respmap;
			uint8_t		fail_redirect:1;
		} http;

#endif

#if defined(LWS_ROLE_MQTT)

		struct {
			const char	*topic;	    /* stream sends on this topic */
			const char	*subscribe; /* stream subscribes to this topic */

			const char	*will_topic;
			const char	*will_message;

			const char	*birth_topic;
			const char	*birth_message;

			uint16_t	keep_alive;
			uint8_t		qos;
			uint8_t		clean_start;
			uint8_t		will_qos;
			uint8_t		will_retain;
			uint8_t		birth_qos;
			uint8_t		birth_retain;
			uint8_t		aws_iot;
			uint8_t		retain;

		} mqtt;

#endif

		/* details for non-http related protocols... */
	} u;

#if defined(LWS_WITH_SSPLUGINS)
	const
	struct lws_ss_plugin	*plugins[2]; /**< NULL or auth plugin */
	const void		*plugins_info[2];   /**< plugin-specific data */
#endif

#if defined(LWS_WITH_SECURE_STREAMS_AUTH_SIGV4)
	/* directly point to the metadata name, no need to expand */
	const char *aws_region;
	const char *aws_service;
#endif
	/*
	 * We're either a client connection policy that wants a trust store,
	 * or we're a server policy that wants a mem cert and key... Hold
	 * these mutually-exclusive things in a union.
	 */

	union {
		const lws_ss_trust_store_t		*store;
		/**< CA certs needed for conn validation, only set between
		 * policy parsing and vhost creation */
		struct {
			const lws_ss_x509_t		*cert;
			/**< the server's signed cert with the pubkey */
			const lws_ss_x509_t		*key;
			/**< the server's matching private key */
		} server;
	} trust;

	const lws_retry_bo_t	*retry_bo;   /**< retry policy to use */

	uint32_t		proxy_buflen; /**< max dsh alloc for proxy */
	uint32_t		proxy_buflen_rxflow_on_above;
	uint32_t		proxy_buflen_rxflow_off_below;

	uint32_t		client_buflen; /**< max dsh alloc for client */
	uint32_t		client_buflen_rxflow_on_above;
	uint32_t		client_buflen_rxflow_off_below;


	uint32_t		timeout_ms;  /**< default message response
					      * timeout in ms */
	uint32_t		flags;	     /**< stream attribute flags */

	uint16_t		port;	     /**< endpoint port */

	uint8_t			metadata_count;    /**< metadata count */
	uint8_t			protocol;    /**< protocol index */
	uint8_t			client_cert; /**< which client cert to apply
						  0 = none, 1+ = cc 0+ */
	uint8_t			priority;	/* 0 = normal, 6 = max normal,
						 * 7 = network management */
} lws_ss_policy_t;

#if !defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)

/*
 * These only exist / have meaning if there's a dynamic JSON policy enabled
 */

LWS_VISIBLE LWS_EXTERN int
lws_ss_policy_parse_begin(struct lws_context *context, int overlay);

LWS_VISIBLE LWS_EXTERN int
lws_ss_policy_parse_abandon(struct lws_context *context);

LWS_VISIBLE LWS_EXTERN int
lws_ss_policy_parse(struct lws_context *context, const uint8_t *buf, size_t len);

LWS_VISIBLE LWS_EXTERN int
lws_ss_policy_overlay(struct lws_context *context, const char *overlay);

/*
 * You almost certainly don't want these, they return the first policy or auth
 * object in a linked-list of objects created by lws_ss_policy_parse above,
 * they are exported to generate static policy with
 */
LWS_VISIBLE LWS_EXTERN const lws_ss_policy_t *
lws_ss_policy_get(struct lws_context *context);

LWS_VISIBLE LWS_EXTERN const lws_ss_auth_t *
lws_ss_auth_get(struct lws_context *context);

#endif
