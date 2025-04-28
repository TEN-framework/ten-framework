/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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

/*! \defgroup tls_sessions TLS Session Management

    APIs related to managing TLS Sessions
*/
//@{


#define LWS_SESSION_TAG_LEN 96

struct lws_tls_session_dump
{
	char			tag[LWS_SESSION_TAG_LEN];
	void			*blob;
        void			*opaque;
	size_t			blob_len;
};

typedef int (*lws_tls_sess_cb_t)(struct lws_context *cx,
				 struct lws_tls_session_dump *info);

/**
 * lws_tls_session_dump_save() - serialize a tls session via a callback
 *
 * \param vh: the vhost to load into the session cache
 * \param host: the name of the host the session relates to
 * \param port: the port the session connects to on the host
 * \param cb_save: the callback to perform the saving of the session blob
 * \param opq: an opaque pointer passed into the callback
 *
 * If a session matching the vhost/host/port exists in the vhost's session
 * cache, serialize it via the provided callback.
 *
 * \p opq is passed to the callback without being used by lws at all.
 */
LWS_VISIBLE LWS_EXTERN int
lws_tls_session_dump_save(struct lws_vhost *vh, const char *host, uint16_t port,
			  lws_tls_sess_cb_t cb_save, void *opq);

/**
 * lws_tls_session_dump_load() - deserialize a tls session via a callback
 *
 * \param vh: the vhost to load into the session cache
 * \param host: the name of the host the session relates to
 * \param port: the port the session connects to on the host
 * \param cb_load: the callback to retreive the session blob from
 * \param opq: an opaque pointer passed into the callback
 *
 * Try to preload a session described by the first three parameters into the
 * client session cache, from the given callback.
 *
 * \p opq is passed to the callback without being used by lws at all.
 */
LWS_VISIBLE LWS_EXTERN int
lws_tls_session_dump_load(struct lws_vhost *vh, const char *host, uint16_t port,
			  lws_tls_sess_cb_t cb_load, void *opq);

///@}
