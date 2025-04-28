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

/* minimal space for typical headers and CSP stuff */

#define LWS_RECOMMENDED_MIN_HEADER_SPACE 2048

/*! \defgroup http HTTP

    Modules related to handling HTTP
*/
//@{

/*! \defgroup httpft HTTP File transfer
 * \ingroup http

    APIs for sending local files in response to HTTP requests
*/
//@{

/**
 * lws_get_mimetype() - Determine mimetype to use from filename
 *
 * \param file:		filename
 * \param m:		NULL, or mount context
 *
 * This uses a canned list of known filetypes first, if no match and m is
 * non-NULL, then tries a list of per-mount file suffix to mimtype mappings.
 *
 * Returns either NULL or a pointer to the mimetype matching the file.
 */
LWS_VISIBLE LWS_EXTERN const char *
lws_get_mimetype(const char *file, const struct lws_http_mount *m);

/**
 * lws_serve_http_file() - Send a file back to the client using http
 * \param wsi:		Websocket instance (available from user callback)
 * \param file:		The file to issue over http
 * \param content_type:	The http content type, eg, text/html
 * \param other_headers:	NULL or pointer to header string
 * \param other_headers_len:	length of the other headers if non-NULL
 *
 *	This function is intended to be called from the callback in response
 *	to http requests from the client.  It allows the callback to issue
 *	local files down the http link in a single step.
 *
 *	Returning <0 indicates error and the wsi should be closed.  Returning
 *	>0 indicates the file was completely sent and
 *	lws_http_transaction_completed() called on the wsi (and close if != 0)
 *	==0 indicates the file transfer is started and needs more service later,
 *	the wsi should be left alone.
 */
LWS_VISIBLE LWS_EXTERN int
lws_serve_http_file(struct lws *wsi, const char *file, const char *content_type,
		    const char *other_headers, int other_headers_len);

LWS_VISIBLE LWS_EXTERN int
lws_serve_http_file_fragment(struct lws *wsi);
//@}


enum http_status {
	HTTP_STATUS_CONTINUE					= 100,

	HTTP_STATUS_OK						= 200,
	HTTP_STATUS_NO_CONTENT					= 204,
	HTTP_STATUS_PARTIAL_CONTENT				= 206,

	HTTP_STATUS_MOVED_PERMANENTLY				= 301,
	HTTP_STATUS_FOUND					= 302,
	HTTP_STATUS_SEE_OTHER					= 303,
	HTTP_STATUS_NOT_MODIFIED				= 304,

	HTTP_STATUS_BAD_REQUEST					= 400,
	HTTP_STATUS_UNAUTHORIZED,
	HTTP_STATUS_PAYMENT_REQUIRED,
	HTTP_STATUS_FORBIDDEN,
	HTTP_STATUS_NOT_FOUND,
	HTTP_STATUS_METHOD_NOT_ALLOWED,
	HTTP_STATUS_NOT_ACCEPTABLE,
	HTTP_STATUS_PROXY_AUTH_REQUIRED,
	HTTP_STATUS_REQUEST_TIMEOUT,
	HTTP_STATUS_CONFLICT,
	HTTP_STATUS_GONE,
	HTTP_STATUS_LENGTH_REQUIRED,
	HTTP_STATUS_PRECONDITION_FAILED,
	HTTP_STATUS_REQ_ENTITY_TOO_LARGE,
	HTTP_STATUS_REQ_URI_TOO_LONG,
	HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE,
	HTTP_STATUS_REQ_RANGE_NOT_SATISFIABLE,
	HTTP_STATUS_EXPECTATION_FAILED,

	HTTP_STATUS_INTERNAL_SERVER_ERROR			= 500,
	HTTP_STATUS_NOT_IMPLEMENTED,
	HTTP_STATUS_BAD_GATEWAY,
	HTTP_STATUS_SERVICE_UNAVAILABLE,
	HTTP_STATUS_GATEWAY_TIMEOUT,
	HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED,
};
/*! \defgroup html-chunked-substitution HTML Chunked Substitution
 * \ingroup http
 *
 * ##HTML chunked Substitution
 *
 * APIs for receiving chunks of text, replacing a set of variable names via
 * a callback, and then prepending and appending HTML chunked encoding
 * headers.
 */
//@{

struct lws_process_html_args {
	char *p; /**< pointer to the buffer containing the data */
	int len; /**< length of the original data at p */
	int max_len; /**< maximum length we can grow the data to */
	int final; /**< set if this is the last chunk of the file */
	int chunked; /**< 0 == unchunked, 1 == produce chunk headers
			(incompatible with HTTP/2) */
};

typedef const char *(*lws_process_html_state_cb)(void *data, int index);

struct lws_process_html_state {
	char *start; /**< pointer to start of match */
	char swallow[16]; /**< matched character buffer */
	int pos; /**< position in match */
	void *data; /**< opaque pointer */
	const char * const *vars; /**< list of variable names */
	int count_vars; /**< count of variable names */

