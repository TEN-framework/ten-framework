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
 * included from libwebsockets.h
 *
 *
 * Secure Streams is a *payload-only* client communication channel where all the
 * details about the connection are held in a systemwide policy database and
 * are keyed by the streamtype field... the user of the communication channel
 * does not know or manage the choice of endpoint, tls CA, or even wire
 * protocol.  The advantage is he then does not have any dependency on any of
 * those and they can be changed just by changing the policy database without
 * touching the code using the stream.
 *
 * There are two ways secure streams interfaces to user code:
 *
 * 1) [Linux / RTOS] the natural, smallest interface is to call back to user
 *    code that only operates directly from the lws event loop thread context
 *    (direct callbacks from lws_ss_t)
 *
 *    lws_thread( [user code] ---- lws )
 *
 * 2) [Linux] where the user code is in a different process and communicates
 *    asynchronously via a proxy socket
 *
 *    user_process{ [user code] | shim | socket-}------ lws_process{ lws }
 *
 * In the second, IPC, case, all packets are prepended by one or more bytes
 * indicating the packet type and serializing any associated data, known as
 * Serialized Secure Streams or SSS.
 *
 * Serialized Secure Streams
 * -------------------------
 *
 * On the transport, adjacent packets may be coalesced, that is, the original
 * packet sizes are lost and two or more packets are combined.  For that reason
 * the serialization format always contains a 1-byte type and then a 2-byte
 * frame length.
 *
 * Client to proxy
 *
 * - Proxied connection setup
 *
 *   -  0: LWSSS_SER_TXPRE_STREAMTYPE
 *   -  1: 2-byte MSB-first rest-of-frame length
 *   -  3: 1-byte Client SSS protocol version (introduced in SSSv1)
 *   -  4: 4-byte Client PID (introduced in SSSv1)
 *   -  8: 4-byte MSB-first initial tx credit
 *   - 12: the streamtype name with no NUL
 *
 * - Proxied tx
 *
 *   -  0: LWSSS_SER_TXPRE_TX_PAYLOAD
 *   -  1: 2 byte MSB-first rest-of-frame length
 *   -  3: 4-byte MSB-first flags
 *   -  7: 4-byte MSB-first us between client requested write and wrote to proxy
 *   - 11: 8-byte MSB-first us resolution unix time client wrote to proxy
 *   - 19: payload
 *
 * - Proxied secure stream destroy
 *
 *   -  0: LWSSS_SER_TXPRE_DESTROYING
 *   -  1: 00, 00
 *
 * - Proxied metadata - sent when one metadata item set clientside
 *
 *   -  0: LWSSS_SER_TXPRE_METADATA
 *   -  1: 2-byte MSB-first rest-of-frame length
 *   -  3: 1-byte metadata name length
 *   -  4: metadata name
 *   -  ...: metadata value (for rest of packet)
 *
 * - TX credit management - sent when using tx credit apis, cf METADATA
 *
 *   - 0: LWSSS_SER_TXPRE_TXCR_UPDATE
 *   - 1: 2-byte MSB-first rest-of-frame length 00, 04
 *   - 3: 4-byte additional tx credit adjust value
 *
 * - Stream timeout management - forwarded when user applying or cancelling t.o.
 *
 *   -  0: LWSSS_SER_TXPRE_TIMEOUT_UPDATE
 *   -  1: 2-byte MSB-first rest-of-frame length 00, 04
 *   -  3: 4-byte MSB-first unsigned 32-bit timeout, 0 = use policy, -1 = cancel
 *
 * - Passing up payload length hint
 *
 *   -  0: LWSSS_SER_TXPRE_PAYLOAD_LENGTH_HINT
 *   -  1: 2-byte MSB-first rest-of-frame length 00, 04
 *   -  3: 4-byte MSB-first unsigned 32-bit payload length hint
 *
 * Proxy to client
 *
 * - Proxied connection setup result
 *
 *   -  0: LWSSS_SER_RXPRE_CREATE_RESULT
 *   -  1: 2 byte MSB-first rest-of-frame length (usually 00, 03)
 *   -  3: 1 byte result, 0 = success.  On failure, proxy will close connection.
 *   -  4: 4 byte client dsh allocation recommended for stream type, from policy
 *         (introduced in SSSv1)
 *   -  8: 2 byte MSB-first initial tx credit
 *   - 10: if present, comma-sep list of rideshare types from policy
 *
 * - Proxied rx
 *
 *   -  0: LWSSS_SER_RXPRE_RX_PAYLOAD
 *   -  1: 2 byte MSB-first rest-of-frame length
 *   -  3: 4-byte MSB-first flags
 *   -  7: 4-byte MSB-first us between inbound read and wrote to client
 *   - 11: 8-byte MSB-first us resolution unix time proxy wrote to client
 *   - 17: (rideshare name len + rideshare name if flags & LWSSS_FLAG_RIDESHARE)
 *          payload
 *
 * - Proxied tx credit
 *
 *   -  0: LWSSS_SER_RXPRE_TXCR_UPDATE
 *   -  1: 00, 04
 *   -  3: 4-byte MSB-first addition tx credit bytes
 *
 * - Proxied rx metadata
 *
 *   -  0: LWSSS_SER_RXPRE_METADATA
 *   -  1: 2-byte MSB-first rest-of-frame length
 *   -  3: 1-byte metadata name length
 *   -  4: metadata name
 *   -  ...: metadata value (for rest of packet)
 *
 * - Proxied state (8 or 11 byte packet)
 *
 *   -  0: LWSSS_SER_RXPRE_CONNSTATE
 *   -  1: 00, 05 if state < 256, else 00, 08
 *   -  3: 1 byte state index if state < 256, else 4-byte MSB-first state index
 *   -  4 or 7: 4-byte MSB-first ordinal
 *
 * - Proxied performance information
 *
 *   -  0: LWSSS_SER_RXPRE_PERF
 *   -  1: 2-byte MSB-first rest-of-frame length
 *   -  3: ... performance JSON (for rest of packet)
 *
 * Proxied tx may be read by the proxy but rejected due to lack of buffer space
 * at the proxy.  For that reason, tx must be held at the sender until it has
 * been acknowledged or denied.
 *
 * Sinks
 * -----
 *
 * Sinks are logical "servers", you can register as a sink for a particular
 * streamtype by using the lws_ss_create() api with ssi->register_sink set to 1.
 *
 * For directly fulfilled Secure Streams, new streams of that streamtype bind
 * to the rx, tx and state handlers given when it was registered.
 *
 *  - When new streams are created the registered sink handler for (*state) is
 *    called with event LWSSSCS_SINK_JOIN and the new client stream handle in
 *    the h_src parameter.
 *
 *  - When the client stream sends something to the sink, it calls the sink's
 *    (*rx) with the client stream's
 */

