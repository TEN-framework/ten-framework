/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2019 - 2020 Andy Green <andy@warmcat.com>
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
 * This is the headers for secure stream api variants that deal with clients in
 * different threads or even different processes.
 *
 * lws_ss_          when client is directly using the event loop
 * lws_sstc_        when client is in a different thread to the event loop
 * lws_sspc_        when client is in a different process to the event loop
 *
 * The client api is almost the same except the slightly diffent names.
 */

/*
 * lws_sspc_ apis... different process
 */

/*
 * Helper translation so user code written to lws_ss_ can be built for
 * lws_sspc_ in one step by #define LWS_SS_USE_SSPC before including
 */

struct lws_sspc_handle;

#if defined(LWS_SS_USE_SSPC)
#define lws_ss_handle			lws_sspc_handle
#define lws_ss_create			lws_sspc_create
#define lws_ss_destroy			lws_sspc_destroy
#define lws_ss_request_tx		lws_sspc_request_tx
#define lws_ss_request_tx_len		lws_sspc_request_tx_len
#define lws_ss_client_connect		lws_sspc_client_connect
#define lws_ss_get_sequencer		lws_sspc_get_sequencer
#define lws_ss_proxy_create		lws_sspc_proxy_create
#define lws_ss_get_context		lws_sspc_get_context
#define lws_ss_rideshare		lws_sspc_rideshare
#define lws_ss_set_metadata		lws_sspc_set_metadata
#define lws_ss_get_metadata		lws_sspc_get_metadata
#define lws_ss_add_peer_tx_credit	lws_sspc_add_peer_tx_credit
#define lws_ss_get_est_peer_tx_credit	lws_sspc_get_est_peer_tx_credit
#define lws_ss_start_timeout		lws_sspc_start_timeout
#define lws_ss_cancel_timeout		lws_sspc_cancel_timeout
#define lws_ss_to_user_object		lws_sspc_to_user_object
#define lws_ss_change_handlers		lws_sspc_change_handlers
#define lws_smd_ss_rx_forward		lws_smd_sspc_rx_forward
#define lws_ss_tag			lws_sspc_tag
#define _lws_fi_user_ss_fi		_lws_fi_user_sspc_fi
#define lwsl_ss_get_cx			lwsl_sspc_get_cx

LWS_VISIBLE LWS_EXTERN void
lws_log_prepend_sspc(struct lws_log_cx *cx, void *obj, char **p, char *e);

LWS_VISIBLE LWS_EXTERN struct lws_log_cx *
lwsl_sspc_get_cx(struct lws_sspc_handle *ss);

#undef lwsl_ss
#define lwsl_ss lwsl_sspc

#undef lwsl_hexdump_ss
#define lwsl_hexdump_ss lwsl_hexdump_sspc
#endif

#define lwsl_sspc(_h, _fil, ...) \
		 _lws_log_cx(lwsl_sspc_get_cx(_h), lws_log_prepend_sspc, _h, \
					_fil, __func__, __VA_ARGS__)

#define lwsl_hexdump_sspc(_h, _fil, _buf, _len) \
		lwsl_hexdump_level_cx(lwsl_sspc_get_cx(_h), \
				      lws_log_prepend_sspc, \
				      _h, _fil, _buf, _len)

/*
 * lwsl_sspc
 */

