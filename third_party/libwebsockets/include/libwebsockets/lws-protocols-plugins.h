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

/*! \defgroup Protocols-and-Plugins Protocols and Plugins
 * \ingroup lwsapi
 *
 * ##Protocol and protocol plugin -related apis
 *
 * Protocols bind ws protocol names to a custom callback specific to that
 * protocol implementaion.
 *
 * A list of protocols can be passed in at context creation time, but it is
 * also legal to leave that NULL and add the protocols and their callback code
 * using plugins.
 *
 * Plugins are much preferable compared to cut and pasting code into an
 * application each time, since they can be used standalone.
 */
///@{
/** struct lws_protocols -	List of protocols and handlers client or server
 *					supports. */

struct lws_protocols {
	const char *name;
	/**< Protocol name that must match the one given in the client
	 * Javascript new WebSocket(url, 'protocol') name. */
	lws_callback_function *callback;
	/**< The service callback used for this protocol.  It allows the
	 * service action for an entire protocol to be encapsulated in
	 * the protocol-specific callback */
	size_t per_session_data_size;
	/**< Each new connection using this protocol gets
	 * this much memory allocated on connection establishment and
	 * freed on connection takedown.  A pointer to this per-connection
	 * allocation is passed into the callback in the 'user' parameter */
	size_t rx_buffer_size;
	/**< lws allocates this much space for rx data and informs callback
	 * when something came.  Due to rx flow control, the callback may not
	 * be able to consume it all without having to return to the event
	 * loop.  That is supported in lws.
	 *
	 * If .tx_packet_size is 0, this also controls how much may be sent at
	 * once for backwards compatibility.
	 */
	unsigned int id;
	/**< ignored by lws, but useful to contain user information bound
	 * to the selected protocol.  For example if this protocol was
	 * called "myprotocol-v2", you might set id to 2, and the user
	 * code that acts differently according to the version can do so by
	 * switch (wsi->a.protocol->id), user code might use some bits as
	 * capability flags based on selected protocol version, etc. */
	void *user; /**< ignored by lws, but user code can pass a pointer
			here it can later access from the protocol callback */
	size_t tx_packet_size;
	/**< 0 indicates restrict send() size to .rx_buffer_size for backwards-
	 * compatibility.
	 * If greater than zero, a single send() is restricted to this amount
	 * and any remainder is buffered by lws and sent afterwards also in
	 * these size chunks.  Since that is expensive, it's preferable
	 * to restrict one fragment you are trying to send to match this
	 * size.
	 */

	/* Add new things just above here ---^
	 * This is part of the ABI, don't needlessly break compatibility */
};

#define LWS_PROTOCOL_LIST_TERM { NULL, NULL, 0, 0, 0, NULL, 0 }

/**
 * lws_vhost_name_to_protocol() - get vhost's protocol object from its name
 *
 * \param vh: vhost to search
 * \param name: protocol name
 *
 * Returns NULL or a pointer to the vhost's protocol of the requested name
 */
LWS_VISIBLE LWS_EXTERN const struct lws_protocols *
lws_vhost_name_to_protocol(struct lws_vhost *vh, const char *name);

/**
 * lws_get_protocol() - Returns a protocol pointer from a websocket
 *				  connection.
 * \param wsi:	pointer to struct websocket you want to know the protocol of
 *
 *
 *	Some apis can act on all live connections of a given protocol,
 *	this is how you can get a pointer to the active protocol if needed.
 */
LWS_VISIBLE LWS_EXTERN const struct lws_protocols *
lws_get_protocol(struct lws *wsi);

/** lws_protocol_get() -  deprecated: use lws_get_protocol */
LWS_VISIBLE LWS_EXTERN const struct lws_protocols *
lws_protocol_get(struct lws *wsi) LWS_WARN_DEPRECATED;

/**
 * lws_protocol_vh_priv_zalloc() - Allocate and zero down a protocol's per-vhost
 *				   storage
 * \param vhost:	vhost the instance is related to
 * \param prot:		protocol the instance is related to
 * \param size:		bytes to allocate
 *
 * Protocols often find it useful to allocate a per-vhost struct, this is a
 * helper to be called in the per-vhost init LWS_CALLBACK_PROTOCOL_INIT
 */
LWS_VISIBLE LWS_EXTERN void *
lws_protocol_vh_priv_zalloc(struct lws_vhost *vhost,
			    const struct lws_protocols *prot, int size);

/**
 * lws_protocol_vh_priv_get() - retreive a protocol's per-vhost storage
 *
 * \param vhost:	vhost the instance is related to
 * \param prot:		protocol the instance is related to
 *
 * Recover a pointer to the allocated per-vhost storage for the protocol created
 * by lws_protocol_vh_priv_zalloc() earlier
 */