/** \defgroup secstr Secure Streams
* ##Secure Streams
*
* Secure Streams related apis
*/
///@{

#define LWS_SS_MTU 1540

struct lws_ss_handle;
typedef uint32_t lws_ss_tx_ordinal_t;

/*
 * connection state events
 *
 * If you add states, take care about the state names and state transition
 * validity enforcement tables too
 */
typedef enum {
	/* zero means unset */
	LWSSSCS_CREATING		= 1,
	LWSSSCS_DISCONNECTED,
	LWSSSCS_UNREACHABLE,		/* oridinal arg = 1 = caused by dns
					 * server reachability failure */
	LWSSSCS_AUTH_FAILED,
	LWSSSCS_CONNECTED,
	LWSSSCS_CONNECTING,
	LWSSSCS_DESTROYING,
	LWSSSCS_POLL,
	LWSSSCS_ALL_RETRIES_FAILED,	/* all retries in bo policy failed */
	LWSSSCS_QOS_ACK_REMOTE,		/* remote peer received and acked tx */
	LWSSSCS_QOS_NACK_REMOTE,
	LWSSSCS_QOS_ACK_LOCAL,		/* local proxy accepted our tx */
	LWSSSCS_QOS_NACK_LOCAL,		/* local proxy refused our tx */
	LWSSSCS_TIMEOUT,		/* optional timeout timer fired */

	LWSSSCS_SERVER_TXN,
	LWSSSCS_SERVER_UPGRADE,		/* the server protocol upgraded */

	LWSSSCS_EVENT_WAIT_CANCELLED, /* somebody called lws_cancel_service */

	LWSSSCS_UPSTREAM_LINK_RETRY,	/* if we are being proxied over some
					 * intermediate link, this transient
					 * state may be sent to indicate we are
					 * waiting to establish that link before
					 * creation can proceed.. ack is the
					 * number of ms we have been trying */

	LWSSSCS_SINK_JOIN,		/* sinks get this when a new source
					 * stream joins the sink */
	LWSSSCS_SINK_PART,		/* sinks get this when a new source
					 * stream leaves the sink */

	LWSSSCS_USER_BASE = 1000
} lws_ss_constate_t;