#if (_LWS_ENABLED_LOGS & LLL_ERR)
#define lwsl_sspc_err(_w, ...) lwsl_sspc(_w, LLL_ERR, __VA_ARGS__)
#else
#define lwsl_sspc_err(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_WARN)
#define lwsl_sspc_warn(_w, ...) lwsl_sspc(_w, LLL_WARN, __VA_ARGS__)
#else
#define lwsl_sspc_warn(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_NOTICE)
#define lwsl_sspc_notice(_w, ...) lwsl_sspc(_w, LLL_NOTICE, __VA_ARGS__)
#else
#define lwsl_sspc_notice(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_INFO)
#define lwsl_sspc_info(_w, ...) lwsl_sspc(_w, LLL_INFO, __VA_ARGS__)
#else
#define lwsl_sspc_info(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_DEBUG)
#define lwsl_sspc_debug(_w, ...) lwsl_sspc(_w, LLL_DEBUG, __VA_ARGS__)
#else
#define lwsl_sspc_debug(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_PARSER)
#define lwsl_sspc_parser(_w, ...) lwsl_sspc(_w, LLL_PARSER, __VA_ARGS__)
#else
#define lwsl_sspc_parser(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_HEADER)
#define lwsl_sspc_header(_w, ...) lwsl_sspc(_w, LLL_HEADER, __VA_ARGS__)
#else
#define lwsl_sspc_header(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_EXT)
#define lwsl_sspc_ext(_w, ...) lwsl_sspc(_w, LLL_EXT, __VA_ARGS__)
#else
#define lwsl_sspc_ext(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_CLIENT)
#define lwsl_sspc_client(_w, ...) lwsl_sspc(_w, LLL_CLIENT, __VA_ARGS__)
#else
#define lwsl_sspc_client(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_LATENCY)
#define lwsl_sspc_latency(_w, ...) lwsl_sspc(_w, LLL_LATENCY, __VA_ARGS__)
#else
#define lwsl_sspc_latency(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_THREAD)
#define lwsl_sspc_thread(_w, ...) lwsl_sspc(_w, LLL_THREAD, __VA_ARGS__)
#else
#define lwsl_sspc_thread(_w, ...) do {} while(0)
#endif

#if (_LWS_ENABLED_LOGS & LLL_USER)
#define lwsl_sspc_user(_w, ...) lwsl_sspc(_w, LLL_USER, __VA_ARGS__)
#else
#define lwsl_sspc_user(_w, ...) do {} while(0)
#endif

#define lwsl_hexdump_sspc_err(_v, ...)    lwsl_hexdump_sspc(_v, LLL_ERR, __VA_ARGS__)
#define lwsl_hexdump_sspc_warn(_v, ...)   lwsl_hexdump_sspc(_v, LLL_WARN, __VA_ARGS__)
#define lwsl_hexdump_sspc_notice(_v, ...) lwsl_hexdump_sspc(_v, LLL_NOTICE, __VA_ARGS__)
#define lwsl_hexdump_sspc_info(_v, ...)   lwsl_hexdump_sspc(_v, LLL_INFO, __VA_ARGS__)
#define lwsl_hexdump_sspc_debug(_v, ...)  lwsl_hexdump_sspc(_v, LLL_DEBUG, __VA_ARGS__)


LWS_VISIBLE LWS_EXTERN int
lws_sspc_create(struct lws_context *context, int tsi, const lws_ss_info_t *ssi,
		void *opaque_user_data, struct lws_sspc_handle **ppss,
		struct lws_sequencer *seq_owner, const char **ppayload_fmt);

/**
 * lws_sspc_destroy() - Destroy secure stream
 *
 * \param ppss: pointer to lws_ss_t pointer to be destroyed
 *
 * Destroys the lws_ss_t pointed to by *ppss, and sets *ppss to NULL.
 */
LWS_VISIBLE LWS_EXTERN void
lws_sspc_destroy(struct lws_sspc_handle **ppss);

/**
 * lws_sspc_request_tx() - Schedule stream for tx
 *
 * \param pss: pointer to lws_ss_t representing stream that wants to transmit
 *
 * Schedules a write on the stream represented by \p pss.  When it's possible to
 * write on this stream, the *tx callback will occur with an empty buffer for
 * the stream owner to fill in.
 */
LWS_VISIBLE LWS_EXTERN lws_ss_state_return_t
lws_sspc_request_tx(struct lws_sspc_handle *pss);

/**
 * lws_sspc_request_tx_len() - Schedule stream for tx with length hint
 *
 * \param h: pointer to handle representing stream that wants to transmit
 * \param len: the length of the write in bytes
 *
 * Schedules a write on the stream represented by \p pss.  When it's possible to
 * write on this stream, the *tx callback will occur with an empty buffer for
 * the stream owner to fill in.
 *
 * This api variant should be used when it's possible the payload will go out
 * over h1 with x-web-form-urlencoded or similar Content-Type.
 *
 * The serialized, sspc type api actually serializes and forwards the length
 * hint to its upstream proxy, where it's available for use to produce the
 * internet-capable protocol framing.
 */
LWS_VISIBLE LWS_EXTERN lws_ss_state_return_t
lws_sspc_request_tx_len(struct lws_sspc_handle *h, unsigned long len);

/**
 * lws_sspc_client_connect() - Attempt the client connect
 *
 * \param h: secure streams handle
 *
 * Starts the connection process for the secure stream.  Returns 0.
 */
