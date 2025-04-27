/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2019 Andy Green <andy@warmcat.com>
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

/*! \defgroup sending-data Sending data

    APIs related to writing data on a connection
*/
//@{
#if !defined(LWS_SIZEOFPTR)
#define LWS_SIZEOFPTR ((int)sizeof (void *))
#endif

#if defined(__x86_64__)
#define _LWS_PAD_SIZE 16	/* Intel recommended for best performance */
#else
#define _LWS_PAD_SIZE LWS_SIZEOFPTR   /* Size of a pointer on the target arch */
#endif
#define _LWS_PAD(n) (((n) % _LWS_PAD_SIZE) ? \
		((n) + (_LWS_PAD_SIZE - ((n) % _LWS_PAD_SIZE))) : (n))
/* last 2 is for lws-meta */
#define LWS_PRE _LWS_PAD(4 + 10 + 2)
/* used prior to 1.7 and retained for backward compatibility */
#define LWS_SEND_BUFFER_PRE_PADDING LWS_PRE
#define LWS_SEND_BUFFER_POST_PADDING 0

#define LWS_WRITE_RAW LWS_WRITE_HTTP

/*
 * NOTE: These public enums are part of the abi.  If you want to add one,
 * add it at where specified so existing users are unaffected.
 */
enum lws_write_protocol {
	LWS_WRITE_TEXT						= 0,
	/**< Send a ws TEXT message,the pointer must have LWS_PRE valid
	 * memory behind it.
	 *
	 * The receiver expects only valid utf-8 in the payload */
	LWS_WRITE_BINARY					= 1,
	/**< Send a ws BINARY message, the pointer must have LWS_PRE valid
	 * memory behind it.
	 *
	 * Any sequence of bytes is valid */
	LWS_WRITE_CONTINUATION					= 2,
	/**< Continue a previous ws message, the pointer must have LWS_PRE valid
	 * memory behind it */
	LWS_WRITE_HTTP						= 3,
	/**< Send HTTP content */

	/* LWS_WRITE_CLOSE is handled by lws_close_reason() */
	LWS_WRITE_PING						= 5,
	LWS_WRITE_PONG						= 6,

	/* Same as write_http but we know this write ends the transaction */
	LWS_WRITE_HTTP_FINAL					= 7,

	/* HTTP2 */

	LWS_WRITE_HTTP_HEADERS					= 8,
	/**< Send http headers (http2 encodes this payload and LWS_WRITE_HTTP
	 * payload differently, http 1.x links also handle this correctly. so
	 * to be compatible with both in the future,header response part should
	 * be sent using this regardless of http version expected)
	 */
	LWS_WRITE_HTTP_HEADERS_CONTINUATION			= 9,
	/**< Continuation of http/2 headers
	 */

	/****** add new things just above ---^ ******/

	/* flags */

	LWS_WRITE_BUFLIST = 0x20,
	/**< Don't actually write it... stick it on the output buflist and
	 *   write it as soon as possible.  Useful if you learn you have to
	 *   write something, have the data to write to hand but the timing is
	 *   unrelated as to whether the connection is writable or not, and were
	 *   otherwise going to have to allocate a temp buffer and write it
	 *   later anyway */

	LWS_WRITE_NO_FIN = 0x40,
	/**< This part of the message is not the end of the message */

	LWS_WRITE_H2_STREAM_END = 0x80,
	/**< Flag indicates this packet should go out with STREAM_END if h2
	 * STREAM_END is allowed on DATA or HEADERS.
	 */

	LWS_WRITE_CLIENT_IGNORE_XOR_MASK = 0x80
	/**< client packet payload goes out on wire unmunged
	 * only useful for security tests since normal servers cannot
	 * decode the content if used */
};

/* used with LWS_CALLBACK_CHILD_WRITE_VIA_PARENT */

struct lws_write_passthru {
	struct lws *wsi;
	unsigned char *buf;
	size_t len;
	enum lws_write_protocol wp;
};


