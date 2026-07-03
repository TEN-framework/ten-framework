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

/** \defgroup conmon Connection Latency information
 * ## Connection Latency information
 *
 * When LWS_WITH_CONMON is enabled at build, collects detailed statistics
 * about the client connection setup latency, available to the connection
 * itself
 */
///@{

/* enough for 4191s, or just over an hour */
typedef uint32_t lws_conmon_interval_us_t;

/*
 * Connection latency information... note that not all wsi actually make
 * connections, for example h2 streams after the initial one will have 0
 * for everything except ciu_txn_resp.
 *
 * If represented in JSON, it should look like this
 *
 *     {
 *        "peer": "46.105.127.147",
 *        "dns_us": 1234,
 *        "dns_disp": 1,
 *        "sockconn_us": 1234,
 *        "tls_us": 1234,
 *        "txn_resp_us": 1234,
 *        "dns":["46.105.127.147", "2001:41d0:2:ee93::1"],
 *        "prot_specific": {
 *        	"protocol": "http",
 *        	"resp": 200
 *        }
 *      }
 *
 * The indexes in "dns_disp" are declared in lws_conmon_dns_disposition_t
 * below.
 *
 * "prot_specific" may not be present if the protocol doesn't have anything
 * to report or is not supported.
 */

typedef enum lws_conmon_pcol {
	LWSCONMON_PCOL_NONE,
	LWSCONMON_PCOL_HTTP, /* .protocol_specific.http is valid */
} lws_conmon_pcol_t;

typedef enum lws_conmon_dns_disposition {
	LWSCONMON_DNS_NONE,
	/**< did not attempt DNS */
	LWSCONMON_DNS_OK				= 1,
	/**< DNS lookup did give results */
	LWSCONMON_DNS_SERVER_UNREACHABLE		= 2,
	/**< DNS server was not reachable */
	LWSCONMON_DNS_NO_RESULT				= 3
	/**< DNS server replied but nothing usable */
} lws_conmon_dns_disposition_t;

struct lws_conmon {
	lws_sockaddr46				peer46;
	/**< The peer we actually connected to, if any.  .peer46.sa4.sa_family
	 * is either 0 if invalid, or the AF_ */

	union {
		struct {
			int	response;
			/**< h1 http response code */
		} http;
	} protocol_specific;
	/**< possibly-present protocol-specific additional information.  This
	 * is only valid for the first transaction after connection and does
	 * not capture results for persistent or muxed connections like ws
	 * messages, mqtt messages, or h2 streams */

	struct addrinfo				*dns_results_copy;
	/**< NULL, or Allocated copy of dns results, owned by this object and
	 * freed when object destroyed.
	 * Only set if client flag LCCSCF_CONMON applied  */

	lws_conmon_interval_us_t		ciu_dns;
	/**< 0, or if a socket connection, us taken to acquire this DNS response
	 *
	 */
	lws_conmon_interval_us_t		ciu_sockconn;
	/**< 0, or if connection-based, the us interval between the socket
	 * connect() attempt that succeeded, and the connection setup */
	lws_conmon_interval_us_t		ciu_tls;
	/**< 0 if no tls, or us taken to establish the tls tunnel */
	lws_conmon_interval_us_t		ciu_txn_resp;
	/**< 0, or if the protocol supports transactions, the interval between
	 * sending the initial transaction request and starting to receive the
	 * response */

	lws_conmon_pcol_t			pcol;
	/**< indicates which extra protocol_specific info member is valid,
	 *   if any */

	lws_conmon_dns_disposition_t		dns_disposition;
	/**< indicates general disposition of DNS request */
};

/**
 * lws_conmon_wsi_take() - create a connection latency object from client wsi
 *
 * \param context: lws wsi
 * \param dest: conmon struct to fill
 *
 * Copies wsi conmon data into the caller's struct.  Passes ownership of
 * any allocations in the addrinfo list to the caller, lws will not delete that
 * any more on wsi close after this call.  The caller must call
 * lws_conmon_release() on the struct to destroy any addrinfo in the struct
 * that is prepared by this eventually but it can defer it as long as it wants.
 *
 * Other than the addrinfo list, the contents of the returned object are
 * completely selfcontained and don't point outside of the object itself, ie,
 * everything else in there remains in scope while the object itself does.
 */
LWS_VISIBLE LWS_EXTERN void
lws_conmon_wsi_take(struct lws *wsi, struct lws_conmon *dest);

/**
 * lws_conmon_release() - free any allocations in the conmon struct
 *
 * \param conmon: pointer to conmon struct
 *
 * Destroys any allocations in the conmon struct so it can go out of scope.
 * It doesn't free \p dest itself, it's designed to clean out a struct that
 * is on the stack or embedded in another object.
 */
LWS_VISIBLE LWS_EXTERN void
lws_conmon_release(struct lws_conmon *conmon);

///@}