	lws_process_html_state_cb replace;
		/**< called on match to perform substitution */
};

/*! lws_chunked_html_process() - generic chunked substitution
 * \param args: buffer to process using chunked encoding
 * \param s: current processing state
 */
LWS_VISIBLE LWS_EXTERN int
lws_chunked_html_process(struct lws_process_html_args *args,
			 struct lws_process_html_state *s);
//@}

/** \defgroup HTTP-headers-read HTTP headers: read
 * \ingroup http
 *
 * ##HTTP header releated functions
 *
 *  In lws the client http headers are temporarily stored in a pool, only for the
 *  duration of the http part of the handshake.  It's because in most cases,
 *  the header content is ignored for the whole rest of the connection lifetime
 *  and would then just be taking up space needlessly.
 *
 *  During LWS_CALLBACK_HTTP when the URI path is delivered is the last time
 *  the http headers are still allocated, you can use these apis then to
 *  look at and copy out interesting header content (cookies, etc)
 *
 *  Notice that the header total length reported does not include a terminating
 *  '\0', however you must allocate for it when using the _copy apis.  So the
 *  length reported for a header containing "123" is 3, but you must provide
 *  a buffer of length 4 so that "123\0" may be copied into it, or the copy
 *  will fail with a nonzero return code.
 *
 *  In the special case of URL arguments, like ?x=1&y=2, the arguments are
 *  stored in a token named for the method, eg,  WSI_TOKEN_GET_URI if it
 *  was a GET or WSI_TOKEN_POST_URI if POST.  You can check the total
 *  length to confirm the method.
 *
 *  For URL arguments, each argument is stored urldecoded in a "fragment", so
 *  you can use the fragment-aware api lws_hdr_copy_fragment() to access each
 *  argument in turn: the fragments contain urldecoded strings like x=1 or y=2.
 *
 *  As a convenience, lws has an api that will find the fragment with a
 *  given name= part, lws_get_urlarg_by_name().
 */
///@{

/** struct lws_tokens
 * you need these to look at headers that have been parsed if using the
 * LWS_CALLBACK_FILTER_CONNECTION callback.  If a header from the enum
 * list below is absent, .token = NULL and len = 0.  Otherwise .token
 * points to .len chars containing that header content.
 */
struct lws_tokens {
	unsigned char *token; /**< pointer to start of the token */
	int len; /**< length of the token's value */
};

/* enum lws_token_indexes
 * these have to be kept in sync with lextable.h / minilex.c
 *
 * NOTE: These public enums are part of the abi.  If you want to add one,
 * add it at where specified so existing users are unaffected.
 */