LWS_VISIBLE LWS_EXTERN lws_ss_state_return_t
lws_sspc_client_connect(struct lws_sspc_handle *h);

/**
 * lws_sspc_get_sequencer() - Return parent sequencer pointer if any
 *
 * \param h: secure streams handle
 *
 * Returns NULL if the secure stream is not associated with a sequencer.
 * Otherwise returns a pointer to the owning sequencer.  You can use this to
 * identify which sequencer to direct messages to, from the secure stream
 * callback.
 */
LWS_VISIBLE LWS_EXTERN struct lws_sequencer *
lws_sspc_get_sequencer(struct lws_sspc_handle *h);

/**
 * lws_sspc_proxy_create() - Start a unix domain socket proxy for Secure Streams
 *
 * \param context: lws_context
 *
 * Creates a vhost that listens on an abstract namespace unix domain socket at
 * address "proxy.ss.lws".  Client connections to this proxy to Secure Streams
 */
LWS_VISIBLE LWS_EXTERN int
lws_sspc_proxy_create(struct lws_context *context);

/**
 * lws_ss_get_context() - convenience helper to recover the lws context
 *
 * \h: secure streams handle
 *
 * Returns the lws context.  Dispenses with the need to pass a copy of it into
 * your secure streams handler.
 */

LWS_VISIBLE LWS_EXTERN struct lws_context *
lws_sspc_get_context(struct lws_sspc_handle *h);

LWS_VISIBLE extern const struct lws_protocols lws_sspc_protocols[2];

LWS_VISIBLE LWS_EXTERN const char *
lws_sspc_rideshare(struct lws_sspc_handle *h);


/**
 * lws_sspc_set_metadata() - allow user to bind external data to defined ss metadata
 *
 * \h: secure streams handle
 * \name: metadata name from the policy
 * \value: pointer to user-managed data to bind to name
 * \len: length of the user-managed data in value
 *
 * Binds user-managed data to the named metadata item from the ss policy.
 * If present, the metadata item is handled in a protocol-specific way using
 * the associated policy information.  For example, in the policy
 *
 *  	"\"metadata\":"		"["
 *		"{\"uptag\":"  "\"X-Upload-Tag:\"},"
 *		"{\"ctype\":"  "\"Content-Type:\"},"
 *		"{\"xctype\":" "\"X-Content-Type:\"}"
 *	"],"
 *
 * when the policy is using h1 is interpreted to add h1 headers of the given
 * name with the value of the metadata on the left.
 *
 * Return 0 if OK, or nonzero if failed.
 */
LWS_VISIBLE LWS_EXTERN int
lws_sspc_set_metadata(struct lws_sspc_handle *h, const char *name,
		      const void *value, size_t len);

LWS_VISIBLE LWS_EXTERN int
lws_sspc_get_metadata(struct lws_sspc_handle *h, const char *name,
		      const void **value, size_t *len);

LWS_VISIBLE LWS_EXTERN int
lws_sspc_add_peer_tx_credit(struct lws_sspc_handle *h, int32_t add);

LWS_VISIBLE LWS_EXTERN int
lws_sspc_get_est_peer_tx_credit(struct lws_sspc_handle *h);

LWS_VISIBLE LWS_EXTERN void
lws_sspc_start_timeout(struct lws_sspc_handle *h, unsigned int timeout_ms);

LWS_VISIBLE LWS_EXTERN void
lws_sspc_cancel_timeout(struct lws_sspc_handle *h);

LWS_VISIBLE LWS_EXTERN void *
lws_sspc_to_user_object(struct lws_sspc_handle *h);

LWS_VISIBLE LWS_EXTERN void
lws_sspc_change_handlers(struct lws_sspc_handle *h,
	lws_ss_state_return_t (*rx)(void *userobj, const uint8_t *buf,
				    size_t len, int flags),
	lws_ss_state_return_t (*tx)(void *userobj, lws_ss_tx_ordinal_t ord,
				    uint8_t *buf, size_t *len, int *flags),
	lws_ss_state_return_t (*state)(void *userobj, void *h_src
					/* ss handle type */,
				       lws_ss_constate_t state,
				       lws_ss_tx_ordinal_t ack));

const char *
lws_sspc_tag(struct lws_sspc_handle *h);