LWS_VISIBLE LWS_EXTERN void *
lws_protocol_vh_priv_get(struct lws_vhost *vhost,
			 const struct lws_protocols *prot);

/**
 * lws_vhd_find_by_pvo() - find a partner vhd
 *
 *  \param cx: the lws_context
 *  \param protname: the name of the lws_protocol the vhd belongs to
 *  \param pvo_name: the name of a pvo that must exist bound to the vhd
 *  \param pvo_value: the required value of the named pvo
 *
 * This allows architectures with multiple protocols bound together to
 * cleanly discover partner protocol instances even on completely
 * different vhosts.  For example, a proxy may consist of two protocols
 * listening on different vhosts, and there may be multiple instances
 * of the proxy in the same process.  It's desirable that each side of
 * the proxy is an independent protocol that can be freely bound to any
 * vhost, eg, allowing Unix Domain to tls / h2 proxying, or each side
 * bound to different network interfaces for localhost-only visibility
 * on one side, using existing vhost management.
 *
 * That leaves the problem that the two sides have to find each other
 * and bind at runtime.  This api allows each side to specify the
 * protocol name, and a common pvo name and pvo value that indicates
 * the two sides belong together, and search through all the instantiated
 * vhost-protocols looking for a match.  If found, the private allocation
 * (aka "vhd" of the match is returned).  NULL is returned on no match.
 *
 * Since this can only succeed when called by the last of the two
 * protocols to be instantiated, both sides should call it and handle
 * NULL gracefully, since it may mean that they were first and their
 * partner vhsot-protocol has not been instantiated yet.
 */
LWS_VISIBLE LWS_EXTERN void *
lws_vhd_find_by_pvo(struct lws_context *cx, const char *protname,
		    const char *pvo_name, const char *pvo_value);


/**
 * lws_adjust_protocol_psds - change a vhost protocol's per session data size
 *
 * \param wsi: a connection with the protocol to change
 * \param new_size: the new size of the per session data size for the protocol
 *
 * Returns user_space for the wsi, after allocating
 *
 * This should not be used except to initalize a vhost protocol's per session
 * data size one time, before any connections are accepted.
 *
 * Sometimes the protocol wraps another protocol and needs to discover and set
 * its per session data size at runtime.
 */
LWS_VISIBLE LWS_EXTERN void *
lws_adjust_protocol_psds(struct lws *wsi, size_t new_size);

/**
 * lws_finalize_startup() - drop initial process privileges
 *
 * \param context:	lws context
 *
 * This is called after the end of the vhost protocol initializations, but
 * you may choose to call it earlier
 */
LWS_VISIBLE LWS_EXTERN int
lws_finalize_startup(struct lws_context *context);

/**
 * lws_pvo_search() - helper to find a named pvo in a linked-list
 *
 * \param pvo:	the first pvo in the linked-list
 * \param name: the name of the pvo to return if found
 *
 * Returns NULL, or a pointer to the name pvo in the linked-list
 */
LWS_VISIBLE LWS_EXTERN const struct lws_protocol_vhost_options *
lws_pvo_search(const struct lws_protocol_vhost_options *pvo, const char *name);

/**
 * lws_pvo_get_str() - retreive a string pvo value
 *
 * \param in:	the first pvo in the linked-list
 * \param name: the name of the pvo to return if found
 * \param result: pointer to a const char * to get the result if any
 *
 * Returns 0 if found and *result set, or nonzero if not found
 */
LWS_VISIBLE LWS_EXTERN int
lws_pvo_get_str(void *in, const char *name, const char **result);

LWS_VISIBLE LWS_EXTERN int
lws_protocol_init(struct lws_context *context);

#define LWS_PLUGIN_API_MAGIC 191

/*
 * Abstract plugin header for any kind of plugin class, always at top of
 * actual class plugin export type.
 *
 * The export type object must be exported with the same name as the plugin
 * file, eg, libmyplugin.so must export a const one of these as the symbol
 * "myplugin".
 *
 * That is the only expected export from the plugin.
 */

typedef struct lws_plugin_header {
	const char *name;
	const char *_class;
	const char *lws_build_hash; /* set to LWS_BUILD_HASH */

	unsigned int api_magic;
	/* set to LWS_PLUGIN_API_MAGIC at plugin build time */

	/* plugin-class specific superclass data follows */
} lws_plugin_header_t;

/*
 * "lws_protocol_plugin" class export, for lws_protocol implementations done
 * as plugins
 */
