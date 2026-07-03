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
 *
 * This provides a clean way to interface lws user code to be able to
 * work unchanged on different systems for fetching common system information,
 * and performing common system operations like reboot.
 */

/*
 * Types of system blob that can be set and retreived
 */

typedef enum {
	LWS_SYSBLOB_TYPE_AUTH,
	LWS_SYSBLOB_TYPE_CLIENT_CERT_DER = LWS_SYSBLOB_TYPE_AUTH + 2,
	LWS_SYSBLOB_TYPE_CLIENT_KEY_DER,
	LWS_SYSBLOB_TYPE_DEVICE_SERIAL,
	LWS_SYSBLOB_TYPE_DEVICE_FW_VERSION,
	LWS_SYSBLOB_TYPE_DEVICE_TYPE,
	LWS_SYSBLOB_TYPE_NTP_SERVER,
	LWS_SYSBLOB_TYPE_MQTT_CLIENT_ID,
	LWS_SYSBLOB_TYPE_MQTT_USERNAME,
	LWS_SYSBLOB_TYPE_MQTT_PASSWORD,

#if defined(LWS_WITH_SECURE_STREAMS_AUTH_SIGV4)
	/* extend 4 more auth blobs, each has 2 slots */
	LWS_SYSBLOB_TYPE_EXT_AUTH1,
	LWS_SYSBLOB_TYPE_EXT_AUTH2 = LWS_SYSBLOB_TYPE_EXT_AUTH1 + 2,
	LWS_SYSBLOB_TYPE_EXT_AUTH3 = LWS_SYSBLOB_TYPE_EXT_AUTH2 + 2,
	LWS_SYSBLOB_TYPE_EXT_AUTH4 = LWS_SYSBLOB_TYPE_EXT_AUTH3 + 2,
	LWS_SYSBLOB_TYPE_EXT_AUTH4_1,
#endif

	LWS_SYSBLOB_TYPE_COUNT /* ... always last */
} lws_system_blob_item_t;

/* opaque generic blob whose content may be on-the-heap or pointed-to
 * directly case by case.  When it's on the heap, it can be produced by
 * appending (it's a buflist underneath).  Either way, it can be consumed by
 * copying out a given length from a given offset.
 */

typedef struct lws_system_blob lws_system_blob_t;

LWS_EXTERN LWS_VISIBLE void
lws_system_blob_direct_set(lws_system_blob_t *b, const uint8_t *ptr, size_t len);

LWS_EXTERN LWS_VISIBLE void
lws_system_blob_heap_empty(lws_system_blob_t *b);

LWS_EXTERN LWS_VISIBLE int
lws_system_blob_heap_append(lws_system_blob_t *b, const uint8_t *ptr, size_t len);

LWS_EXTERN LWS_VISIBLE size_t
lws_system_blob_get_size(lws_system_blob_t *b);

/* return 0 and sets *ptr to point to blob data if possible, nonzero = fail */
LWS_EXTERN LWS_VISIBLE int
lws_system_blob_get_single_ptr(lws_system_blob_t *b, const uint8_t **ptr);

LWS_EXTERN LWS_VISIBLE int
lws_system_blob_get(lws_system_blob_t *b, uint8_t *ptr, size_t *len, size_t ofs);

LWS_EXTERN LWS_VISIBLE void
lws_system_blob_destroy(lws_system_blob_t *b);

/*
 * Get the opaque blob for index idx of various system blobs.  Returns 0 if
 * *b was set otherwise nonzero means out of range
 */

LWS_EXTERN LWS_VISIBLE lws_system_blob_t *
lws_system_get_blob(struct lws_context *context, lws_system_blob_item_t type,
                    int idx);

/*
 * Lws view of system state... normal operation from user code perspective is
 * dependent on implicit (eg, knowing the date for cert validation) and
 * explicit dependencies.
 *
 * Bit of lws and user code can register notification handlers that can enforce
 * dependent operations before state transitions can complete.
 */