enum {
	LWSSS_FLAG_SOM						= (1 << 0),
	/* payload contains the start of new message */
	LWSSS_FLAG_EOM						= (1 << 1),
	/* payload contains the end of message */
	LWSSS_FLAG_POLL						= (1 << 2),
	/* Not a real transmit... poll for rx if protocol needs it */
	LWSSS_FLAG_RELATED_START				= (1 << 3),
	/* Appears in a zero-length message indicating a message group of zero
	 * or more messages is now starting. */
	LWSSS_FLAG_RELATED_END					= (1 << 4),
	/* Appears in a zero-length message indicating a message group of zero
	 * or more messages has now finished. */
	LWSSS_FLAG_RIDESHARE					= (1 << 5),
	/* Serialized payload starts with non-default rideshare name length and
	 * name string without NUL, then payload */
	LWSSS_FLAG_PERF_JSON					= (1 << 6),
	/* This RX is JSON performance data, only on streams with "perf" flag
	 * set */

	/*
	 * In the case the secure stream is proxied across a process or thread
	 * boundary, eg by proxying through a socket for IPC, metadata must be
	 * carried in-band.  A byte is prepended to each rx payload to
	 * differentiate what it is.
	 *
	 * Secure streams where the user is called back directly does not need
	 * any of this and only pure payloads are passed.
	 *
	 * rx (received by client) prepends for proxied connections
	 */

	LWSSS_SER_RXPRE_RX_PAYLOAD				= 0x55,
	LWSSS_SER_RXPRE_CREATE_RESULT,
	LWSSS_SER_RXPRE_CONNSTATE,
	LWSSS_SER_RXPRE_TXCR_UPDATE,
	LWSSS_SER_RXPRE_METADATA,
	LWSSS_SER_RXPRE_TLSNEG_ENCLAVE_SIGN,
	LWSSS_SER_RXPRE_PERF,

	/* tx (send by client) prepends for proxied connections */

	LWSSS_SER_TXPRE_STREAMTYPE				= 0xaa,
	LWSSS_SER_TXPRE_ONWARD_CONNECT,
	LWSSS_SER_TXPRE_DESTROYING,
	LWSSS_SER_TXPRE_TX_PAYLOAD,
	LWSSS_SER_TXPRE_METADATA,
	LWSSS_SER_TXPRE_TXCR_UPDATE,
	LWSSS_SER_TXPRE_TIMEOUT_UPDATE,
	LWSSS_SER_TXPRE_PAYLOAD_LENGTH_HINT,
	LWSSS_SER_TXPRE_TLSNEG_ENCLAVE_SIGNED,
};

typedef enum {
	LPCSPROX_WAIT_INITIAL_TX = 1, /* after connect, must send streamtype */
	LPCSPROX_REPORTING_FAIL, /* stream creation failed, wait to to tell */
	LPCSPROX_REPORTING_OK, /* stream creation succeeded, wait to to tell */
	LPCSPROX_OPERATIONAL, /* ready for payloads */
	LPCSPROX_DESTROYED,

	LPCSCLI_SENDING_INITIAL_TX,  /* after connect, must send streamtype */
	LPCSCLI_WAITING_CREATE_RESULT,   /* wait to hear if proxy ss create OK */
	LPCSCLI_LOCAL_CONNECTED,	      /* we are in touch with the proxy */
	LPCSCLI_ONWARD_CONNECT,	      /* request onward ss connection */
	LPCSCLI_OPERATIONAL, /* ready for payloads */

} lws_ss_conn_states_t;

/*
 * Returns from state() callback can tell the caller what the user code
 * wants to do
 */