/**
 * lws_write() - Apply protocol then write data to client
 *
 * \param wsi:	Websocket instance (available from user callback)
 * \param buf:	The data to send.  For data being sent on a websocket
 *		connection (ie, not default http), this buffer MUST have
 *		LWS_PRE bytes valid BEFORE the pointer.
 *		This is so the protocol header data can be added in-situ.
 * \param len:	Count of the data bytes in the payload starting from buf
 * \param protocol:	Use LWS_WRITE_HTTP to reply to an http connection, and one
 *		of LWS_WRITE_BINARY or LWS_WRITE_TEXT to send appropriate
 *		data on a websockets connection.  Remember to allow the extra
 *		bytes before and after buf if LWS_WRITE_BINARY or LWS_WRITE_TEXT
 *		are used.
 *
 * This function provides the way to issue data back to the client, for any
 * role (h1, h2, ws, raw, etc).  It can only be called from the WRITEABLE
 * callback.
 *
 * IMPORTANT NOTICE!
 *
 * When sending with ws protocol
 *
 * LWS_WRITE_TEXT,
 * LWS_WRITE_BINARY,
 * LWS_WRITE_CONTINUATION,
 * LWS_WRITE_PING,
 * LWS_WRITE_PONG,
 *
 * or sending on http/2... the send buffer has to have LWS_PRE bytes valid
 * BEFORE the buffer pointer you pass to lws_write().  Since you'll probably
 * want to use http/2 before too long, it's wise to just always do this with
 * lws_write buffers... LWS_PRE is typically 16 bytes it's not going to hurt
 * usually.
 *
 * start of alloc       ptr passed to lws_write      end of allocation
 *       |                         |                         |
 *       v  <-- LWS_PRE bytes -->  v                         v
 *       [----------------  allocated memory  ---------------]
 *              (for lws use)      [====== user buffer ======]
 *
 * This allows us to add protocol info before the data, and send as one packet
 * on the network without payload copying, for maximum efficiency.
 *
 * So for example you need this kind of code to use lws_write with a
 * 128-byte payload
 *
 *   char buf[LWS_PRE + 128];
 *
 *   // fill your part of the buffer... for example here it's all zeros
 *   memset(&buf[LWS_PRE], 0, 128);
 *
 *   if (lws_write(wsi, &buf[LWS_PRE], 128, LWS_WRITE_TEXT) < 128) {
 *   		... the connection is dead ...
 *   		return -1;
 *   }
 *
 * LWS_PRE is currently 16, which covers ws and h2 frame headers, and is
 * compatible with 32 and 64-bit alignment requirements.
 *
 * (LWS_SEND_BUFFER_POST_PADDING is deprecated, it's now 0 and can be left off.)
 *
 * Return may be -1 is the write failed in a way indicating that the connection
 * has ended already, in which case you can close your side, or a positive
 * number that is at least the number of bytes requested to send (under some
 * encapsulation scenarios, it can indicate more than you asked was sent).
 *
 * The recommended test of the return is less than what you asked indicates
 * the connection has failed.
 *
 * Truncated Writes
 * ================
 *
 * The OS may not accept everything you asked to write on the connection.
 *
 * Posix defines POLLOUT indication from poll() to show that the connection
 * will accept more write data, but it doesn't specifiy how much.  It may just
 * accept one byte of whatever you wanted to send.
 *
 * LWS will buffer the remainder automatically, and send it out autonomously.
 *
 * During that time, WRITABLE callbacks to user code will be suppressed and
 * instead used internally.  After it completes, it will send an extra WRITEABLE
 * callback to the user code, in case any request was missed.  So it is possible
 * to receive unasked-for WRITEABLE callbacks, the user code should have enough
 * state to know if it wants to write anything and just return if not.
 *
 * This is to handle corner cases where unexpectedly the OS refuses what we
 * usually expect it to accept.  It's not recommended as the way to randomly
 * send huge payloads, since it is being copied on to heap and is inefficient.
 *
 * Huge payloads should instead be sent in fragments that are around 2 x mtu,
 * which is almost always directly accepted by the OS.  To simplify this for
 * ws fragments, there is a helper lws_write_ws_flags() below that simplifies
 * selecting the correct flags to give lws_write() for each fragment.
 *
 * In the case of RFC8441 ws-over-h2, you cannot send ws fragments larger than
 * the max h2 frame size, typically 16KB, but should further restrict it to
 * the same ~2 x mtu limit mentioned above.
 */
LWS_VISIBLE LWS_EXTERN int
lws_write(struct lws *wsi, unsigned char *buf, size_t len,
	  enum lws_write_protocol protocol);

/* helper for case where buffer may be const */
#define lws_write_http(wsi, buf, len) \
	lws_write(wsi, (unsigned char *)(buf), len, LWS_WRITE_HTTP)

/**
 * lws_write_ws_flags() - Helper for multi-frame ws message flags
 *
 * \param initial: the lws_write flag to use for the start fragment, eg,
 *		   LWS_WRITE_TEXT
 * \param is_start: nonzero if this is the first fragment of the message
 * \param is_end: nonzero if this is the last fragment of the message
 *
 * Returns the correct LWS_WRITE_ flag to use for each fragment of a message
 * in turn.
 */
static LWS_INLINE int
lws_write_ws_flags(int initial, int is_start, int is_end)
{
	int r;

	if (is_start)
		r = initial;
	else
		r = LWS_WRITE_CONTINUATION;

	if (!is_end)
		r |= LWS_WRITE_NO_FIN;

	return r;
}

/**
 * lws_raw_transaction_completed() - Helper for flushing before close
 *
 * \param wsi: the struct lws to operate on
 *
 * Returns -1 if the wsi can close now.  However if there is buffered, unsent
 * data, the wsi is marked as to be closed when the output buffer data is
 * drained, and it returns 0.
 *
 * For raw cases where the transaction completed without failure,
 * `return lws_raw_transaction_completed(wsi)` should better be used than
 * return -1.
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_raw_transaction_completed(struct lws *wsi);

///@}