typedef enum { /* keep system_state_names[] in sync in context.c */
	LWS_SYSTATE_UNKNOWN,

	LWS_SYSTATE_CONTEXT_CREATED,	 /* context was just created */
	LWS_SYSTATE_INITIALIZED,	 /* protocols initialized.  Lws itself
					  * can operate normally */
	LWS_SYSTATE_IFACE_COLDPLUG,	 /* existing net ifaces iterated */
	LWS_SYSTATE_DHCP,		 /* at least one net iface configured */
	LWS_SYSTATE_CPD_PRE_TIME,	 /* Captive portal detect without valid
					  * time, good for non-https tests... if
					  * you care about it, implement and
					  * call lws_system_ops_t
					  * .captive_portal_detect_request()
					  * and move the state forward according
					  * to the result. */
	LWS_SYSTATE_TIME_VALID,		 /* ntpclient ran, or hw time valid...
					  * tls cannot work until we reach here
					  */
	LWS_SYSTATE_CPD_POST_TIME,	 /* Captive portal detect after time was
					  * time, good for https tests... if
					  * you care about it, implement and
					  * call lws_system_ops_t
					  * .captive_portal_detect_request()
					  * and move the state forward according
					  * to the result. */

	LWS_SYSTATE_POLICY_VALID,	 /* user code knows how to operate... */
	LWS_SYSTATE_REGISTERED,		 /* device has an identity... */
	LWS_SYSTATE_AUTH1,		 /* identity used for main auth token */
	LWS_SYSTATE_AUTH2,		 /* identity used for optional auth */

	LWS_SYSTATE_OPERATIONAL,	 /* user code can operate normally */

	LWS_SYSTATE_POLICY_INVALID,	 /* user code is changing its policies
					  * drop everything done with old
					  * policy, switch to new then enter
					  * LWS_SYSTATE_POLICY_VALID */
	LWS_SYSTATE_CONTEXT_DESTROYING,	 /* Context is being destroyed */
} lws_system_states_t;

/* Captive Portal Detect -related */

typedef enum {
	LWS_CPD_UNKNOWN = 0,	/* test didn't happen ince last DHCP acq yet */
	LWS_CPD_INTERNET_OK,	/* no captive portal: our CPD test passed OK,
				 * we can go out on the internet */
	LWS_CPD_CAPTIVE_PORTAL,	/* we inferred we're behind a captive portal */
	LWS_CPD_NO_INTERNET,	/* we couldn't touch anything */
} lws_cpd_result_t;

typedef void (*lws_attach_cb_t)(struct lws_context *context, int tsi, void *opaque);
struct lws_attach_item;

LWS_EXTERN LWS_VISIBLE int
lws_tls_jit_trust_got_cert_cb(struct lws_context *cx, void *got_opaque,
			      const uint8_t *skid, size_t skid_len,
			      const uint8_t *der, size_t der_len);

typedef struct lws_system_ops {
	int (*reboot)(void);
	int (*set_clock)(lws_usec_t us);
	int (*attach)(struct lws_context *context, int tsi, lws_attach_cb_t cb,
		      lws_system_states_t state, void *opaque,
		      struct lws_attach_item **get);
	/**< if \p get is NULL, add an attach callback request to the pt for
	 * \p cb with arg \p opaque, that should be called when we're at or past
	 * system state \p state.
	 *
	 * If \p get is non-NULL, look for the first listed item on the pt whose
	 * state situation is ready, and set *get to point to it.  If no items,
	 * or none where the system state is right, set *get to NULL.
	 *
	 * It's done like this so (*attach) can perform system-specific
	 * locking outside of lws core, for both getting and adding items the
	 * same so it is thread-safe.  A non-threadsafe helper
	 * __lws_system_attach() is provided to do the actual work inside the
	 * system-specific locking.
	 */
	int (*captive_portal_detect_request)(struct lws_context *context);
	/**< Check if we can go out on the internet cleanly, or if we are being
	 * redirected or intercepted by a captive portal.
	 * Start the check that proceeds asynchronously, and report the results
	 * by calling lws_captive_portal_detect_result() api
	 */

	int (*metric_report)(lws_metric_pub_t *mdata);
	/**< metric \p item is reporting an event of kind \p rpt,
	 * held in \p mdata... return 0 to leave the metric object as it is,
	 * or nonzero to reset it. */

	int (*jit_trust_query)(struct lws_context *cx, const uint8_t *skid,
			       size_t skid_len, void *got_opaque);
	/**< user defined trust store search, if we do trust a cert with SKID
	 * matching skid / skid_len, then it should get hold of the DER for the
	 * matching root CA and call
	 * lws_tls_jit_trust_got_cert_cb(..., got_opaque) before cleaning up and
	 * returning.  The DER should be destroyed if in heap before returning.
	 */

	uint32_t	wake_latency_us;
	/**< time taken for this device to wake from suspend, in us
	 */
} lws_system_ops_t;