typedef enum lws_ss_state_return {
	LWSSSSRET_TX_DONT_SEND		=  1, /* (*tx) only, or failure */

	LWSSSSRET_OK			=  0, /* no error */
	LWSSSSRET_DISCONNECT_ME		= -1, /* caller should disconnect us */
	LWSSSSRET_DESTROY_ME		= -2, /* caller should destroy us */
} lws_ss_state_return_t;

/**
 * lws_ss_info_t: information about stream to be created
 *
 * Prepare this struct with information about what the stream type is and how
 * the stream should interface with your code, and pass it to lws_ss_create()
 * to create the requested stream.
 */

enum {
	LWSSSINFLAGS_REGISTER_SINK			=	(1 << 0),
	/**< If set, we're not creating a specific stream, but registering
	 * ourselves as the "sink" for .streamtype.  It's analogous to saying
	 * we want to be the many-to-one "server" for .streamtype; when other
	 * streams are created with that streamtype, they should be forwarded
	 * to this stream owner, where they join and part from the sink via
	 * (*state) LWSSSCS_SINK_JOIN / _PART events, the new client handle
	 * being provided in the h_src parameter.
	 */
	LWSSSINFLAGS_PROXIED				=	(1 << 1),
	/**< Set if the stream is being created as a stand-in at the proxy */
	LWSSSINFLAGS_SERVER				=	(1 << 2),
	/**< Set on the server object copy of the ssi / info to indicate that
	 * stream creation using this ssi is for Accepted connections belonging
	 * to a server */
	LWSSSINFLAGS_ACCEPTED				=	(1 << 3),
	/**< Set on the accepted object copy of the ssi / info to indicate that
	 * we are an accepted connection from a server's listening socket */
};

typedef lws_ss_state_return_t (*lws_sscb_rx)(void *userobj, const uint8_t *buf,
					     size_t len, int flags);
typedef lws_ss_state_return_t (*lws_sscb_tx)(void *userobj,
					     lws_ss_tx_ordinal_t ord,
					     uint8_t *buf, size_t *len,
					     int *flags);
typedef lws_ss_state_return_t (*lws_sscb_state)(void *userobj, void *h_src,
						lws_ss_constate_t state,
						lws_ss_tx_ordinal_t ack);

#if defined(LWS_WITH_SECURE_STREAMS_BUFFER_DUMP)
typedef void (*lws_ss_buffer_dump_cb)(void *userobj, const uint8_t *buf,
		size_t len, int done);
#endif

struct lws_ss_policy;

typedef struct lws_ss_info {
	const char *streamtype; /**< type of stream we want to create */
	size_t	    user_alloc; /**< size of user allocation */
	size_t	    handle_offset; /**< offset of handle stg in user_alloc type,
				    set to offsetof(mytype, my_handle_member) */
	size_t	    opaque_user_data_offset;
	/**< offset of opaque user data ptr in user_alloc type, set to
	     offsetof(mytype, opaque_ud_member) */

#if defined(LWS_WITH_SECURE_STREAMS_CPP)
	const struct lws_ss_policy	*policy;
	/**< Normally NULL, or a locally-generated policy to apply to this
	 * connection instead of a named streamtype */
#endif

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	lws_fi_ctx_t				fic;
	/**< Attach external Fault Injection context to the stream, hierarchy
	 * is ss->context */
#endif

	lws_sscb_rx rx;
	/**< callback with rx payload for this stream */
	lws_sscb_tx tx;
	/**< callback to send payload on this stream... 0 = send as set in
	 * len and flags, 1 = do not send anything (ie, not even 0 len frame) */
	lws_sscb_state state;
	/**< advisory cb about state of stream and QoS status if applicable...
	 * h_src is only used with sinks and LWSSSCS_SINK_JOIN/_PART events.
	 * Return nonzero to indicate you want to destroy the stream. */
#if defined(LWS_WITH_SECURE_STREAMS_BUFFER_DUMP)
	lws_ss_buffer_dump_cb dump;
	/**< cb to record needed protocol buffer data*/
#endif
	int	    manual_initial_tx_credit;
	/**< 0 = manage any tx credit automatically, nonzero explicitly sets the
	 * peer stream to have the given amount of tx credit, if the protocol
	 * can support it.
	 *
	 * In the special case of _lws_smd streamtype, this is used to indicate
	 * the connection's rx class mask.
	 * */
	uint32_t	client_pid;
	/**< used in proxy / serialization case to hold the client pid this
	 * proxied connection is to be tagged with
	 */
	uint8_t		flags;
	uint8_t		sss_protocol_version;
	/**< used in proxy / serialization case to hold the SS serialization
	 * protocol level to use with this peer... clients automatically request
	 * the most recent version they were built with
	 * (LWS_SSS_CLIENT_PROTOCOL_VERSION) and the proxy stores the requested
	 * version in here
	 */

} lws_ss_info_t;

