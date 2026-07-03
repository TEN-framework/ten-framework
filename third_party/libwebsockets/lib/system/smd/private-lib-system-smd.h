/*
 * lws System Message Distribution
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
 */


#if defined(LWS_WITH_SECURE_STREAMS)
#define LWS_SMD_SS_RX_HEADER_LEN_EFF	(LWS_SMD_SS_RX_HEADER_LEN)
#else
#define LWS_SMD_SS_RX_HEADER_LEN_EFF	(0)
#endif

struct lws_smd_peer;

typedef struct lws_smd_msg {
	lws_dll2_t			list;

	struct lws_smd_peer		*exc;

	lws_usec_t			timestamp;
	lws_smd_class_t			_class;

	uint16_t			length;
	uint16_t			refcount;

	/* message itself is over-allocated after this */
} lws_smd_msg_t;

typedef struct lws_smd_peer {
	lws_dll2_t			list;

#if defined(LWS_WITH_SECURE_STREAMS)
	lws_ss_handle_t			*ss_handle; /* LSMDT_SECURE_STREAMS */
#endif

	lws_smd_notification_cb_t	cb;   /* LSMDT_<other> */
	struct lws_context		*ctx;
	void				*opaque;

	/* NULL, or next message we will handle */
	lws_smd_msg_t			*tail;

	lws_smd_class_t			_class_filter;
} lws_smd_peer_t;

/*
 * Manages message distribution
 *
 * There is one of these in the lws_context, but the distribution action also
 * gets involved in delivering to pt event loops individually for SMP case
 */

typedef struct lws_smd {
	lws_dll2_owner_t		owner_messages; /* lws_smd_msg_t */
	lws_mutex_t			lock_messages;
	lws_dll2_owner_t		owner_peers;	/* lws_smd_peer_t */
	lws_mutex_t			lock_peers;

	/* union of peer class filters, suppress creation of msg classes not set */
	lws_smd_class_t			_class_filter;

	char				delivering;
} lws_smd_t;

/* check if this tsi has pending messages to deliver */

int
lws_smd_message_pending(struct lws_context *ctx);

int
lws_smd_msg_distribute(struct lws_context *ctx);

int
_lws_smd_destroy(struct lws_context *ctx);

