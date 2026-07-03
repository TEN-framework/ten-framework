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

#if defined(LWS_WITH_UDP)

typedef enum dns_query_type {
	LWS_ADNS_RECORD_A					= 0x01,
	LWS_ADNS_RECORD_CNAME					= 0x05,
	LWS_ADNS_RECORD_MX					= 0x0f,
	LWS_ADNS_RECORD_AAAA					= 0x1c,
} adns_query_type_t;

typedef enum {
	LADNS_RET_FAILED_WSI_CLOSED				= -4,
	LADNS_RET_NXDOMAIN					= -3,
	LADNS_RET_TIMEDOUT					= -2,
	LADNS_RET_FAILED					= -1,
	LADNS_RET_FOUND,
	LADNS_RET_CONTINUING
} lws_async_dns_retcode_t;

struct addrinfo;

typedef struct lws * (*lws_async_dns_cb_t)(struct lws *wsi, const char *ads,
		const struct addrinfo *result, int n, void *opaque);

/**
 * lws_async_dns_query() - perform a dns lookup using async dns
 *
 * \param context: the lws_context
 * \param tsi: thread service index (usually 0)
 * \param name: DNS name to look up
 * \param qtype: type of query (A, AAAA etc)
 * \param cb: query completion callback
 * \param wsi: wsi if the query is related to one
 *
 * Starts an asynchronous DNS lookup, on completion the \p cb callback will
 * be called.
 *
 * The reference count on the cached object is incremented for every callback
 * that was called with the cached addrinfo results.
 *
 * The cached object can't be evicted until the reference count reaches zero...
 * use lws_async_dns_freeaddrinfo() to indicate you're finsihed with the
 * results for each callback that happened with them.
 */
LWS_VISIBLE LWS_EXTERN lws_async_dns_retcode_t
lws_async_dns_query(struct lws_context *context, int tsi, const char *name,
		    adns_query_type_t qtype, lws_async_dns_cb_t cb,
		    struct lws *wsi, void *opaque);

/**
 * lws_async_dns_freeaddrinfo() - decrement refcount on cached addrinfo results
 *
 * \param pai: a pointert to a pointer to first addrinfo returned as result in the callback
 *
 * Decrements the cache object's reference count.  When it reaches zero, the
 * cached object may be reaped subject to LRU rules.
 *
 * The pointer to the first addrinfo give in the argument is set to NULL.
 */
LWS_VISIBLE LWS_EXTERN void
lws_async_dns_freeaddrinfo(const struct addrinfo **ai);

#endif