/**
 * lws_ss_create() - Create secure stream
 *
 * \param context: the lws context to create this inside
 * \param tsi: service thread index to create on (normally 0)
 * \param ssi: pointer to lws_ss_info_t filled in with info about desired stream
 * \param opaque_user_data: opaque data to set in the stream's user object
 * \param ppss: pointer to secure stream handle pointer set on exit
 * \param ppayload_fmt: NULL or pointer to a string ptr to take payload format
 *			name from the policy
 *
 * Requests a new secure stream described by \p ssi be created.  If successful,
 * the stream is created, its state callback called with LWSSSCS_CREATING, \p *ppss
 * is set to point to the handle, and it returns 0.  If it failed, it returns
 * nonzero.
 *
 * Along with the opaque stream object, streams overallocate
 *
 * 1) a user data struct whose size is set in ssi
 * 2) nauth plugin instantiation data (size set in the plugin struct)
 * 3) sauth plugin instantiation data (size set in the plugin struct)
 * 4) space for a copy of the stream type name
 *
 * The user data struct is initialized to all zeros, then the .handle_offset and
 * .opaque_user_data_offset fields of the ssi are used to prepare the user data
 * struct with the ss handle that was created, and a copy of the
 * opaque_user_data pointer given as an argument.
 *
 * If you want to set up the stream with specific information, point to it in
 * opaque_user_data and use the copy of that pointer in your user data member
 * for it starting from the LWSSSCS_CREATING state call.
 *
 * Since different endpoints chosen by the policy may require different payload
 * formats, \p ppayload_fmt is set to point to the name of the needed payload
 * format from the policy database if non-NULL.
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_ss_create(struct lws_context *context, int tsi, const lws_ss_info_t *ssi,
	      void *opaque_user_data, struct lws_ss_handle **ppss,
	      struct lws_sequencer *seq_owner, const char **ppayload_fmt);

/**
 * lws_ss_destroy() - Destroy secure stream
 *
 * \param ppss: pointer to lws_ss_t pointer to be destroyed
 *
 * Destroys the lws_ss_t pointed to by \p *ppss, and sets \p *ppss to NULL.
 */
LWS_VISIBLE LWS_EXTERN void
lws_ss_destroy(struct lws_ss_handle **ppss);

/**
 * lws_ss_request_tx() - Schedule stream for tx
 *
 * \param pss: pointer to lws_ss_t representing stream that wants to transmit
 *
 * Schedules a write on the stream represented by \p pss.  When it's possible to
 * write on this stream, the \p *tx callback will occur with an empty buffer for
 * the stream owner to fill in.
 *
 * Returns 0 or LWSSSSRET_DESTROY_ME
 */
LWS_VISIBLE LWS_EXTERN lws_ss_state_return_t LWS_WARN_UNUSED_RESULT
lws_ss_request_tx(struct lws_ss_handle *pss);

/**
 * lws_ss_request_tx() - Schedule stream for tx
 *
 * \param pss: pointer to lws_ss_t representing stream that wants to transmit
 * \param len: the length of the write in bytes
 *
 * Schedules a write on the stream represented by \p pss.  When it's possible to
 * write on this stream, the \p *tx callback will occur with an empty buffer for
 * the stream owner to fill in.
 *
 * This api variant should be used when it's possible the payload will go out
 * over h1 with x-web-form-urlencoded or similar Content-Type.
 */
LWS_VISIBLE LWS_EXTERN lws_ss_state_return_t LWS_WARN_UNUSED_RESULT
lws_ss_request_tx_len(struct lws_ss_handle *pss, unsigned long len);

