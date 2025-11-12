/*
 * lws System Message Distribution
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

#define LWS_SMD_MAX_PAYLOAD		384
#define LWS_SMD_CLASS_BITFIELD_BYTES	4

#define LWS_SMD_STREAMTYPENAME		"_lws_smd"
#define LWS_SMD_SS_RX_HEADER_LEN	16

typedef uint32_t lws_smd_class_t;

struct lws_smd_msg; /* opaque */
struct lws_smd_peer; /* opaque */

/*
 * Well-known device classes
 */

enum {
	LWSSMDCL_INTERACTION					= (1 << 0),
	/**<
	 * Any kind of event indicating a user was interacting with the device,
	 * eg, press a button, touched the screen, lifted the device etc
	 */
	LWSSMDCL_SYSTEM_STATE					= (1 << 1),
	/**<
	 * The lws_system state changed, eg, to OPERATIONAL
	 */
	LWSSMDCL_NETWORK					= (1 << 2),
	/**<
	 * Something happened on the network, eg, link-up or DHCP, or captive
	 * portal state update
	 */
	LWSSMDCL_METRICS					= (1 << 3),
	/**<
	 * An SS client process is reporting a metric to the proxy (this class
	 * is special in that it is not rebroadcast by the proxy)
	 */

	LWSSMDCL_USER_BASE_BITNUM				= 24
};

/**
 * lws_smd_msg_alloc() - allocate a message of length len
 *
 * \param ctx: the lws_context
 * \param _class: the smd message class, recipients filter on this
 * \param len: the required payload length
 *
 * This helper returns an opaque lws_smd_msg pointer and sets *buf to a buffer
 * associated with it of length \p len.
 *
 * In this way the lws_msg_smd type remains completely opaque and the allocated
 * area can be prepared by the caller directly, without copying.
 *
 * On failure, it returns NULL... it may fail for OOM but it may also fail if
 * you request to allocate for a message class that the system has no
 * participant who is listening for that class of event currently... the event
 * generation action at the caller should be bypassed without error then.
 *
 * This is useful if you have a message you know the length of.  For text-based
 * messages like JSON, lws_smd_msg_printf() is more convenient.
 */
LWS_VISIBLE LWS_EXTERN void * /* payload */
lws_smd_msg_alloc(struct lws_context *ctx, lws_smd_class_t _class, size_t len);

/**
 * lws_smd_msg_free() - abandon a previously allocated message before sending
 *
 * \param payload: pointer the previously-allocated message payload
 *
 * Destroys a previously-allocated opaque message object and the requested
 * buffer space, in the case that between allocating it and sending it, some
 * condition was met that means it can no longer be sent, eg, an error
 * generating the content.  Otherwise there is no need to destroy allocated
 * message objects with this, lws will take care of it.
 */
LWS_VISIBLE LWS_EXTERN void
lws_smd_msg_free(void **payload);

/**
 * lws_smd_msg_send() - queue a previously allocated message
 *
 * \param ctx: the lws_context
 * \param msg: the prepared message
 *
 * Queues an allocated, prepared message for delivery to smd clients
 *
 * This is threadsafe to call from a non-service thread.
 */
LWS_VISIBLE LWS_EXTERN int
lws_smd_msg_send(struct lws_context *ctx, void *payload);

/**
 * lws_smd_msg_printf() - queue a previously allocated message
 *
 * \param ctx: the lws_context
 * \param _class: the message class
 * \param format: the format string to prepare the payload with
 * \param ...: arguments for the format string, if any
 *
 * For string-based messages, eg, JSON, allows formatted creating of the payload
 * size discovery, allocation and message send all in one step.
 *
 * Unlike lws_smd_msg_alloc() you do not need to know the length beforehand as
 * this computes it and calls lws_smd_msg_alloc() with the correct length.
 *
 * To be clear this also calls through to lws_smd_msg_send(), it really does
 * everything in one step.  If there are no registered participants that want
 * messages of \p _class, this function returns immediately without doing any
 * allocation or anything else.
 *
 * This is threadsafe to call from a non-service thread.
 */