enum lws_token_indexes {
	WSI_TOKEN_GET_URI, /* 0 */
	WSI_TOKEN_POST_URI,
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_OPTIONS_URI,
#endif
	WSI_TOKEN_HOST,
	WSI_TOKEN_CONNECTION,
	WSI_TOKEN_UPGRADE, /* 5 */
	WSI_TOKEN_ORIGIN,
#if defined(LWS_ROLE_WS) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_DRAFT,
#endif
	WSI_TOKEN_CHALLENGE,
#if defined(LWS_ROLE_WS) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_EXTENSIONS,
	WSI_TOKEN_KEY1, /* 10 */
	WSI_TOKEN_KEY2,
	WSI_TOKEN_PROTOCOL,
	WSI_TOKEN_ACCEPT,
	WSI_TOKEN_NONCE,
#endif
	WSI_TOKEN_HTTP,
#if defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_HTTP2_SETTINGS, /* 16 */
#endif
	WSI_TOKEN_HTTP_ACCEPT,
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_HTTP_AC_REQUEST_HEADERS,
#endif
	WSI_TOKEN_HTTP_IF_MODIFIED_SINCE,
	WSI_TOKEN_HTTP_IF_NONE_MATCH, /* 20 */
	WSI_TOKEN_HTTP_ACCEPT_ENCODING,
	WSI_TOKEN_HTTP_ACCEPT_LANGUAGE,
	WSI_TOKEN_HTTP_PRAGMA,
	WSI_TOKEN_HTTP_CACHE_CONTROL,
	WSI_TOKEN_HTTP_AUTHORIZATION,
	WSI_TOKEN_HTTP_COOKIE,
	WSI_TOKEN_HTTP_CONTENT_LENGTH, /* 27 */
	WSI_TOKEN_HTTP_CONTENT_TYPE,
	WSI_TOKEN_HTTP_DATE,
	WSI_TOKEN_HTTP_RANGE,
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_HTTP_REFERER,
#endif
#if defined(LWS_ROLE_WS) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_KEY,
	WSI_TOKEN_VERSION,
	WSI_TOKEN_SWORIGIN,
#endif
#if defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_HTTP_COLON_AUTHORITY,
	WSI_TOKEN_HTTP_COLON_METHOD,
	WSI_TOKEN_HTTP_COLON_PATH,
	WSI_TOKEN_HTTP_COLON_SCHEME,
	WSI_TOKEN_HTTP_COLON_STATUS,
#endif

#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_HTTP_ACCEPT_CHARSET,
#endif
	WSI_TOKEN_HTTP_ACCEPT_RANGES,
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_HTTP_ACCESS_CONTROL_ALLOW_ORIGIN,
#endif
	WSI_TOKEN_HTTP_AGE,
	WSI_TOKEN_HTTP_ALLOW,
	WSI_TOKEN_HTTP_CONTENT_DISPOSITION,
	WSI_TOKEN_HTTP_CONTENT_ENCODING,
	WSI_TOKEN_HTTP_CONTENT_LANGUAGE,
	WSI_TOKEN_HTTP_CONTENT_LOCATION,
	WSI_TOKEN_HTTP_CONTENT_RANGE,
	WSI_TOKEN_HTTP_ETAG,
	WSI_TOKEN_HTTP_EXPECT,
	WSI_TOKEN_HTTP_EXPIRES,
	WSI_TOKEN_HTTP_FROM,
	WSI_TOKEN_HTTP_IF_MATCH,
	WSI_TOKEN_HTTP_IF_RANGE,
	WSI_TOKEN_HTTP_IF_UNMODIFIED_SINCE,
	WSI_TOKEN_HTTP_LAST_MODIFIED,
	WSI_TOKEN_HTTP_LINK,
	WSI_TOKEN_HTTP_LOCATION,
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_HTTP_MAX_FORWARDS,
	WSI_TOKEN_HTTP_PROXY_AUTHENTICATE,
	WSI_TOKEN_HTTP_PROXY_AUTHORIZATION,
#endif
	WSI_TOKEN_HTTP_REFRESH,
	WSI_TOKEN_HTTP_RETRY_AFTER,
	WSI_TOKEN_HTTP_SERVER,
	WSI_TOKEN_HTTP_SET_COOKIE,
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_HTTP_STRICT_TRANSPORT_SECURITY,
#endif
	WSI_TOKEN_HTTP_TRANSFER_ENCODING,
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_HTTP_USER_AGENT,
	WSI_TOKEN_HTTP_VARY,
	WSI_TOKEN_HTTP_VIA,
	WSI_TOKEN_HTTP_WWW_AUTHENTICATE,
#endif
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_PATCH_URI,
	WSI_TOKEN_PUT_URI,
	WSI_TOKEN_DELETE_URI,
#endif

	WSI_TOKEN_HTTP_URI_ARGS,
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_PROXY,
	WSI_TOKEN_HTTP_X_REAL_IP,
#endif
	WSI_TOKEN_HTTP1_0,
	WSI_TOKEN_X_FORWARDED_FOR,
	WSI_TOKEN_CONNECT,
	WSI_TOKEN_HEAD_URI,
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_TE,
	WSI_TOKEN_REPLAY_NONCE, /* ACME */
#endif
#if defined(LWS_ROLE_H2) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_COLON_PROTOCOL,
#endif
	WSI_TOKEN_X_AUTH_TOKEN,
	WSI_TOKEN_DSS_SIGNATURE,

	/****** add new things just above ---^ ******/

	/* use token storage to stash these internally, not for
	 * user use */

	_WSI_TOKEN_CLIENT_SENT_PROTOCOLS,
	_WSI_TOKEN_CLIENT_PEER_ADDRESS,
	_WSI_TOKEN_CLIENT_URI,
	_WSI_TOKEN_CLIENT_HOST,
	_WSI_TOKEN_CLIENT_ORIGIN,
	_WSI_TOKEN_CLIENT_METHOD,
	_WSI_TOKEN_CLIENT_IFACE,
	_WSI_TOKEN_CLIENT_ALPN,

	/* always last real token index*/
	WSI_TOKEN_COUNT,

	/* parser state additions, no storage associated */
	WSI_TOKEN_NAME_PART,
#if defined(LWS_WITH_CUSTOM_HEADERS) || defined(LWS_HTTP_HEADERS_ALL)
	WSI_TOKEN_UNKNOWN_VALUE_PART,
#endif
	WSI_TOKEN_SKIPPING,
	WSI_TOKEN_SKIPPING_SAW_CR,
	WSI_PARSING_COMPLETE,
	WSI_INIT_TOKEN_MUXURL,
};

struct lws_token_limits {
	unsigned short token_limit[WSI_TOKEN_COUNT]; /**< max chars for this token */
};