/**
 * lws_ss_client_connect() - Attempt the client connect
 *
 * \param h: secure streams handle
 *
 * Starts the connection process for the secure stream.
 *
 * Can return any of the lws_ss_state_return_t values depending on user
 * state callback returns.
 *
 * LWSSSSRET_OK means the connection is ongoing.
 *
 */
LWS_VISIBLE LWS_EXTERN lws_ss_state_return_t LWS_WARN_UNUSED_RESULT
lws_ss_client_connect(struct lws_ss_handle *h);

/**
 * lws_ss_get_sequencer() - Return parent sequencer pointer if any
 *
 * \param h: secure streams handle
 *
 * Returns NULL if the secure stream is not associated with a sequencer.
 * Otherwise returns a pointer to the owning sequencer.  You can use this to
 * identify which sequencer to direct messages to, from the secure stream
 * callback.
 */
LWS_VISIBLE LWS_EXTERN struct lws_sequencer *
lws_ss_get_sequencer(struct lws_ss_handle *h);

/**
 * lws_ss_proxy_create() - Start a unix domain socket proxy for Secure Streams
 *
 * \param context: lws_context
 * \param bind: if port is 0, unix domain path with leading @ for abstract.
 *		if port nonzero, NULL, or network interface to bind listen to
 * \param port: tcp port to listen on
 *
 * Creates a vhost that listens either on an abstract namespace unix domain
 * socket (port = 0) or a tcp listen socket (port nonzero).  If bind is NULL
 * and port is 0, the abstract unix domain socket defaults to "proxy.ss.lws".
 *
 * Client connections to this proxy to Secure Streams are fulfilled using the
 * policy local to the proxy and the data passed between the client and the
 * proxy using serialized Secure Streams protocol.
 */
LWS_VISIBLE LWS_EXTERN int
lws_ss_proxy_create(struct lws_context *context, const char *bind, int port);

/**
 * lws_ss_state_name() - convenience helper to get a printable conn state name
 *
 * \param state: the connection state index
 *
 * Returns a printable name for the connection state index passed in.
 */
LWS_VISIBLE LWS_EXTERN const char *
lws_ss_state_name(int state);

/**
 * lws_ss_get_context() - convenience helper to recover the lws context
 *
 * \param h: secure streams handle
 *
 * Returns the lws context.  Dispenses with the need to pass a copy of it into
 * your secure streams handler.
 */
LWS_VISIBLE LWS_EXTERN struct lws_context *
lws_ss_get_context(struct lws_ss_handle *h);

#define LWSSS_TIMEOUT_FROM_POLICY				0

/**
 * lws_ss_start_timeout() - start or restart the timeout on the stream
 *
 * \param h: secure streams handle
 * \param timeout_ms: LWSSS_TIMEOUT_FROM_POLICY for policy value, else use timeout_ms
 *
 * Starts or restarts the stream's own timeout timer.  If the specified time
 * passes without lws_ss_cancel_timeout() being called on the stream, then the
 * stream state callback receives LWSSSCS_TIMEOUT
 *
 * The process being protected by the timeout is up to the user code, it may be
 * arbitrarily long and cross multiple protocol transactions or involve other
 * streams.  It's up to the user to decide when to start and when / if to cancel
 * the stream timeout.
 */
LWS_VISIBLE LWS_EXTERN void
lws_ss_start_timeout(struct lws_ss_handle *h, unsigned int timeout_ms);

/**
 * lws_ss_cancel_timeout() - remove any timeout on the stream
 *
 * \param h: secure streams handle
 *
 * Disable any timeout that was applied to the stream by lws_ss_start_timeout().
 */
LWS_VISIBLE LWS_EXTERN void
lws_ss_cancel_timeout(struct lws_ss_handle *h);

/**
 * lws_ss_to_user_object() - convenience helper to get user object from handle
 *
 * \param h: secure streams handle
 *
 * Returns the user allocation related to the handle.  Normally you won't need
 * this since it's available in the rx, tx and state callbacks as "userdata"
 * already.
 */
LWS_VISIBLE LWS_EXTERN void *
lws_ss_to_user_object(struct lws_ss_handle *h);