typedef struct lws_plugin_protocol {
	lws_plugin_header_t hdr;

	const struct lws_protocols *protocols; /**< array of supported protocols provided by plugin */
	const struct lws_extension *extensions; /**< array of extensions provided by plugin */
	int count_protocols; /**< how many protocols */
	int count_extensions; /**< how many extensions */
} lws_plugin_protocol_t;


/*
 * This is the dynamic, runtime created part of the plugin instantiation.
 * These are kept in a linked-list and destroyed with the context.
 */

struct lws_plugin {
	struct lws_plugin *list; /**< linked list */

	const lws_plugin_header_t *hdr;

	union {
#if defined(LWS_WITH_LIBUV) && defined(UV_ERRNO_MAP)
#if (UV_VERSION_MAJOR > 0)
		uv_lib_t lib; /**< shared library pointer */
#endif
#endif
		void *l; /**<  */
	} u;
};

/*
 * Event lib library plugin type (when LWS_WITH_EVLIB_PLUGINS)
 * Public so new event libs can equally be supported outside lws itself
 */

typedef struct lws_plugin_evlib {
	lws_plugin_header_t hdr;
	const struct lws_event_loop_ops *ops;
} lws_plugin_evlib_t;

typedef int (*each_plugin_cb_t)(struct lws_plugin *p, void *user);

/**
 * lws_plugins_init() - dynamically load plugins of matching class from dirs
 *
 * \param pplugin:	pointer to linked-list for this kind of plugin
 * \param d: array of directory paths to look in
 * \param _class: class string that plugin must declare
 * \param filter: NULL, or a string that must appear after the third char of the plugin filename
 * \param each: NULL, or each_plugin_cb_t callback for each instantiated plugin
 * \param each_user: pointer passed to each callback
 *
 * Allows you to instantiate a class of plugins to a specified linked-list.
 * The each callback allows you to init each inistantiated callback and pass a
 * pointer each_user to it.
 *
 * To take down the plugins, pass a pointer to the linked-list head to
 * lws_plugins_destroy.
 *
 * This is used for lws protocol plugins but you can define your own plugin
 * class name like "mypluginclass", declare it in your plugin headers, and load
 * your own plugins to your own list using this api the same way.
 */
LWS_VISIBLE LWS_EXTERN int
lws_plugins_init(struct lws_plugin **pplugin, const char * const *d,
		 const char *_class, const char *filter,
		 each_plugin_cb_t each, void *each_user);

/**
 * lws_plugins_destroy() - dynamically unload list of plugins
 *
 * \param pplugin:	pointer to linked-list for this kind of plugin
 * \param each: NULL, or each_plugin_cb_t callback for each instantiated plugin
 * \param each_user: pointer passed to each callback
 *
 * Allows you to destroy a class of plugins from a specified linked-list
 * created by a call to lws_plugins_init().
 *
 * The each callback allows you to deinit each inistantiated callback and pass a
 * pointer each_user to it, just before its footprint is destroyed.
 */
LWS_VISIBLE LWS_EXTERN int
lws_plugins_destroy(struct lws_plugin **pplugin, each_plugin_cb_t each,
		    void *each_user);

#if defined(LWS_WITH_PLUGINS_BUILTIN)

/* provide exports for builtin plugin protocols */

extern const struct lws_protocols post_demo_protocols[1];
extern const struct lws_protocols lws_raw_proxy_protocols[1];
extern const struct lws_protocols lws_status_protocols[1];
extern const struct lws_protocols lws_mirror_protocols[1];
extern const struct lws_protocols lws_ssh_base_protocols[2];
extern const struct lws_protocols post_demo_protocols[1];
extern const struct lws_protocols dumb_increment_protocols[1];
extern const struct lws_protocols deaddrop_protocols[1];
extern const struct lws_protocols lws_raw_test_protocols[1];
extern const struct lws_protocols lws_sshd_demo_protocols[1];
extern const struct lws_protocols lws_acme_client_protocols[1];
extern const struct lws_protocols client_loopback_test_protocols[1];
extern const struct lws_protocols fulltext_demo_protocols[1];
extern const struct lws_protocols lws_openmetrics_export_protocols[
#if defined(LWS_WITH_SERVER) && defined(LWS_WITH_CLIENT) && defined(LWS_ROLE_WS)
	4
#else
#if defined(LWS_WITH_SERVER)
	3
#else
	1
#endif
#endif
	];

#define LWSOMPROIDX_DIRECT_HTTP_SERVER		0
#define LWSOMPROIDX_PROX_HTTP_SERVER		1
#define LWSOMPROIDX_PROX_WS_SERVER		2
#define LWSOMPROIDX_PROX_WS_CLIENT		3

#endif

///@}