LWS_VISIBLE LWS_EXTERN int
lws_smd_msg_printf(struct lws_context *ctx, lws_smd_class_t _class,
		   const char *format, ...) LWS_FORMAT(3);

/**
 * lws_smd_ss_msg_printf() - helper to prepare smd ss message tx
 *
 * \param h: the ss handle
 * \param buf: the ss tx buffer
 * \param len: on entry, points to the ss tx buffer length, on exit, set to used
 * \param _class: the message class
 * \param format: the format string to prepare the payload with
 * \param ...: arguments for the format string, if any
 *
 * This helper lets you produce SMD messages on an SS link of the builtin
 * streamtype LWS_SMD_STREAMTYPENAME, using the same api format as
 * lws_smd_msg_prinf(), but writing the message into the ss tx buffer from
 * its tx() callback.
 */

struct lws_ss_handle;
LWS_VISIBLE LWS_EXTERN int
lws_smd_ss_msg_printf(const char *tag, uint8_t *buf, size_t *len,
		      lws_smd_class_t _class, const char *format, ...)
		      LWS_FORMAT(5);

/**
 * lws_smd_ss_rx_forward() - helper to forward smd messages that came in by SS
 *
 * \param ss_user: ss user pointer, as delivered to rx callback
 * \param buf: the ss rx buffer
 * \param len: the length of the ss rx buffer
 *
 * Proxied Secure Streams with the streamtype LWS_SMD_STREAMTYPENAME receive
 * serialized SMD messages from the proxy, this helper allows them to be
 * translated into deserialized SMD messages and forwarded to registered SMD
 * participants in the local context in one step.
 *
 * Just pass through what arrived in the LWS_SMD_STREAMTYPENAME rx() callback
 * to this api.
 *
 * Returns 0 if OK else nonzero if unable to queue the SMD message.
 */
LWS_VISIBLE LWS_EXTERN int
lws_smd_ss_rx_forward(void *ss_user, const uint8_t *buf, size_t len);

LWS_VISIBLE LWS_EXTERN int
lws_smd_sspc_rx_forward(void *ss_user, const uint8_t *buf, size_t len);

typedef int (*lws_smd_notification_cb_t)(void *opaque, lws_smd_class_t _class,
					 lws_usec_t timestamp, void *buf,
					 size_t len);

#define LWSSMDREG_FLAG_PROXIED_SS	(1 << 0)
/**< It's actually a proxied SS connection registering, opaque is the ss h */

/*
 * lws_smd_register() - register to receive smd messages
 *
 * \param ctx: the lws_context
 * \param opaque: an opaque pointer handed to the callback
 * \param flags: typically 0
 * \param _class_filter: bitmap of message classes we care about
 * \param cb: the callback to receive messages
 *
 * Queues an allocated, prepared message for delivery to smd clients.
 *
 * Returns NULL on failure, or an opaque handle which may be given to
 * lws_smd_unregister() to stop participating in the shared message queue.
 *
 * This is threadsafe to call from a non-service thread.
 */

LWS_VISIBLE LWS_EXTERN struct lws_smd_peer *
lws_smd_register(struct lws_context *ctx, void *opaque, int flags,
		 lws_smd_class_t _class_filter, lws_smd_notification_cb_t cb);

/*
 * lws_smd_unregister() - unregister receiving smd messages
 *
 * \param pr: the handle returned from the registration
 *
 * Destroys the registration of the callback for messages and ability to send
 * messages.
 *
 * It's not necessary to call this if the registration wants to survive for as
 * long as the lws_context... lws_context_destroy will also clean up any
 * registrations still active by then.
 */

LWS_VISIBLE LWS_EXTERN void
lws_smd_unregister(struct lws_smd_peer *pr);