/**
 * lws_ss_rideshare() - find the current streamtype when types rideshare
 *
 * \param h: the stream handle
 *
 * Under some conditions, the payloads may be structured using protocol-
 * specific formatting, eg, http multipart mime.  It's possible to map the
 * logical partitions in the payload to different stream types using
 * the policy "rideshare" feature.
 *
 * This api lets the callback code find out which rideshare stream type the
 * current payload chunk belongs to.
 */
LWS_VISIBLE LWS_EXTERN const char *
lws_ss_rideshare(struct lws_ss_handle *h);


/**
 * lws_ss_set_metadata() - allow user to bind external data to defined ss metadata
 *
 * \param h: secure streams handle
 * \param name: metadata name from the policy
 * \param value: pointer to user-managed data to bind to name
 * \param len: length of the user-managed data in value
 *
 * Binds user-managed data to the named metadata item from the ss policy.
 * If present, the metadata item is handled in a protocol-specific way using
 * the associated policy information.  For example, in the policy
 *
 *  	"\"metadata\":"		"["
 *		"{\"uptag\":"  "\"X-Upload-Tag:\"},"
 *		"{\"ctype\":"  "\"Content-Type:\"},"
 *		"{\"xctype\":" "\"\"}"
 *	"],"
 *
 * when the policy is using h1 is interpreted to add h1 headers of the given
 * name with the value of the metadata on the left.
 *
 * Return 0 if OK or nonzero if, eg, metadata name does not exist on the
 * streamtype.  You must check the result of this, eg, transient OOM can cause
 * these to fail and you should retry later.
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_ss_set_metadata(struct lws_ss_handle *h, const char *name,
		    const void *value, size_t len);

/**
 * lws_ss_alloc_set_metadata() - copy data and bind to ss metadata
 *
 * \param h: secure streams handle
 * \param name: metadata name from the policy
 * \param value: pointer to user-managed data to bind to name
 * \param len: length of the user-managed data in value
 *
 * Same as lws_ss_set_metadata(), but allocates a heap buffer for the data
 * first and takes a copy of it, so the original can go out of scope
 * immediately after.
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_ss_alloc_set_metadata(struct lws_ss_handle *h, const char *name,
			  const void *value, size_t len);

/**
 * lws_ss_get_metadata() - get current value of stream metadata item
 *
 * \param h: secure streams handle
 * \param name: metadata name from the policy
 * \param value: pointer to pointer to be set to point at the value
 * \param len: pointer to size_t to set to the length of the value
 *
 * Binds user-managed data to the named metadata item from the ss policy.
 * If present, the metadata item is handled in a protocol-specific way using
 * the associated policy information.  For example, in the policy
 *
 *  	"\"metadata\":"		"["
 *		"{\"uptag\":"  "\"X-Upload-Tag:\"},"
 *		"{\"ctype\":"  "\"Content-Type:\"},"
 *		"{\"xctype\":" "\"\"}"
 *	"],"
 *
 * when the policy is using h1 is interpreted to add h1 headers of the given
 * name with the value of the metadata on the left.
 *
 * Return 0 if \p *value and \p *len set OK, or nonzero if, eg, metadata \p name does
 * not exist on the streamtype.
 *
 * The pointed-to values may only exist until the next time around the event
 * loop.
 */
LWS_VISIBLE LWS_EXTERN int
lws_ss_get_metadata(struct lws_ss_handle *h, const char *name,
		    const void **value, size_t *len);

/**
 * lws_ss_server_ack() - indicate how we feel about what the server has sent
 *
 * \param h: ss handle of accepted connection
 * \param nack: 0 means we are OK with it, else some problem
 *
 * For SERVER secure streams
 *
 * Depending on the protocol, the server sending us something may be
 * transactional, ie, built into it sending something is the idea we will
 * respond somehow out-of-band; HTTP is like this with, eg, 200 response code.
 *
 * Calling this with nack=0 indicates that when we later respond, we want to
 * acknowledge the transaction (eg, it means a 200 if http underneath), if
 * nonzero that the transaction should act like it failed.
 *
 * If the underlying protocol doesn't understand transactions (eg, ws) then this
 * has no effect either way.
 */
LWS_VISIBLE LWS_EXTERN void
lws_ss_server_ack(struct lws_ss_handle *h, int nack);

typedef void (*lws_sssfec_cb)(struct lws_ss_handle *h, void *arg);