#if defined(LWS_WITH_SYS_STATE)

/**
 * lws_system_get_state_manager() - return the state mgr object for system state
 *
 * \param context: the lws_context
 *
 * The returned pointer can be used with the lws_state_ apis
 */

LWS_EXTERN LWS_VISIBLE lws_state_manager_t *
lws_system_get_state_manager(struct lws_context *context);

#endif

/* wrappers handle NULL members or no ops struct set at all cleanly */

#define LWSSYSGAUTH_HEX (1 << 0)

/**
 * lws_system_get_ops() - get ahold of the system ops struct from the context
 *
 * \param context: the lws_context
 *
 * Returns the system ops struct.  It may return NULL and if not, anything in
 * there may be NULL.
 */
LWS_EXTERN LWS_VISIBLE const lws_system_ops_t *
lws_system_get_ops(struct lws_context *context);

#if defined(LWS_WITH_SYS_STATE)

/**
 * lws_system_context_from_system_mgr() - return context from system state mgr
 *
 * \param mgr: pointer to specifically the system state mgr
 *
 * Returns the context from the system state mgr.  Helper since the lws_context
 * is opaque.
 */
LWS_EXTERN LWS_VISIBLE struct lws_context *
lws_system_context_from_system_mgr(lws_state_manager_t *mgr);

#endif

/**
 * __lws_system_attach() - get and set items on context attach list
 *
 * \param context: context to get or set attach items to
 * \param tsi: thread service index (normally 0)
 * \param cb: callback to call from context event loop thread
 * \param state: the lws_system state we have to be in or have passed through
 * \param opaque: optional pointer to user specific info given to callback
 * \param get: NULL, or pointer to pointer to take detached tail item on exit
 *
 * This allows other threads to enqueue callback requests to happen from a pt's
 * event loop thread safely.  The callback gets the context pointer and a user
 * opaque pointer that can be optionally given when the item is added to the
 * attach list.
 *
 * This api is the no-locking core function for getting and setting items on the
 * pt's attach list.  The lws_system operation (*attach) is the actual
 * api that user and internal code calls for this feature, it should perform
 * system-specific locking, call this helper, release the locking and then
 * return the result.  This api is public only so it can be used in the locked
 * implementation of (*attach).
 *
 * If get is NULL, then the call adds to the head of the pt attach list using
 * cb, state, and opaque; if get is non-NULL, then *get is set to the first
 * waiting attached item that meets the state criteria and that item is removed
 * from the list.
 *
 * This is a non-threadsafe helper only designed to be called from
 * implementations of struct lws_system's (*attach) operation where system-
 * specific locking has been applied around it, making it threadsafe.
 */
LWS_EXTERN LWS_VISIBLE int
__lws_system_attach(struct lws_context *context, int tsi, lws_attach_cb_t cb,
		    lws_system_states_t state, void *opaque,
		    struct lws_attach_item **get);