enum lws_h2_settings {
	H2SET_HEADER_TABLE_SIZE = 1,
	H2SET_ENABLE_PUSH,
	H2SET_MAX_CONCURRENT_STREAMS,
	H2SET_INITIAL_WINDOW_SIZE,
	H2SET_MAX_FRAME_SIZE,
	H2SET_MAX_HEADER_LIST_SIZE,
	H2SET_RESERVED7,
	H2SET_ENABLE_CONNECT_PROTOCOL, /* defined in mcmanus-httpbis-h2-ws-02 */

	H2SET_COUNT /* always last */
};

/**
 * lws_token_to_string() - returns a textual representation of a hdr token index
 *
 * \param token: token index
 */
LWS_VISIBLE LWS_EXTERN const unsigned char *
lws_token_to_string(enum lws_token_indexes token);

/**
 * lws_hdr_total_length: report length of all fragments of a header totalled up
 *		The returned length does not include the space for a
 *		terminating '\0'
 *
 * \param wsi: websocket connection
 * \param h: which header index we are interested in
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_hdr_total_length(struct lws *wsi, enum lws_token_indexes h);

/**
 * lws_hdr_fragment_length: report length of a single fragment of a header
 *		The returned length does not include the space for a
 *		terminating '\0'
 *
 * \param wsi: websocket connection
 * \param h: which header index we are interested in
 * \param frag_idx: which fragment of h we want to get the length of
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_hdr_fragment_length(struct lws *wsi, enum lws_token_indexes h,
			int frag_idx);

/**
 * lws_hdr_copy() - copy all fragments of the given header to a buffer
 *		The buffer length len must include space for an additional
 *		terminating '\0', or it will fail returning -1.
 *
 * \param wsi: websocket connection
 * \param dest: destination buffer
 * \param len: length of destination buffer
 * \param h: which header index we are interested in
 *
 * copies the whole, aggregated header, even if it was delivered in
 * several actual headers piece by piece.  Returns -1 or length of the whole
 * header.
 */
LWS_VISIBLE LWS_EXTERN int
lws_hdr_copy(struct lws *wsi, char *dest, int len, enum lws_token_indexes h);

/**
 * lws_hdr_copy_fragment() - copy a single fragment of the given header to a buffer
 *		The buffer length len must include space for an additional
 *		terminating '\0', or it will fail returning -1.
 *		If the requested fragment index is not present, it fails
 *		returning -1.
 *
 * \param wsi: websocket connection
 * \param dest: destination buffer
 * \param len: length of destination buffer
 * \param h: which header index we are interested in
 * \param frag_idx: which fragment of h we want to copy
 *
 * Normally this is only useful
 * to parse URI arguments like ?x=1&y=2, token index WSI_TOKEN_HTTP_URI_ARGS
 * fragment 0 will contain "x=1" and fragment 1 "y=2"
 */
LWS_VISIBLE LWS_EXTERN int
lws_hdr_copy_fragment(struct lws *wsi, char *dest, int len,
		      enum lws_token_indexes h, int frag_idx);

/**
 * lws_hdr_custom_length() - return length of a custom header
 *
 * \param wsi: websocket connection
 * \param name: header string (including terminating :)
 * \param nlen: length of name
 *
 * Lws knows about 100 common http headers, and parses them into indexes when
 * it recognizes them.  When it meets a header that it doesn't know, it stores
 * the name and value directly, and you can look them up using
 * lws_hdr_custom_length() and lws_hdr_custom_copy().
 *
 * This api returns -1, or the length of the value part of the header if it
 * exists.  Lws must be built with LWS_WITH_CUSTOM_HEADERS (on by default) to
 * use this api.
 */
LWS_VISIBLE LWS_EXTERN int
lws_hdr_custom_length(struct lws *wsi, const char *name, int nlen);

/**
 * lws_hdr_custom_copy() - copy value part of a custom header
 *
 * \param wsi: websocket connection
 * \param dst: pointer to buffer to receive the copy
 * \param len: number of bytes available at dst
 * \param name: header string (including terminating :)
 * \param nlen: length of name
 *
 * Lws knows about 100 common http headers, and parses them into indexes when
 * it recognizes them.  When it meets a header that it doesn't know, it stores
 * the name and value directly, and you can look them up using
 * lws_hdr_custom_length() and lws_hdr_custom_copy().
 *
 * This api returns -1, or the length of the string it copied into dst if it
 * was big enough to contain both the string and an extra terminating NUL. Lws
 * must be built with LWS_WITH_CUSTOM_HEADERS (on by default) to use this api.
 */
LWS_VISIBLE LWS_EXTERN int
lws_hdr_custom_copy(struct lws *wsi, char *dst, int len, const char *name,
		    int nlen);