/**
 * lws_ss_server_foreach_client() - callback for each live client connected to server
 *
 * \param h: server ss handle
 * \param cb: the callback
 * \param arg: arg passed to callback
 *
 * For SERVER secure streams
 *
 * Call the callback \p cb once for each client ss connected to the server,
 * passing \p arg as an additional callback argument each time.
 */
LWS_VISIBLE LWS_EXTERN void
lws_ss_server_foreach_client(struct lws_ss_handle *h, lws_sssfec_cb cb,
			     void *arg);

/**
 * lws_ss_change_handlers() - helper for dynamically changing stream handlers
 *
 * \param h: ss handle
 * \param rx: the new RX handler
 * \param tx: the new TX handler
 * \param state: the new state handler
 *
 * Handlers set to NULL are left unchanged.
 *
 * This works on any handle, client or server and takes effect immediately.
 *
 * Depending on circumstances this may be helpful when
 *
 * a) a server stream undergoes an LWSSSCS_SERVER_UPGRADE (as in http -> ws) and
 * the payloads in the new protocol have a different purpose that is best
 * handled in their own rx and tx callbacks, and
 *
 * b) you may want to serve several different, possibly large things based on
 * what was requested.  Setting a customized handler allows clean encapsulation
 * of the different serving strategies.
 *
 * If the stream is long-lived, like ws, you should set the changed handler back
 * to the default when the transaction wanting it is completed.
 */
LWS_VISIBLE LWS_EXTERN void
lws_ss_change_handlers(struct lws_ss_handle *h, lws_sscb_rx rx, lws_sscb_tx tx,
		       lws_sscb_state state);

/**
 * lws_ss_add_peer_tx_credit() - allow peer to transmit more to us
 *
 * \param h: secure streams handle
 * \param add: additional tx credit (signed)
 *
 * Indicate to remote peer that we can accept \p add bytes more payload being
 * sent to us.
 */
LWS_VISIBLE LWS_EXTERN int
lws_ss_add_peer_tx_credit(struct lws_ss_handle *h, int32_t add);

/**
 * lws_ss_get_est_peer_tx_credit() - get our current estimate of peer's tx credit
 *
 * \param h: secure streams handle
 *
 * Based on what credit we gave it, and what we have received, report our
 * estimate of peer's tx credit usable to transmit to us.  This may be outdated
 * in that some or all of its credit may already have been expended by sending
 * stuff to us that is in flight already.
 */
LWS_VISIBLE LWS_EXTERN int
lws_ss_get_est_peer_tx_credit(struct lws_ss_handle *h);

LWS_VISIBLE LWS_EXTERN const char *
lws_ss_tag(struct lws_ss_handle *h);


#if defined(LWS_WITH_SECURE_STREAMS_AUTH_SIGV4)
/**
 * lws_ss_sigv4_set_aws_key() - set aws credential into system blob
 *
 * \param context: lws_context
 * \param idx:     the system blob index specified in the policy, currently
 *                  up to 4 blobs.
 * \param keyid:   aws access keyid
 * \param key:     aws access key
 *
 * Return 0 if OK or nonzero if e.g. idx is invalid; system blob heap appending
 * fails.
 */

LWS_VISIBLE LWS_EXTERN int
lws_ss_sigv4_set_aws_key(struct lws_context* context, uint8_t idx,
		                const char * keyid, const char * key);

/**
 * lws_aws_filesystem_credentials_helper() - read aws credentials from file
 *
 * \param path: path to read, ~ at start is converted to $HOME contents if any
 * \param kid: eg, "aws_access_key_id"
 * \param ak: eg, "aws_secret_access_key"
 * \param aws_keyid: pointer to pointer for allocated keyid from credentials file
 * \param aws_key: pointer to pointer for allocated key from credentials file
 *
 * Return 0 if both *aws_keyid and *aws_key allocated from the config file, else
 * nonzero, and neither *aws_keyid or *aws_key are allocated.
 *
 * If *aws_keyid and *aws_key are set, it's the user's responsibility to
 * free() them when they are no longer needed.
 */

LWS_VISIBLE LWS_EXTERN int
lws_aws_filesystem_credentials_helper(const char *path, const char *kid,
				      const char *ak, char **aws_keyid,
				      char **aws_key);

#endif

///@}