enum {
	LWSDH_IPV4_SUBNET_MASK		= 0,
	LWSDH_IPV4_BROADCAST,
	LWSDH_LEASE_SECS,
	LWSDH_REBINDING_SECS,
	LWSDH_RENEWAL_SECS,

	_LWSDH_NUMS_COUNT,

	LWSDH_SA46_IP			= 0,
	LWSDH_SA46_DNS_SRV_1,
	LWSDH_SA46_DNS_SRV_2,
	LWSDH_SA46_DNS_SRV_3,
	LWSDH_SA46_DNS_SRV_4,
	LWSDH_SA46_IPV4_ROUTER,
	LWSDH_SA46_NTP_SERVER,
	LWSDH_SA46_DHCP_SERVER,

	_LWSDH_SA46_COUNT,
};

typedef struct lws_dhcpc_ifstate {
	char				ifname[16];
	char				domain[64];
	uint8_t				mac[6];
	uint32_t			nums[_LWSDH_NUMS_COUNT];
	lws_sockaddr46			sa46[_LWSDH_SA46_COUNT];
} lws_dhcpc_ifstate_t;

typedef int (*dhcpc_cb_t)(void *opaque, lws_dhcpc_ifstate_t *is);

/**
 * lws_dhcpc_request() - add a network interface to dhcpc management
 *
 * \param c: the lws_context
 * \param i: the interface name, like "eth0"
 * \param af: address family
 * \param cb: the change callback
 * \param opaque: opaque pointer given to the callback
 *
 * Register a network interface as being managed by DHCP.  lws will proceed to
 * try to acquire an IP.  Requires LWS_WITH_SYS_DHCP_CLIENT at cmake.
 */
LWS_EXTERN LWS_VISIBLE int
lws_dhcpc_request(struct lws_context *c, const char *i, int af, dhcpc_cb_t cb,
		void *opaque);

/**
 * lws_dhcpc_remove() - remove a network interface to dhcpc management
 *
 * \param context: the lws_context
 * \param iface: the interface name, like "eth0"
 *
 * Remove handling of the network interface from dhcp.
 */
LWS_EXTERN LWS_VISIBLE int
lws_dhcpc_remove(struct lws_context *context, const char *iface);

/**
 * lws_dhcpc_status() - has any interface reached BOUND state
 *
 * \param context: the lws_context
 * \param sa46: set to a DNS server from a bound interface, or NULL
 *
 * Returns 1 if any network interface managed by dhcpc has reached the BOUND
 * state (has acquired an IP, gateway and DNS server), otherwise 0.
 */
LWS_EXTERN LWS_VISIBLE int
lws_dhcpc_status(struct lws_context *context, lws_sockaddr46 *sa46);

/**
 * lws_system_cpd_start() - helper to initiate captive portal detection
 *
 * \param context: the lws_context
 *
 * Resets the context's captive portal state to LWS_CPD_UNKNOWN and calls the
 * lws_system_ops_t captive_portal_detect_request() implementation to begin
 * testing the captive portal state.
 */
LWS_EXTERN LWS_VISIBLE int
lws_system_cpd_start(struct lws_context *context);

LWS_EXTERN LWS_VISIBLE void
lws_system_cpd_start_defer(struct lws_context *cx, lws_usec_t defer_us);


/**
 * lws_system_cpd_set() - report the result of the captive portal detection
 *
 * \param context: the lws_context
 * \param result: one of the LWS_CPD_ constants representing captive portal state
 *
 * Sets the context's captive portal detection state to result.  User captive
 * portal detection code would call this once it had a result from its test.
 */
LWS_EXTERN LWS_VISIBLE void
lws_system_cpd_set(struct lws_context *context, lws_cpd_result_t result);


/**
 * lws_system_cpd_state_get() - returns the last tested captive portal state
 *
 * \param context: the lws_context
 *
 * Returns one of the LWS_CPD_ constants indicating the system's understanding
 * of the current captive portal situation.
 */
LWS_EXTERN LWS_VISIBLE lws_cpd_result_t
lws_system_cpd_state_get(struct lws_context *context);