typedef void (*lws_hdr_custom_fe_cb_t)(const char *name, int nlen, void *opaque);
/**
 * lws_hdr_custom_name_foreach() - Iterate the custom header names
 *
 * \param wsi: websocket connection
 * \param cb: callback for each custom header name
 * \param opaque: ignored by lws except to pass to callback
 *
 * Lws knows about 100 common http headers, and parses them into indexes when
 * it recognizes them.  When it meets a header that it doesn't know, it stores
 * the name and value directly, and you can look them up using
 * lws_hdr_custom_length() and lws_hdr_custom_copy().
 * 
 * This api returns -1 on error else 0. Use lws_hdr_custom_copy() to get the
 * values of headers. Lws must be built with LWS_WITH_CUSTOM_HEADERS (on by
 * default) to use this api.
 */
LWS_VISIBLE LWS_EXTERN int
lws_hdr_custom_name_foreach(struct lws *wsi, lws_hdr_custom_fe_cb_t cb, void *opaque);

/**
 * lws_get_urlarg_by_name_safe() - get copy and return length of y for x=y urlargs
 *
 * \param wsi: the connection to check
 * \param name: the arg name, like "token" or "token="
 * \param buf: the buffer to receive the urlarg (including the name= part)
 * \param len: the length of the buffer to receive the urlarg
 *
 * Returns -1 if not present, else the length of y in the urlarg name=y.  If
 * zero or greater, then buf contains a copy of the string y.  Any = after the
 * name match is trimmed off if the name does not end with = itself.
 *
 * This returns the explicit length and so can deal with binary blobs that are
 * percent-encoded.  It also makes sure buf has a NUL just after the valid
 * length so it can work with NUL-based apis if you don't care about truncation.
 *
 * buf may have been written even when -1 is returned indicating no match.
 *
 * Use this in place of lws_get_urlarg_by_name() that does not return an
 * explicit length.
 */
LWS_VISIBLE LWS_EXTERN int
lws_get_urlarg_by_name_safe(struct lws *wsi, const char *name, char *buf, int len);

/**
 * lws_get_urlarg_by_name() - return pointer to arg value if present
 *
 * \param wsi: the connection to check
 * \param name: the arg name, like "token="
 * \param buf: the buffer to receive the urlarg (including the name= part)
 * \param len: the length of the buffer to receive the urlarg
 *
 *     Returns NULL if not found or a pointer inside buf to just after the
 *     name= part.
 *
 * This assumed the argument can be represented with a NUL-terminated string.
 * It can't correctly deal with binary values encoded with %XX, eg. %00 will
 * be understood to terminate the string.
 *
 * Use lws_get_urlarg_by_name_safe() instead of this, which returns the length.
 */
LWS_VISIBLE LWS_EXTERN const char *
lws_get_urlarg_by_name(struct lws *wsi, const char *name, char *buf, int len)
/* LWS_WARN_DEPRECATED */;
///@}

/*! \defgroup HTTP-headers-create HTTP headers: create
 *
 * ## HTTP headers: Create
 *
 * These apis allow you to create HTTP response headers in a way compatible with
 * both HTTP/1.x and HTTP/2.
 *
 * They each append to a buffer taking care about the buffer end, which is
 * passed in as a pointer.  When data is written to the buffer, the current
 * position p is updated accordingly.
 *
 * All of these apis are LWS_WARN_UNUSED_RESULT as they can run out of space
 * and fail with nonzero return.
 */
///@{

#define LWSAHH_CODE_MASK			((1 << 16) - 1)
#define LWSAHH_FLAG_NO_SERVER_NAME		(1 << 30)

/**
 * lws_add_http_header_status() - add the HTTP response status code
 *
 * \param wsi: the connection to check
 * \param code: an HTTP code like 200, 404 etc (see enum http_status)
 * \param p: pointer to current position in buffer pointer
 * \param end: pointer to end of buffer
 *
 * Adds the initial response code, so should be called first.
 *
 * Code may additionally take OR'd flags:
 *
 *    LWSAHH_FLAG_NO_SERVER_NAME:  don't apply server name header this time
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_add_http_header_status(struct lws *wsi,
			   unsigned int code, unsigned char **p,
			   unsigned char *end);
/**
 * lws_add_http_header_by_name() - append named header and value
 *
 * \param wsi: the connection to check
 * \param name: the hdr name, like "my-header:"
 * \param value: the value after the = for this header
 * \param length: the length of the value
 * \param p: pointer to current position in buffer pointer
 * \param end: pointer to end of buffer
 *
 * Appends name: value to the headers
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_add_http_header_by_name(struct lws *wsi, const unsigned char *name,
			    const unsigned char *value, int length,
			    unsigned char **p, unsigned char *end);
/**
 * lws_add_http_header_by_token() - append given header and value
 *
 * \param wsi: the connection to check
 * \param token: the token index for the hdr
 * \param value: the value after the = for this header
 * \param length: the length of the value
 * \param p: pointer to current position in buffer pointer
 * \param end: pointer to end of buffer
 *
 * Appends name=value to the headers, but is able to take advantage of better
 * HTTP/2 coding mechanisms where possible.
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_add_http_header_by_token(struct lws *wsi, enum lws_token_indexes token,
			     const unsigned char *value, int length,
			     unsigned char **p, unsigned char *end);
/**
 * lws_add_http_header_content_length() - append content-length helper
 *
 * \param wsi: the connection to check
 * \param content_length: the content length to use
 * \param p: pointer to current position in buffer pointer
 * \param end: pointer to end of buffer
 *
 * Appends content-length: content_length to the headers
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_add_http_header_content_length(struct lws *wsi,
				   lws_filepos_t content_length,
				   unsigned char **p, unsigned char *end);
/**
 * lws_finalize_http_header() - terminate header block
 *
 * \param wsi: the connection to check
 * \param p: pointer to current position in buffer pointer
 * \param end: pointer to end of buffer
 *
 * Indicates no more headers will be added
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_finalize_http_header(struct lws *wsi, unsigned char **p,
			 unsigned char *end);

/**
 * lws_finalize_write_http_header() - Helper finializing and writing http headers
 *
 * \param wsi: the connection to check
 * \param start: pointer to the start of headers in the buffer, eg &buf[LWS_PRE]
 * \param p: pointer to current position in buffer pointer
 * \param end: pointer to end of buffer
 *
 * Terminates the headers correctly accoring to the protocol in use (h1 / h2)
 * and writes the headers.  Returns nonzero for error.
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_finalize_write_http_header(struct lws *wsi, unsigned char *start,
			       unsigned char **p, unsigned char *end);

#define LWS_ILLEGAL_HTTP_CONTENT_LEN ((lws_filepos_t)-1ll)

/**
 * lws_add_http_common_headers() - Helper preparing common http headers
 *
 * \param wsi: the connection to check
 * \param code: an HTTP code like 200, 404 etc (see enum http_status)
 * \param content_type: the content type, like "text/html"
 * \param content_len: the content length, in bytes
 * \param p: pointer to current position in buffer pointer
 * \param end: pointer to end of buffer
 *
 * Adds the initial response code, so should be called first.
 *
 * Code may additionally take OR'd flags:
 *
 *    LWSAHH_FLAG_NO_SERVER_NAME:  don't apply server name header this time
 *
 * This helper just calls public apis to simplify adding headers that are
 * commonly needed.  If it doesn't fit your case, or you want to add additional
 * headers just call the public apis directly yourself for what you want.
 *
 * You can miss out the content length header by providing the constant
 * LWS_ILLEGAL_HTTP_CONTENT_LEN for the content_len.
 *
 * It does not call lws_finalize_http_header(), to allow you to add further
 * headers after calling this.  You will need to call that yourself at the end.
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_add_http_common_headers(struct lws *wsi, unsigned int code,
			    const char *content_type, lws_filepos_t content_len,
			    unsigned char **p, unsigned char *end);

enum {
	LWSHUMETH_GET,
	LWSHUMETH_POST,
	LWSHUMETH_OPTIONS,
	LWSHUMETH_PUT,
	LWSHUMETH_PATCH,
	LWSHUMETH_DELETE,
	LWSHUMETH_CONNECT,
	LWSHUMETH_HEAD,
	LWSHUMETH_COLON_PATH,
};

/**
 * lws_http_get_uri_and_method() - Get information on method and url
 *
 * \param wsi: the connection to get information on
 * \param puri_ptr: points to pointer to set to url
 * \param puri_len: points to int to set to uri length
 *
 * Returns -1 or method index as one of the LWSHUMETH_ constants
 *
 * If returns method, *puri_ptr is set to the method's URI string and *puri_len
 * to its length
 */

LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_http_get_uri_and_method(struct lws *wsi, char **puri_ptr, int *puri_len);

///@}

/*! \defgroup urlendec Urlencode and Urldecode
 * \ingroup http
 *
 * ##HTML chunked Substitution
 *
 * APIs for receiving chunks of text, replacing a set of variable names via
 * a callback, and then prepending and appending HTML chunked encoding
 * headers.
 */
//@{

/**
 * lws_urlencode() - like strncpy but with urlencoding
 *
 * \param escaped: output buffer
 * \param string: input buffer ('/0' terminated)
 * \param len: output buffer max length
 *
 * Because urlencoding expands the output string, it's not
 * possible to do it in-place, ie, with escaped == string
 */
LWS_VISIBLE LWS_EXTERN const char *
lws_urlencode(char *escaped, const char *string, int len);

/*
 * URLDECODE 1 / 2
 *
 * This simple urldecode only operates until the first '\0' and requires the
 * data to exist all at once
 */
/**
 * lws_urldecode() - like strncpy but with urldecoding
 *
 * \param string: output buffer
 * \param escaped: input buffer ('\0' terminated)
 * \param len: output buffer max length
 *
 * This is only useful for '\0' terminated strings
 *
 * Since urldecoding only shrinks the output string, it is possible to
 * do it in-place, ie, string == escaped
 *
 * Returns 0 if completed OK or nonzero for urldecode violation (non-hex chars
 * where hex required, etc)
 */
LWS_VISIBLE LWS_EXTERN int
lws_urldecode(char *string, const char *escaped, int len);
///@}

/**
 * lws_http_date_render_from_unix() - render unixtime as RFC7231 date string
 *
 * \param buf:		Destination string buffer
 * \param len:		avilable length of dest string buffer in bytes
 * \param t:		pointer to the time_t to render
 *
 * Returns 0 if time_t is rendered into the string buffer successfully, else
 * nonzero.
 */
LWS_VISIBLE LWS_EXTERN int
lws_http_date_render_from_unix(char *buf, size_t len, const time_t *t);

/**
 * lws_http_date_parse_unix() - parse a RFC7231 date string into unixtime
 *
 * \param b:		Source string buffer
 * \param len:		avilable length of source string buffer in bytes
 * \param t:		pointer to the destination time_t to set
 *
 * Returns 0 if string buffer parsed as RFC7231 time successfully, and
 * *t set to the parsed unixtime, else return nonzero.
 */
LWS_VISIBLE LWS_EXTERN int
lws_http_date_parse_unix(const char *b, size_t len, time_t *t);

/**
 * lws_http_check_retry_after() - increase a timeout if retry-after present
 *
 * \param wsi:		http stream this relates to
 * \param us_interval_in_out: default us retry interval on entry may be updated
 *
 * This function may extend the incoming retry interval if the server has
 * requested that using retry-after: header.  It won't reduce the incoming
 * retry interval, only leave it alone or increase it.
 *
 * *us_interval_in_out should be set to a default retry interval on entry, if
 * the wsi has a retry-after time or interval that resolves to an interval
 * longer than the entry *us_interval_in_out, that will be updated to the longer
 * interval and return 0.
 *
 * If no usable retry-after or the time is now or in the past,
 * *us_interval_in_out is left alone and the function returns nonzero.
 */
LWS_VISIBLE LWS_EXTERN int
lws_http_check_retry_after(struct lws *wsi, lws_usec_t *us_interval_in_out);

/**
 * lws_return_http_status() - Return simple http status
 * \param wsi:		Websocket instance (available from user callback)
 * \param code:		Status index, eg, 404
 * \param html_body:		User-readable HTML description < 1KB, or NULL
 *
 *	Helper to report HTTP errors back to the client cleanly and
 *	consistently
 */
LWS_VISIBLE LWS_EXTERN int
lws_return_http_status(struct lws *wsi, unsigned int code,
		       const char *html_body);

/**
 * lws_http_redirect() - write http redirect out on wsi
 *
 * \param wsi:	websocket connection
 * \param code:	HTTP response code (eg, 301)
 * \param loc:	where to redirect to
 * \param len:	length of loc
 * \param p:	pointer current position in buffer (updated as we write)
 * \param end:	pointer to end of buffer
 *
 * Returns amount written, or < 0 indicating fatal write failure.
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_http_redirect(struct lws *wsi, int code, const unsigned char *loc, int len,
		  unsigned char **p, unsigned char *end);

/**
 * lws_http_transaction_completed() - wait for new http transaction or close
 * \param wsi:	websocket connection
 *
 *	Returns nonzero if the HTTP connection must close now
 *	Returns 0 and resets connection to wait for new HTTP header /
 *	  transaction if possible
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_http_transaction_completed(struct lws *wsi);

/**
 * lws_http_headers_detach() - drop the associated headers storage and allow
 *				it to be reused by another connection
 * \param wsi:	http connection
 *
 * If the wsi has an ah headers struct attached, detach it.
 */
LWS_VISIBLE LWS_EXTERN int
lws_http_headers_detach(struct lws *wsi);

/**
 * lws_http_mark_sse() - called to indicate this http stream is now doing SSE
 *
 * \param wsi:	http connection
 *
 * Cancel any timeout on the wsi, and for h2, mark the network connection as
 * containing an immortal stream for the duration the SSE stream is open.
 */
LWS_VISIBLE LWS_EXTERN int
lws_http_mark_sse(struct lws *wsi);

/**
 * lws_h2_client_stream_long_poll_rxonly() - h2 stream to immortal read-only
 *
 * \param wsi: h2 stream client wsi
 *
 * Send END_STREAM-flagged zero-length DATA frame to set client stream wsi into
 * half-closed (local) and remote into half-closed (remote).  Set the client
 * stream wsi to be immortal (not subject to timeouts).
 *
 * Used if the remote server supports immortal long poll to put the stream into
 * a read-only state where it can wait as long as needed for rx.
 *
 * Returns 0 if the process (which happens asynchronously) started or non-zero
 * if it wasn't an h2 stream.
 */
LWS_VISIBLE LWS_EXTERN int
lws_h2_client_stream_long_poll_rxonly(struct lws *wsi);

/**
 * lws_http_compression_apply() - apply an http compression transform
 *
 * \param wsi: the wsi to apply the compression transform to
 * \param name: NULL, or the name of the compression transform, eg, "deflate"
 * \param p: pointer to pointer to headers buffer
 * \param end: pointer to end of headers buffer
 * \param decomp: 0 = add compressor to wsi, 1 = add decompressor
 *
 * This allows transparent compression of dynamically generated HTTP.  The
 * requested compression (eg, "deflate") is only applied if the client headers
 * indicated it was supported (and it has support in lws), otherwise it's a NOP.
 *
 * If the requested compression method is NULL, then the supported compression
 * formats are tried, and for non-decompression (server) mode the first that's
 * found on the client's accept-encoding header is chosen.
 *
 * NOTE: the compression transform, same as h2 support, relies on the user
 * code using LWS_WRITE_HTTP and then LWS_WRITE_HTTP_FINAL on the last part
 * written.  The internal lws fileserving code already does this.
 *
 * If the library was built without the cmake option
 * LWS_WITH_HTTP_STREAM_COMPRESSION set, then a NOP is provided for this api,
 * allowing user code to build either way and use compression if available.
 */
LWS_VISIBLE LWS_EXTERN int
lws_http_compression_apply(struct lws *wsi, const char *name,
			   unsigned char **p, unsigned char *end, char decomp);

/**
 * lws_http_is_redirected_to_get() - true if redirected to GET
 *
 * \param wsi: the wsi to check
 *
 * Check if the wsi is currently in GET mode, after, eg, doing a POST and
 * receiving a 303.
 */
LWS_VISIBLE LWS_EXTERN int
lws_http_is_redirected_to_get(struct lws *wsi);

/**
 * lws_http_cookie_get() - return copy of named cookie if present
 *
 * \param wsi: the wsi to check
 * \param name: name of the cookie
 * \param buf: buffer to store the cookie contents into
 * \param max_len: on entry, maximum length of buf... on exit, used len of buf
 *
 * If no cookie header, or no cookie of the requested name, or the value is
 * larger than can fit in buf, returns nonzero.
 *
 * If the cookie is found, copies its value into buf with a terminating NUL,
 * sets *max_len to the used length, and returns 0.
 *
 * This handles the parsing of the possibly multi-cookie header string and
 * terminating the requested cookie at the next ; if present.
 */
LWS_VISIBLE LWS_EXTERN int
lws_http_cookie_get(struct lws *wsi, const char *name, char *buf, size_t *max);

/**
 * lws_http_client_http_error() - determine if the response code indicates an error
 *
 * \param code: the response code to test
 *
 * Returns nonzero if the code indicates an error, else zero if reflects a
 * non-error condition
 */
#define lws_http_client_http_resp_is_error(code) (!(code < 400))

/**
 * lws_h2_update_peer_txcredit() - manually update stream peer tx credit
 *
 * \param wsi: the h2 child stream whose peer credit to change
 * \param sid: the stream ID, or LWS_H2_STREAM_SID for the wsi stream ID
 * \param bump: signed change to confer upon peer tx credit for sid
 *
 * In conjunction with LCCSCF_H2_MANUAL_RXFLOW flag, allows the user code to
 * selectively starve the remote peer of the ability to send us data on a client
 * connection.
 *
 * Normally lws sends an initial window size for the peer to send to it of 0,
 * but during the header phase it sends a WINDOW_UPDATE to increase the amount
 * available.  LCCSCF_H2_MANUAL_RXFLOW restricts this initial increase in tx
 * credit for the stream, before it has been asked to send us anything, to the
 * amount specified in the client info .manual_initial_tx_credit member, and
 * this api can be called to send the other side permission to send us up to
 * \p bump additional bytes.
 *
 * The nwsi tx credit is updated automatically for exactly what was sent to us
 * on a stream with LCCSCF_H2_MANUAL_RXFLOW flag, but the stream's own tx credit
 * must be handled manually by user code via this api.
 *
 * Returns 0 for success or nonzero for failure.
 */
#define LWS_H2_STREAM_SID -1
LWS_VISIBLE LWS_EXTERN int
lws_h2_update_peer_txcredit(struct lws *wsi, unsigned int sid, int bump);


/**
 * lws_h2_get_peer_txcredit_estimate() - return peer tx credit estimate
 *
 * \param wsi: the h2 child stream whose peer credit estimate to return
 *
 * Returns the estimated amount of tx credit at the peer, in other words the
 * number of bytes the peer is authorized to send to us.
 *
 * It's an 'estimate' because we don't know how much is already in flight
 * towards us and actually already used.
 */
LWS_VISIBLE LWS_EXTERN int
lws_h2_get_peer_txcredit_estimate(struct lws *wsi);

///@}

