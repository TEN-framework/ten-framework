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

/*! \defgroup context-and-vhost context and vhost related functions
 * ##Context and Vhost releated functions
 * \ingroup lwsapi
 *
 *
 *  LWS requires that there is one context, in which you may define multiple
 *  vhosts.  Each vhost is a virtual host, with either its own listen port
 *  or sharing an existing one.  Each vhost has its own SSL context that can
 *  be set up individually or left disabled.
 *
 *  If you don't care about multiple "site" support, you can ignore it and
 *  lws will create a single default vhost at context creation time.
 */
///@{

/*
 * NOTE: These public enums are part of the abi.  If you want to add one,
 * add it at where specified so existing users are unaffected.
 */


#define LWS_SERVER_OPTION_REQUIRE_VALID_OPENSSL_CLIENT_CERT	 ((1ll << 1) | \
								  (1ll << 12))
	/**< (VH) Don't allow the connection unless the client has a
	 * client cert that we recognize; provides
	 * LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT */
#define LWS_SERVER_OPTION_SKIP_SERVER_CANONICAL_NAME		  (1ll << 2)
	/**< (CTX) Don't try to get the server's hostname */
#define LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT		 ((1ll << 3) | \
								  (1ll << 12))
	/**< (VH) Allow non-SSL (plaintext) connections on the same
	 * port as SSL is listening.  If combined with
	 * LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS it will try to
	 * force http connections on an https listener (eg, http://x.com:443) to
	 * redirect to an explicit https connection (eg, https://x.com)
	 */
#define LWS_SERVER_OPTION_LIBEV					 (1ll << 4)
	/**< (CTX) Use libev event loop */
#define LWS_SERVER_OPTION_DISABLE_IPV6				 (1ll << 5)
	/**< (VH) Disable IPV6 support */
#define LWS_SERVER_OPTION_DISABLE_OS_CA_CERTS			 (1ll << 6)
	/**< (VH) Don't load OS CA certs, you will need to load your
	 * own CA cert(s) */
#define LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED		 (1ll << 7)
	/**< (VH) Accept connections with no valid Cert (eg, selfsigned) */
#define LWS_SERVER_OPTION_VALIDATE_UTF8				 (1ll << 8)
	/**< (VH) Check UT-8 correctness */
#define LWS_SERVER_OPTION_SSL_ECDH				 ((1ll << 9) | \
								  (1ll << 12))
	/**< (VH)  initialize ECDH ciphers */
#define LWS_SERVER_OPTION_LIBUV					(1ll << 10)
	/**< (CTX)  Use libuv event loop */
#define LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS		((1ll << 11) |\
								 (1ll << 12))
	/**< (VH) Use an http redirect to force the client to ask for https.
	 * Notice if your http server issues the STS header and the client has
	 * ever seen that, the client will fail the http connection before it
	 * can actually do the redirect.
	 *
	 * Combine with LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS to handle, eg,
	 * http://x.com:443 -> https://x.com
	 *
	 * (deprecated: use mount redirection) */
#define LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT			 (1ll << 12)
	/**< (CTX) Initialize the SSL library at all */
#define LWS_SERVER_OPTION_EXPLICIT_VHOSTS			 (1ll << 13)
	/**< (CTX) Only create the context when calling context
	 * create api, implies user code will create its own vhosts */
#define LWS_SERVER_OPTION_UNIX_SOCK				 (1ll << 14)
	/**< (VH) Use Unix socket */
#define LWS_SERVER_OPTION_STS					 (1ll << 15)
	/**< (VH) Send Strict Transport Security header, making
	 * clients subsequently go to https even if user asked for http */
#define LWS_SERVER_OPTION_IPV6_V6ONLY_MODIFY			 (1ll << 16)
	/**< (VH) Enable LWS_SERVER_OPTION_IPV6_V6ONLY_VALUE to take effect */
#define LWS_SERVER_OPTION_IPV6_V6ONLY_VALUE			 (1ll << 17)
	/**< (VH) if set, only ipv6 allowed on the vhost */
#define LWS_SERVER_OPTION_UV_NO_SIGSEGV_SIGFPE_SPIN		 (1ll << 18)
	/**< (CTX) Libuv only: Do not spin on SIGSEGV / SIGFPE.  A segfault
	 * normally makes the lib spin so you can attach a debugger to it
	 * even if it happened without a debugger in place.  You can disable
	 * that by giving this option.
	 */
#define LWS_SERVER_OPTION_JUST_USE_RAW_ORIGIN			 (1ll << 19)
	/**< For backwards-compatibility reasons, by default
	 * lws prepends "http://" to the origin you give in the client
	 * connection info struct.  If you give this flag when you create
	 * the context, only the string you give in the client connect
	 * info for .origin (if any) will be used directly.
	 */
#define LWS_SERVER_OPTION_FALLBACK_TO_RAW /* use below name */	 (1ll << 20)
#define LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG (1ll << 20)
	/**< (VH) if invalid http is coming in the first line, then abandon
	 * trying to treat the connection as http, and belatedly apply the
	 * .listen_accept_role / .listen_accept_protocol info struct members to
	 * the connection.  If they are NULL, for backwards-compatibility the
	 * connection is bound to "raw-skt" role, and in order of priority:
	 * 1) the vh protocol with a pvo named "raw", 2) the vh protocol with a
	 * pvo named "default", or 3) protocols[0].
	 *
	 * Must be combined with LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT
	 * to work with a socket listening with tls.
	 */

#define LWS_SERVER_OPTION_LIBEVENT				(1ll << 21)
	/**< (CTX) Use libevent event loop */

#define LWS_SERVER_OPTION_ONLY_RAW /* Use below name instead */	(1ll << 22)
#define LWS_SERVER_OPTION_ADOPT_APPLY_LISTEN_ACCEPT_CONFIG	(1ll << 22)
	/**< (VH) All connections to this vhost / port are bound to the
	 * role and protocol given in .listen_accept_role /
	 * .listen_accept_protocol.
	 *
	 * If those explicit user-controlled names are NULL, for backwards-
	 * compatibility the connection is bound to "raw-skt" role, and in order
	 * of priority: 1) the vh protocol with a pvo named "raw", 2) the vh
	 * protocol with a pvo named "default", or 3) protocols[0].
	 *
	 * It's much preferred to specify the role + protocol using the
	 * .listen_accept_role and .listen_accept_protocol in the info struct.
	 */
#define LWS_SERVER_OPTION_ALLOW_LISTEN_SHARE			(1ll << 23)
	/**< (VH) Set to allow multiple listen sockets on one interface +
	 * address + port.  The default is to strictly allow only one
	 * listen socket at a time.  This is automatically selected if you
	 * have multiple service threads.  Linux only.
	 */
#define LWS_SERVER_OPTION_CREATE_VHOST_SSL_CTX			(1ll << 24)
	/**< (VH) Force setting up the vhost SSL_CTX, even though the user
	 * code doesn't explicitly provide a cert in the info struct.  It
	 * implies the user code is going to provide a cert at the
	 * LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS callback, which
	 * provides the vhost SSL_CTX * in the user parameter.
	 */
#define LWS_SERVER_OPTION_SKIP_PROTOCOL_INIT			(1ll << 25)
	/**< (VH) You probably don't want this.  It forces this vhost to not
	 * call LWS_CALLBACK_PROTOCOL_INIT on its protocols.  It's used in the
	 * special case of a temporary vhost bound to a single protocol.
	 */
#define LWS_SERVER_OPTION_IGNORE_MISSING_CERT			(1ll << 26)
	/**< (VH) Don't fail if the vhost TLS cert or key are missing, just
	 * continue.  The vhost won't be able to serve anything, but if for
	 * example the ACME plugin was configured to fetch a cert, this lets
	 * you bootstrap your vhost from having no cert to start with.
	 */
#define LWS_SERVER_OPTION_VHOST_UPG_STRICT_HOST_CHECK		(1ll << 27)
	/**< (VH) On this vhost, if the connection is being upgraded, insist
	 * that there's a Host: header and that the contents match the vhost
	 * name + port (443 / 80 are assumed if no :port given based on if the
	 * connection is using TLS).
	 *
	 * By default, without this flag, on upgrade lws just checks that the
	 * Host: header was given without checking the contents... this is to
	 * allow lax hostname mappings like localhost / 127.0.0.1, and CNAME
	 * mappings like www.mysite.com / mysite.com
	 */
#define LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE (1ll << 28)
	/**< (VH) Send lws default HTTP headers recommended by Mozilla
	 * Observatory for security.  This is a helper option that sends canned
	 * headers on each http response enabling a VERY strict Content Security
	 * Policy.  The policy is so strict, for example it won't let the page
	 * run its own inline JS nor show images or take CSS from a different
	 * server.  In many cases your JS only comes from your server as do the
	 * image sources and CSS, so that is what you want... attackers hoping
	 * to inject JS into your DOM are completely out of luck since even if
	 * they succeed, it will be rejected for execution by the browser
	 * according to the strict CSP.  In other cases you have to deviate from
	 * the complete strictness, in which case don't use this flag: use the
	 * .headers member in the vhost init described in struct
	 * lws_context_creation_info instead to send the adapted headers
	 * yourself.
	 */

#define LWS_SERVER_OPTION_ALLOW_HTTP_ON_HTTPS_LISTENER		 (1ll << 29)
	/**< (VH) If you really want to allow HTTP connections on a tls
	 * listener, you can do it with this combined with
	 * LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT.  But this is allowing
	 * accidental loss of the security assurances provided by tls depending
	 * on the client using http when he meant https... it's not
	 * recommended.
	 */
#define LWS_SERVER_OPTION_FAIL_UPON_UNABLE_TO_BIND		 (1ll << 30)
	/**< (VH) When instantiating a new vhost and the specified port is
	 * already in use, a null value shall be return to signal the error.
	 */

#define LWS_SERVER_OPTION_H2_JUST_FIX_WINDOW_UPDATE_OVERFLOW	 (1ll << 31)
	/**< (VH) Indicates the connections using this vhost should ignore
	 * h2 WINDOW_UPDATE from broken peers and fix them up */

#define LWS_SERVER_OPTION_VH_H2_HALF_CLOSED_LONG_POLL		 (1ll << 32)
	/**< (VH) Tell the vhost to treat half-closed remote clients as
	 * entered into an immortal (ie, not subject to normal timeouts) long
	 * poll mode.
	 */

#define LWS_SERVER_OPTION_GLIB					 (1ll << 33)
	/**< (CTX) Use glib event loop */

#define LWS_SERVER_OPTION_H2_PRIOR_KNOWLEDGE			 (1ll << 34)
	/**< (VH) Tell the vhost to treat plain text http connections as
	 * H2 with prior knowledge (no upgrade request involved)
	 */

#define LWS_SERVER_OPTION_NO_LWS_SYSTEM_STATES			 (1ll << 35)
	/**< (CTX) Disable lws_system state, eg, because we are a secure streams
	 * proxy client that is not trying to track system state by itself. */

#define LWS_SERVER_OPTION_SS_PROXY				 (1ll << 36)
	/**< (VH) We are being a SS Proxy listen socket for the vhost */

#define LWS_SERVER_OPTION_SDEVENT			 	 (1ll << 37)
	/**< (CTX) Use sd-event loop */

#define LWS_SERVER_OPTION_ULOOP					 (1ll << 38)
	/**< (CTX) Use libubox / uloop event loop */

#define LWS_SERVER_OPTION_DISABLE_TLS_SESSION_CACHE		 (1ll << 39)
	/**< (VHOST) Disallow use of client tls caching (on by default) */


	/****** add new things just above ---^ ******/


#define lws_check_opt(c, f) ((((uint64_t)c) & ((uint64_t)f)) == ((uint64_t)f))

struct lws_plat_file_ops;
struct lws_ss_policy;
struct lws_ss_plugin;
struct lws_metric_policy;

typedef int (*lws_context_ready_cb_t)(struct lws_context *context);

typedef int (*lws_peer_limits_notify_t)(struct lws_context *ctx,
					lws_sockfd_type sockfd,
					lws_sockaddr46 *sa46);

/** struct lws_context_creation_info - parameters to create context and /or vhost with
 *
 * This is also used to create vhosts.... if LWS_SERVER_OPTION_EXPLICIT_VHOSTS
 * is not given, then for backwards compatibility one vhost is created at
 * context-creation time using the info from this struct.
 *
 * If LWS_SERVER_OPTION_EXPLICIT_VHOSTS is given, then no vhosts are created
 * at the same time as the context, they are expected to be created afterwards.
 */
struct lws_context_creation_info {
#if defined(LWS_WITH_NETWORK)
	const char *iface;
	/**< VHOST: NULL to bind the listen socket to all interfaces, or the
	 * interface name, eg, "eth2"
	 * If options specifies LWS_SERVER_OPTION_UNIX_SOCK, this member is
	 * the pathname of a UNIX domain socket. you can use the UNIX domain
	 * sockets in abstract namespace, by prepending an at symbol to the
	 * socket name. */
	const struct lws_protocols *protocols;
	/**< VHOST: Array of structures listing supported protocols and a
	 * protocol-specific callback for each one.  The list is ended with an
	 * entry that has a NULL callback pointer.  SEE ALSO .pprotocols below,
	 * which gives an alternative way to provide an array of pointers to
	 * protocol structs. */
#if defined(LWS_ROLE_WS)
	const struct lws_extension *extensions;
	/**< VHOST: NULL or array of lws_extension structs listing the
	 * extensions this context supports. */
#endif
#if defined(LWS_ROLE_H1) || defined(LWS_ROLE_H2)
	const struct lws_token_limits *token_limits;
	/**< CONTEXT: NULL or struct lws_token_limits pointer which is
	 * initialized with a token length limit for each possible WSI_TOKEN_ */
	const char *http_proxy_address;
	/**< VHOST: If non-NULL, attempts to proxy via the given address.
	 * If proxy auth is required, use format
	 * "username:password\@server:port" */
	const struct lws_protocol_vhost_options *headers;
		/**< VHOST: pointer to optional linked list of per-vhost
		 * canned headers that are added to server responses */

	const struct lws_protocol_vhost_options *reject_service_keywords;
	/**< CONTEXT: Optional list of keywords and rejection codes + text.
	 *
	 * The keywords are checked for existing in the user agent string.
	 *
	 * Eg, "badrobot" "404 Not Found"
	 */
	const struct lws_protocol_vhost_options *pvo;
	/**< VHOST: pointer to optional linked list of per-vhost
	 * options made accessible to protocols */
	const char *log_filepath;
	/**< VHOST: filepath to append logs to... this is opened before
	 *		any dropping of initial privileges */
	const struct lws_http_mount *mounts;
	/**< VHOST: optional linked list of mounts for this vhost */
	const char *server_string;
	/**< CONTEXT: string used in HTTP headers to identify server
	 * software, if NULL, "libwebsockets". */

	const char *error_document_404;
	/**< VHOST: If non-NULL, when asked to serve a non-existent file,
	 *          lws attempts to server this url path instead.  Eg,
	 *          "/404.html" */
	int port;
	/**< VHOST: Port to listen on. Use CONTEXT_PORT_NO_LISTEN to suppress
	 * listening for a client. Use CONTEXT_PORT_NO_LISTEN_SERVER if you are
	 * writing a server but you are using \ref sock-adopt instead of the
	 * built-in listener.
	 *
	 * You can also set port to 0, in which case the kernel will pick
	 * a random port that is not already in use.  You can find out what
	 * port the vhost is listening on using lws_get_vhost_listen_port() */

	unsigned int http_proxy_port;
	/**< VHOST: If http_proxy_address was non-NULL, uses this port */
	unsigned int max_http_header_data2;
	/**< CONTEXT: if max_http_header_data is 0 and this
	 * is nonzero, this will be used in place of the default.  It's
	 * like this for compatibility with the original short version,
	 * this is unsigned int length. */
	unsigned int max_http_header_pool2;
	/**< CONTEXT: if max_http_header_pool is 0 and this
	 * is nonzero, this will be used in place of the default.  It's
	 * like this for compatibility with the original short version:
	 * this is unsigned int length. */

	int keepalive_timeout;
	/**< VHOST: (default = 0 = 5s, 31s for http/2) seconds to allow remote
	 * client to hold on to an idle HTTP/1.1 connection.  Timeout lifetime
	 * applied to idle h2 network connections */
	uint32_t	http2_settings[7];
	/**< VHOST:  if http2_settings[0] is nonzero, the values given in
	 *	      http2_settings[1]..[6] are used instead of the lws
	 *	      platform default values.
	 *	      Just leave all at 0 if you don't care.
	 */

	unsigned short max_http_header_data;
	/**< CONTEXT: The max amount of header payload that can be handled
	 * in an http request (unrecognized header payload is dropped) */
	unsigned short max_http_header_pool;
	/**< CONTEXT: The max number of connections with http headers that
	 * can be processed simultaneously (the corresponding memory is
	 * allocated and deallocated dynamically as needed).  If the pool is
	 * fully busy new incoming connections must wait for accept until one
	 * becomes free. 0 = allow as many ah as number of availble fds for
	 * the process */

#endif

#if defined(LWS_WITH_TLS)
	const char *ssl_private_key_password;
	/**< VHOST: NULL or the passphrase needed for the private key. (For
	 * backwards compatibility, this can also be used to pass the client
	 * cert passphrase when setting up a vhost client SSL context, but it is
	 * preferred to use .client_ssl_private_key_password for that.) */
	const char *ssl_cert_filepath;
	/**< VHOST: If libwebsockets was compiled to use ssl, and you want
	 * to listen using SSL, set to the filepath to fetch the
	 * server cert from, otherwise NULL for unencrypted.  (For backwards
	 * compatibility, this can also be used to pass the client certificate
	 * when setting up a vhost client SSL context, but it is preferred to
	 * use .client_ssl_cert_filepath for that.)
	 *
	 * Notice you can alternatively set a single DER or PEM from a memory
	 * buffer as the vhost tls cert using \p server_ssl_cert_mem and
	 * \p server_ssl_cert_mem_len.
	 */
	const char *ssl_private_key_filepath;
	/**<  VHOST: filepath to private key if wanting SSL mode;
	 * if this is set to NULL but ssl_cert_filepath is set, the
	 * OPENSSL_CONTEXT_REQUIRES_PRIVATE_KEY callback is called
	 * to allow setting of the private key directly via openSSL
	 * library calls.   (For backwards compatibility, this can also be used
	 * to pass the client cert private key filepath when setting up a
	 * vhost client SSL context, but it is preferred to use
	 * .client_ssl_private_key_filepath for that.)
	 *
	 * Notice you can alternatively set a DER or PEM private key from a
	 * memory buffer as the vhost tls private key using
	 * \p server_ssl_private_key_mem and \p server_ssl_private_key_mem_len.
	 */
	const char *ssl_ca_filepath;
	/**< VHOST: CA certificate filepath or NULL.  (For backwards
	 * compatibility, this can also be used to pass the client CA
	 * filepath when setting up a vhost client SSL context,
	 * but it is preferred to use .client_ssl_ca_filepath for that.)
	 *
	 * Notice you can alternatively set a DER or PEM CA cert from a memory
	 * buffer using \p server_ssl_ca_mem and \p server_ssl_ca_mem_len.
	 */
	const char *ssl_cipher_list;
	/**< VHOST: List of valid ciphers to use ON TLS1.2 AND LOWER ONLY (eg,
	 * "RC4-MD5:RC4-SHA:AES128-SHA:AES256-SHA:HIGH:!DSS:!aNULL"
	 * or you can leave it as NULL to get "DEFAULT" (For backwards
	 * compatibility, this can also be used to pass the client cipher
	 * list when setting up a vhost client SSL context,
	 * but it is preferred to use .client_ssl_cipher_list for that.)
	 * SEE .tls1_3_plus_cipher_list and .client_tls_1_3_plus_cipher_list
	 * for the equivalent for tls1.3.
	 */
	const char *ecdh_curve;
	/**< VHOST: if NULL, defaults to initializing server with
	 *   "prime256v1" */
	const char *tls1_3_plus_cipher_list;
	/**< VHOST: List of valid ciphers to use for incoming server connections
	 * ON TLS1.3 AND ABOVE (eg, "TLS_CHACHA20_POLY1305_SHA256" on this vhost
	 * or you can leave it as NULL to get "DEFAULT".
	 * SEE .client_tls_1_3_plus_cipher_list to do the same on the vhost
	 * client SSL_CTX.
	 */

	const void *server_ssl_cert_mem;
	/**< VHOST: Alternative for \p ssl_cert_filepath that allows setting
	 * from memory instead of from a file.  At most one of
	 * \p ssl_cert_filepath or \p server_ssl_cert_mem should be non-NULL. */
	const void *server_ssl_private_key_mem;
	/**<  VHOST: Alternative for \p ssl_private_key_filepath allowing
	 * init from a private key in memory instead of a file.  At most one
	 * of \p ssl_private_key_filepath or \p server_ssl_private_key_mem
	 * should be non-NULL. */
	const void *server_ssl_ca_mem;
	/**< VHOST: Alternative for \p ssl_ca_filepath allowing
	 * init from a CA cert in memory instead of a file.  At most one
	 * of \p ssl_ca_filepath or \p server_ssl_ca_mem should be non-NULL. */

	long ssl_options_set;
	/**< VHOST: Any bits set here will be set as server SSL options */
	long ssl_options_clear;
	/**< VHOST: Any bits set here will be cleared as server SSL options */
	int simultaneous_ssl_restriction;
	/**< CONTEXT: 0 (no limit) or limit of simultaneous SSL sessions
	 * possible.*/
	int simultaneous_ssl_handshake_restriction;
	/**< CONTEXT: 0 (no limit) or limit of simultaneous SSL handshakes ongoing */
	int ssl_info_event_mask;
	/**< VHOST: mask of ssl events to be reported on LWS_CALLBACK_SSL_INFO
	 * callback for connections on this vhost.  The mask values are of
	 * the form SSL_CB_ALERT, defined in openssl/ssl.h.  The default of
	 * 0 means no info events will be reported.
	 */
	unsigned int server_ssl_cert_mem_len;
	/**< VHOST: Server SSL context init: length of server_ssl_cert_mem in
	 * bytes */
	unsigned int server_ssl_private_key_mem_len;
	/**< VHOST: length of \p server_ssl_private_key_mem in memory */
	unsigned int server_ssl_ca_mem_len;
	/**< VHOST: length of \p server_ssl_ca_mem in memory */

	const char *alpn;
	/**< CONTEXT: If non-NULL, default list of advertised alpn, comma-
	 *	      separated
	 *
	 *     VHOST: If non-NULL, per-vhost list of advertised alpn, comma-
	 *	      separated
	 */


#if defined(LWS_WITH_CLIENT)
	const char *client_ssl_private_key_password;
	/**< VHOST: Client SSL context init: NULL or the passphrase needed
	 * for the private key */
	const char *client_ssl_cert_filepath;
	/**< VHOST: Client SSL context init: The certificate the client
	 * should present to the peer on connection */
	const void *client_ssl_cert_mem;
	/**< VHOST: Client SSL context init: client certificate memory buffer or
	 * NULL... use this to load client cert from memory instead of file */
	unsigned int client_ssl_cert_mem_len;
	/**< VHOST: Client SSL context init: length of client_ssl_cert_mem in
	 * bytes */
	const char *client_ssl_private_key_filepath;
	/**<  VHOST: Client SSL context init: filepath to client private key
	 * if this is set to NULL but client_ssl_cert_filepath is set, you
	 * can handle the LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS
	 * callback of protocols[0] to allow setting of the private key directly
	 * via tls library calls */
	const void *client_ssl_key_mem;
	/**< VHOST: Client SSL context init: client key memory buffer or
	 * NULL... use this to load client key from memory instead of file */
	const char *client_ssl_ca_filepath;
	/**< VHOST: Client SSL context init: CA certificate filepath or NULL */
	const void *client_ssl_ca_mem;
	/**< VHOST: Client SSL context init: CA certificate memory buffer or
	 * NULL... use this to load CA cert from memory instead of file */

	const char *client_ssl_cipher_list;
	/**< VHOST: Client SSL context init: List of valid ciphers to use (eg,
	* "RC4-MD5:RC4-SHA:AES128-SHA:AES256-SHA:HIGH:!DSS:!aNULL"
	* or you can leave it as NULL to get "DEFAULT" */
	const char *client_tls_1_3_plus_cipher_list;
	/**< VHOST: List of valid ciphers to use for outgoing client connections
	 * ON TLS1.3 AND ABOVE on this vhost (eg,
	 * "TLS_CHACHA20_POLY1305_SHA256") or you can leave it as NULL to get
	 * "DEFAULT".
	 */

	long ssl_client_options_set;
	/**< VHOST: Any bits set here will be set as CLIENT SSL options */
	long ssl_client_options_clear;
	/**< VHOST: Any bits set here will be cleared as CLIENT SSL options */


	unsigned int client_ssl_ca_mem_len;
	/**< VHOST: Client SSL context init: length of client_ssl_ca_mem in
	 * bytes */
	unsigned int client_ssl_key_mem_len;
	/**< VHOST: Client SSL context init: length of client_ssl_key_mem in
	 * bytes */

#endif

#if !defined(LWS_WITH_MBEDTLS)
	SSL_CTX *provided_client_ssl_ctx;
	/**< CONTEXT: If non-null, swap out libwebsockets ssl
	  * implementation for the one provided by provided_ssl_ctx.
	  * Libwebsockets no longer is responsible for freeing the context
	  * if this option is selected. */
#else /* WITH_MBEDTLS */
	const char *mbedtls_client_preload_filepath;
	/**< CONTEXT: If NULL, no effect.  Otherwise it should point to a
	 * filepath where every created client SSL_CTX is preloaded from the
	 * system trust bundle.
	 *
	 * This sets a processwide variable that affects all contexts.
	 *
	 * Requires that the mbedtls provides mbedtls_x509_crt_parse_file(),
	 * else disabled.
	 */
#endif
#endif

	int ka_time;
	/**< CONTEXT: 0 for no TCP keepalive, otherwise apply this keepalive
	 * timeout to all libwebsocket sockets, client or server */
	int ka_probes;
	/**< CONTEXT: if ka_time was nonzero, after the timeout expires how many
	 * times to try to get a response from the peer before giving up
	 * and killing the connection */
	int ka_interval;
	/**< CONTEXT: if ka_time was nonzero, how long to wait before each ka_probes
	 * attempt */
	unsigned int timeout_secs;
	/**< VHOST: various processes involving network roundtrips in the
	 * library are protected from hanging forever by timeouts.  If
	 * nonzero, this member lets you set the timeout used in seconds.
	 * Otherwise a default timeout is used. */
	unsigned int connect_timeout_secs;
	/**< VHOST: client connections have this long to find a working server
	 * from the DNS results, or the whole connection times out.  If zero,
	 * a default timeout is used */
	int bind_iface;
	/**< VHOST: nonzero to strictly bind sockets to the interface name in
	 * .iface (eg, "eth2"), using SO_BIND_TO_DEVICE.
	 *
	 * Requires SO_BINDTODEVICE support from your OS and CAP_NET_RAW
	 * capability.
	 *
	 * Notice that common things like access network interface IP from
	 * your local machine use your lo / loopback interface and will be
	 * disallowed by this.
	 */
	unsigned int timeout_secs_ah_idle;
	/**< VHOST: seconds to allow a client to hold an ah without using it.
	 * 0 defaults to 10s. */
#endif /* WITH_NETWORK */

#if defined(LWS_WITH_TLS_SESSIONS)
	uint32_t			tls_session_timeout;
	/**< VHOST: seconds until timeout/ttl for newly created sessions.
	 * 0 means default timeout (defined per protocol, usually 300s). */
	uint32_t			tls_session_cache_max;
	/**< VHOST: 0 for default limit of 10, or the maximum number of
	 * client tls sessions we are willing to cache */
#endif

	gid_t gid;
	/**< CONTEXT: group id to change to after setting listen socket,
	 *   or -1. See also .username below. */
	uid_t uid;
	/**< CONTEXT: user id to change to after setting listen socket,
	 *   or -1.  See also .groupname below. */
	uint64_t options;
	/**< VHOST + CONTEXT: 0, or LWS_SERVER_OPTION_... bitfields */
	void *user;
	/**< VHOST + CONTEXT: optional user pointer that will be associated
	 * with the context when creating the context (and can be retrieved by
	 * lws_context_user(context), or with the vhost when creating the vhost
	 * (and can be retrieved by lws_vhost_user(vhost)).  You will need to
	 * use LWS_SERVER_OPTION_EXPLICIT_VHOSTS and create the vhost separately
	 * if you care about giving the context and vhost different user pointer
	 * values.
	 */
	unsigned int count_threads;
	/**< CONTEXT: how many contexts to create in an array, 0 = 1 */
	unsigned int fd_limit_per_thread;
	/**< CONTEXT: nonzero means restrict each service thread to this
	 * many fds, 0 means the default which is divide the process fd
	 * limit by the number of threads.
	 *
	 * Note if this is nonzero, and fd_limit_per_thread multiplied by the
	 * number of service threads is less than the process ulimit, then lws
	 * restricts internal lookup table allocation to the smaller size, and
	 * switches to a less efficient lookup scheme.  You should use this to
	 * trade off speed against memory usage if you know the lws context
	 * will only use a handful of fds.
	 *
	 * Bear in mind lws may use some fds internally, for example for the
	 * cancel pipe, so you may need to allow for some extras for normal
	 * operation.
	 */
	const char *vhost_name;
	/**< VHOST: name of vhost, must match external DNS name used to
	 * access the site, like "warmcat.com" as it's used to match
	 * Host: header and / or SNI name for SSL.
	 * CONTEXT: NULL, or the name to associate with the context for
	 * context-specific logging
	 */
#if defined(LWS_WITH_PLUGINS)
	const char * const *plugin_dirs;
	/**< CONTEXT: NULL, or NULL-terminated array of directories to
	 * scan for lws protocol plugins at context creation time */
#endif
	void *external_baggage_free_on_destroy;
	/**< CONTEXT: NULL, or pointer to something externally malloc'd, that
	 * should be freed when the context is destroyed.  This allows you to
	 * automatically sync the freeing action to the context destruction
	 * action, so there is no need for an external free() if the context
	 * succeeded to create.
	 */


	unsigned int pt_serv_buf_size;
	/**< CONTEXT: 0 = default of 4096.  This buffer is used by
	 * various service related features including file serving, it
	 * defines the max chunk of file that can be sent at once.
	 * At the risk of lws having to buffer failed large sends, it
	 * can be increased to, eg, 128KiB to improve throughput. */
#if defined(LWS_WITH_FILE_OPS)
	const struct lws_plat_file_ops *fops;
	/**< CONTEXT: NULL, or pointer to an array of fops structs, terminated
	 * by a sentinel with NULL .open.
	 *
	 * If NULL, lws provides just the platform file operations struct for
	 * backwards compatibility.
	 */
#endif

#if defined(LWS_WITH_SOCKS5)
	const char *socks_proxy_address;
	/**< VHOST: If non-NULL, attempts to proxy via the given address.
	 * If proxy auth is required, use format
	 * "username:password\@server:port" */
	unsigned int socks_proxy_port;
	/**< VHOST: If socks_proxy_address was non-NULL, uses this port
	 * if nonzero, otherwise requires "server:port" in .socks_proxy_address
	 */
#endif

#if defined(LWS_HAVE_SYS_CAPABILITY_H) && defined(LWS_HAVE_LIBCAP)
	cap_value_t caps[4];
	/**< CONTEXT: array holding Linux capabilities you want to
	 * continue to be available to the server after it transitions
	 * to a noprivileged user.  Usually none are needed but for, eg,
	 * .bind_iface, CAP_NET_RAW is required.  This gives you a way
	 * to still have the capability but drop root.
	 */
	char count_caps;
	/**< CONTEXT: count of Linux capabilities in .caps[].  0 means
	 * no capabilities will be inherited from root (the default) */
#endif
	void **foreign_loops;
	/**< CONTEXT: This is ignored if the context is not being started with
	 *		an event loop, ie, .options has a flag like
	 *		LWS_SERVER_OPTION_LIBUV.
	 *
	 *		NULL indicates lws should start its own even loop for
	 *		each service thread, and deal with closing the loops
	 *		when the context is destroyed.
	 *
	 *		Non-NULL means it points to an array of external
	 *		("foreign") event loops that are to be used in turn for
	 *		each service thread.  In the default case of 1 service
	 *		thread, it can just point to one foreign event loop.
	 */
	void (*signal_cb)(void *event_lib_handle, int signum);
	/**< CONTEXT: NULL: default signal handling.  Otherwise this receives
	 *		the signal handler callback.  event_lib_handle is the
	 *		native event library signal handle, eg uv_signal_t *
	 *		for libuv.
	 */
	struct lws_context **pcontext;
	/**< CONTEXT: if non-NULL, at the end of context destroy processing,
	 * the pointer pointed to by pcontext is written with NULL.  You can
	 * use this to let foreign event loops know that lws context destruction
	 * is fully completed.
	 */
	void (*finalize)(struct lws_vhost *vh, void *arg);
	/**< VHOST: NULL, or pointer to function that will be called back
	 *	    when the vhost is just about to be freed.  The arg parameter
	 *	    will be set to whatever finalize_arg is below.
	 */
	void *finalize_arg;
	/**< VHOST: opaque pointer lws ignores but passes to the finalize
	 *	    callback.  If you don't care, leave it NULL.
	 */
	const char *listen_accept_role;
	/**< VHOST: NULL for default, or force accepted incoming connections to
	 * bind to this role.  Uses the role names from their ops struct, eg,
	 * "raw-skt".
	 */
	const char *listen_accept_protocol;
	/**< VHOST: NULL for default, or force accepted incoming connections to
	 * bind to this vhost protocol name.
	 */
	const struct lws_protocols **pprotocols;
	/**< VHOST: NULL: use .protocols, otherwise ignore .protocols and use
	 * this array of pointers to protocols structs.  The end of the array
	 * is marked by a NULL pointer.
	 *
	 * This is preferred over .protocols, because it allows the protocol
	 * struct to be opaquely defined elsewhere, with just a pointer to it
	 * needed to create the context with it.  .protocols requires also
	 * the type of the user data to be known so its size can be given.
	 */

	const char *username; /**< CONTEXT: string username for post-init
	 * permissions.  Like .uid but takes a string username. */
	const char *groupname; /**< CONTEXT: string groupname for post-init
	 * permissions.  Like .gid but takes a string groupname. */
	const char *unix_socket_perms; /**< VHOST: if your vhost is listening
	 * on a unix socket, you can give a "username:groupname" string here
	 * to control the owner:group it's created with.  It's always created
	 * with 0660 mode. */
	const lws_system_ops_t *system_ops;
	/**< CONTEXT: hook up lws_system_ apis to system-specific
	 * implementations */
	const lws_retry_bo_t *retry_and_idle_policy;
	/**< VHOST: optional retry and idle policy to apply to this vhost.
	 *   Currently only the idle parts are applied to the connections.
	 */
#if defined(LWS_WITH_SYS_STATE)
	lws_state_notify_link_t * const *register_notifier_list;
	/**< CONTEXT: NULL, or pointer to an array of notifiers that should
	 * be registered during context creation, so they can see state change
	 * events from very early on.  The array should end with a NULL. */
#endif
#if defined(LWS_WITH_SECURE_STREAMS)
#if defined(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)
	const struct lws_ss_policy *pss_policies; /**< CONTEXT: point to first
	 * in a linked-list of streamtype policies prepared by user code */
#else
	const char *pss_policies_json; /**< CONTEXT: point to a string
	 * containing a JSON description of the secure streams policies.  Set
	 * to NULL if not using Secure Streams.
	 * If the platform supports files and the string does not begin with
	 * '{', lws treats the string as a filepath to open to get the JSON
	 * policy.
	 */
#endif
	const struct lws_ss_plugin **pss_plugins; /**< CONTEXT: point to an array
	 * of pointers to plugin structs here, terminated with a NULL ptr.
	 * Set to NULL if not using Secure Streams. */
	const char *ss_proxy_bind; /**< CONTEXT: NULL, or: ss_proxy_port == 0:
	 * point to a string giving the Unix Domain Socket address to use (start
	 * with @ for abstract namespace), ss_proxy_port nonzero: set the
	 * network interface address (not name, it's ambiguous for ipv4/6) to
	 * bind the tcp connection to the proxy to */
	const char *ss_proxy_address; /**< CONTEXT: NULL, or if ss_proxy_port
	 * nonzero: the tcp address of the ss proxy to connect to */
	uint16_t ss_proxy_port; /* 0 = if connecting to ss proxy, do it via a
	 * Unix Domain Socket, "+@proxy.ss.lws" if ss_proxy_bind is NULL else
	 * the socket path given in ss_proxy_bind (start it with a + or +@);
	 * nonzero means connect via a tcp socket to the tcp address in
	 * ss_proxy_bind and the given port */
#endif

	int rlimit_nofile;
	/**< 0 = inherit the initial ulimit for files / sockets from the startup
	 * environment.  Nonzero = try to set the limit for this process.
	 */
#if defined(LWS_WITH_PEER_LIMITS)
	lws_peer_limits_notify_t pl_notify_cb;
	/**< CONTEXT: NULL, or a callback to receive notifications each time a
	 * connection is being dropped because of peer limits.
	 *
	 * The callback provides the context, and an lws_sockaddr46 with the
	 * peer address and port.
	 */
	unsigned short ip_limit_ah;
	/**< CONTEXT: max number of ah a single IP may use simultaneously
	 *	      0 is no limit. This is a soft limit: if the limit is
	 *	      reached, connections from that IP will wait in the ah
	 *	      waiting list and not be able to acquire an ah until
	 *	      a connection belonging to the IP relinquishes one it
	 *	      already has.
	 */
	unsigned short ip_limit_wsi;
	/**< CONTEXT: max number of wsi a single IP may use simultaneously.
	 *	      0 is no limit.  This is a hard limit, connections from
	 *	      the same IP will simply be dropped once it acquires the
	 *	      amount of simultaneous wsi / accepted connections
	 *	      given here.
	 */

#endif /* PEER_LIMITS */

#if defined(LWS_WITH_SYS_FAULT_INJECTION)
	lws_fi_ctx_t				fic;
	/**< CONTEXT | VHOST: attach external Fault Injection context to the
	 * lws_context or vhost.  If creating the context + default vhost in
	 * one step, only the context binds to \p fi.  When creating a vhost
	 * otherwise this can bind to the vhost so the faults can be injected
	 * from the start.
	 */
#endif

#if defined(LWS_WITH_SYS_SMD)
	lws_smd_notification_cb_t		early_smd_cb;
	/**< CONTEXT: NULL, or an smd notification callback that will be registered
	 * immediately after the smd in the context is initialized.  This ensures
	 * you can get all notifications without having to intercept the event loop
	 * creation, eg, when using an event library.  Other callbacks can be
	 * registered later manually without problems.
	 */
	void					*early_smd_opaque;
	lws_smd_class_t				early_smd_class_filter;
	lws_usec_t				smd_ttl_us;
	/**< CONTEXT: SMD messages older than this many us are removed from the
	 * queue and destroyed even if not fully delivered yet.  If zero,
	 * defaults to 2 seconds (5 second for FREERTOS).
	 */
	uint16_t				smd_queue_depth;
	/**< CONTEXT: Maximum queue depth, If zero defaults to 40
	 * (20 for FREERTOS) */
#endif

#if defined(LWS_WITH_SYS_METRICS)
	const struct lws_metric_policy		*metrics_policies;
	/**< CONTEXT: non-SS policy metrics policies */
	const char				*metrics_prefix;
	/**< CONTEXT: prefix for this context's metrics, used to distinguish
	 * metrics pooled from different processes / applications, so, eg what
	 * would be "cpu.svc" if this is NULL becomes "myapp.cpu.svc" is this is
	 * set to "myapp".  Policies are applied using the name with the prefix,
	 * if present.
	 */
#endif

	int					fo_listen_queue;
	/**< VHOST: 0 = no TCP_FASTOPEN, nonzero = enable TCP_FASTOPEN if the
	 * platform supports it, with the given queue length for the listen
	 * socket.
	 */

	const struct lws_plugin_evlib		*event_lib_custom;
	/**< CONTEXT: If non-NULL, override event library selection so it uses
	 * this custom event library implementation, instead of default internal
	 * loop.  Don't set any other event lib context creation flags in that
	 * case. it will be used automatically.  This is useful for integration
	 * where an existing application is using its own handrolled event loop
	 * instead of an event library, it provides a way to allow lws to use
	 * the custom event loop natively as if it were an "event library".
	 */

#if defined(LWS_WITH_TLS_JIT_TRUST)
	size_t					jitt_cache_max_footprint;
	/**< CONTEXT: 0 for no limit, else max bytes used by JIT Trust cache...
	 * LRU items are evicted to keep under this limit */
	int					vh_idle_grace_ms;
	/**< CONTEXT: 0 for default of 5000ms, or number of ms JIT Trust vhosts
	 * are allowed to live without active connections using them. */
#endif

	lws_log_cx_t				*log_cx;
	/**< CONTEXT: NULL to use the default, process-scope logging context,
	 * else a specific logging context to associate with this context */

#if defined(LWS_WITH_CACHE_NSCOOKIEJAR) && defined(LWS_WITH_CLIENT)
	const char				*http_nsc_filepath;
	/**< CONTEXT: Filepath to use for http netscape cookiejar file */

	size_t					http_nsc_heap_max_footprint;
	/**< CONTEXT: 0, or limit in bytes for heap usage of memory cookie
	 * cache */
	size_t					http_nsc_heap_max_items;
	/**< CONTEXT: 0, or the max number of items allowed in the cookie cache
	 * before destroying lru items to keep it under the limit */
	size_t					http_nsc_heap_max_payload;
	/**< CONTEXT: 0, or the maximum size of a single cookie we are able to
	 * handle */
#endif

	/* Add new things just above here ---^
	 * This is part of the ABI, don't needlessly break compatibility
	 *
	 * The below is to ensure later library versions with new
	 * members added above will see 0 (default) even if the app
	 * was not built against the newer headers.
	 */

	void *_unused[2]; /**< dummy */
};

/**
 * lws_create_context() - Create the websocket handler
 * \param info:	pointer to struct with parameters
 *
 *	This function creates the listening socket (if serving) and takes care
 *	of all initialization in one step.
 *
 *	If option LWS_SERVER_OPTION_EXPLICIT_VHOSTS is given, no vhost is
 *	created; you're expected to create your own vhosts afterwards using
 *	lws_create_vhost().  Otherwise a vhost named "default" is also created
 *	using the information in the vhost-related members, for compatibility.
 *
 *	After initialization, it returns a struct lws_context * that
 *	represents this server.  After calling, user code needs to take care
 *	of calling lws_service() with the context pointer to get the
 *	server's sockets serviced.  This must be done in the same process
 *	context as the initialization call.
 *
 *	The protocol callback functions are called for a handful of events
 *	including http requests coming in, websocket connections becoming
 *	established, and data arriving; it's also called periodically to allow
 *	async transmission.
 *
 *	HTTP requests are sent always to the FIRST protocol in protocol, since
 *	at that time websocket protocol has not been negotiated.  Other
 *	protocols after the first one never see any HTTP callback activity.
 *
 *	The server created is a simple http server by default; part of the
 *	websocket standard is upgrading this http connection to a websocket one.
 *
 *	This allows the same server to provide files like scripts and favicon /
 *	images or whatever over http and dynamic data over websockets all in
 *	one place; they're all handled in the user callback.
 */
LWS_VISIBLE LWS_EXTERN struct lws_context *
lws_create_context(const struct lws_context_creation_info *info);


/**
 * lws_context_destroy() - Destroy the websocket context
 * \param context:	Websocket context
 *
 *	This function closes any active connections and then frees the
 *	context.  After calling this, any further use of the context is
 *	undefined.
 */
LWS_VISIBLE LWS_EXTERN void
lws_context_destroy(struct lws_context *context);

typedef int (*lws_reload_func)(void);

/**
 * lws_context_deprecate() - Deprecate the websocket context
 *
 * \param context:	Websocket context
 * \param cb: Callback notified when old context listen sockets are closed
 *
 *	This function is used on an existing context before superceding it
 *	with a new context.
 *
 *	It closes any listen sockets in the context, so new connections are
 *	not possible.
 *
 *	And it marks the context to be deleted when the number of active
 *	connections into it falls to zero.
 *
 *	This is aimed at allowing seamless configuration reloads.
 *
 *	The callback cb will be called after the listen sockets are actually
 *	closed and may be reopened.  In the callback the new context should be
 *	configured and created.  (With libuv, socket close happens async after
 *	more loop events).
 */
LWS_VISIBLE LWS_EXTERN void
lws_context_deprecate(struct lws_context *context, lws_reload_func cb);

LWS_VISIBLE LWS_EXTERN int
lws_context_is_deprecated(struct lws_context *context);

/**
 * lws_set_proxy() - Setups proxy to lws_context.
 * \param vhost:	pointer to struct lws_vhost you want set proxy for
 * \param proxy: pointer to c string containing proxy in format address:port
 *
 * Returns 0 if proxy string was parsed and proxy was setup.
 * Returns -1 if proxy is NULL or has incorrect format.
 *
 * This is only required if your OS does not provide the http_proxy
 * environment variable (eg, OSX)
 *
 *   IMPORTANT! You should call this function right after creation of the
 *   lws_context and before call to connect. If you call this
 *   function after connect behavior is undefined.
 *   This function will override proxy settings made on lws_context
 *   creation with genenv() call.
 */
LWS_VISIBLE LWS_EXTERN int
lws_set_proxy(struct lws_vhost *vhost, const char *proxy);

/**
 * lws_set_socks() - Setup socks to lws_context.
 * \param vhost:	pointer to struct lws_vhost you want set socks for
 * \param socks: pointer to c string containing socks in format address:port
 *
 * Returns 0 if socks string was parsed and socks was setup.
 * Returns -1 if socks is NULL or has incorrect format.
 *
 * This is only required if your OS does not provide the socks_proxy
 * environment variable (eg, OSX)
 *
 *   IMPORTANT! You should call this function right after creation of the
 *   lws_context and before call to connect. If you call this
 *   function after connect behavior is undefined.
 *   This function will override proxy settings made on lws_context
 *   creation with genenv() call.
 */
LWS_VISIBLE LWS_EXTERN int
lws_set_socks(struct lws_vhost *vhost, const char *socks);

struct lws_vhost;

/**
 * lws_create_vhost() - Create a vhost (virtual server context)
 * \param context:	pointer to result of lws_create_context()
 * \param info:		pointer to struct with parameters
 *
 * This function creates a virtual server (vhost) using the vhost-related
 * members of the info struct.  You can create many vhosts inside one context
 * if you created the context with the option LWS_SERVER_OPTION_EXPLICIT_VHOSTS
 */
LWS_VISIBLE LWS_EXTERN struct lws_vhost *
lws_create_vhost(struct lws_context *context,
		 const struct lws_context_creation_info *info);

/**
 * lws_vhost_destroy() - Destroy a vhost (virtual server context)
 *
 * \param vh:		pointer to result of lws_create_vhost()
 *
 * This function destroys a vhost.  Normally, if you just want to exit,
 * then lws_destroy_context() will take care of everything.  If you want
 * to destroy an individual vhost and all connections and allocations, you
 * can do it with this.
 *
 * If the vhost has a listen sockets shared by other vhosts, it will be given
 * to one of the vhosts sharing it rather than closed.
 *
 * The vhost close is staged according to the needs of the event loop, and if
 * there are multiple service threads.  At the point the vhost itself if
 * about to be freed, if you provided a finalize callback and optional arg at
 * vhost creation time, it will be called just before the vhost is freed.
 */
LWS_VISIBLE LWS_EXTERN void
lws_vhost_destroy(struct lws_vhost *vh);

/**
 * lwsws_get_config_globals() - Parse a JSON server config file
 * \param info:		pointer to struct with parameters
 * \param d:		filepath of the config file
 * \param config_strings: storage for the config strings extracted from JSON,
 * 			  the pointer is incremented as strings are stored
 * \param len:		pointer to the remaining length left in config_strings
 *			  the value is decremented as strings are stored
 *
 * This function prepares a n lws_context_creation_info struct with global
 * settings from a file d.
 *
 * Requires CMake option LWS_WITH_LEJP_CONF to have been enabled
 */
LWS_VISIBLE LWS_EXTERN int
lwsws_get_config_globals(struct lws_context_creation_info *info, const char *d,
			 char **config_strings, int *len);

/**
 * lwsws_get_config_vhosts() - Create vhosts from a JSON server config file
 * \param context:	pointer to result of lws_create_context()
 * \param info:		pointer to struct with parameters
 * \param d:		filepath of the config file
 * \param config_strings: storage for the config strings extracted from JSON,
 * 			  the pointer is incremented as strings are stored
 * \param len:		pointer to the remaining length left in config_strings
 *			  the value is decremented as strings are stored
 *
 * This function creates vhosts into a context according to the settings in
 *JSON files found in directory d.
 *
 * Requires CMake option LWS_WITH_LEJP_CONF to have been enabled
 */
LWS_VISIBLE LWS_EXTERN int
lwsws_get_config_vhosts(struct lws_context *context,
			struct lws_context_creation_info *info, const char *d,
			char **config_strings, int *len);

/**
 * lws_get_vhost() - return the vhost a wsi belongs to
 *
 * \param wsi: which connection
 */
LWS_VISIBLE LWS_EXTERN struct lws_vhost *
lws_get_vhost(struct lws *wsi);

/**
 * lws_get_vhost_name() - returns the name of a vhost
 *
 * \param vhost: which vhost
 */
LWS_VISIBLE LWS_EXTERN const char *
lws_get_vhost_name(struct lws_vhost *vhost);

/**
 * lws_get_vhost_by_name() - returns the vhost with the requested name, or NULL
 *
 * \param context: the lws_context to look in
 * \param name: vhost name we are looking for
 *
 * Returns NULL, or the vhost with the name \p name
 */
LWS_VISIBLE LWS_EXTERN struct lws_vhost *
lws_get_vhost_by_name(struct lws_context *context, const char *name);

/**
 * lws_get_vhost_port() - returns the port a vhost listens on, or -1
 *
 * \param vhost: which vhost
 */
LWS_VISIBLE LWS_EXTERN int
lws_get_vhost_port(struct lws_vhost *vhost);

/**
 * lws_get_vhost_user() - returns the user pointer for the vhost
 *
 * \param vhost: which vhost
 */
LWS_VISIBLE LWS_EXTERN void *
lws_get_vhost_user(struct lws_vhost *vhost);

/**
 * lws_get_vhost_iface() - returns the binding for the vhost listen socket
 *
 * \param vhost: which vhost
 */
LWS_VISIBLE LWS_EXTERN const char *
lws_get_vhost_iface(struct lws_vhost *vhost);

/**
 * lws_json_dump_vhost() - describe vhost state and stats in JSON
 *
 * \param vh: the vhost
 * \param buf: buffer to fill with JSON
 * \param len: max length of buf
 */
LWS_VISIBLE LWS_EXTERN int
lws_json_dump_vhost(const struct lws_vhost *vh, char *buf, int len);

/**
 * lws_json_dump_context() - describe context state and stats in JSON
 *
 * \param context: the context
 * \param buf: buffer to fill with JSON
 * \param len: max length of buf
 * \param hide_vhosts: nonzero to not provide per-vhost mount etc information
 *
 * Generates a JSON description of vhost state into buf
 */
LWS_VISIBLE LWS_EXTERN int
lws_json_dump_context(const struct lws_context *context, char *buf, int len,
		      int hide_vhosts);

/**
 * lws_vhost_user() - get the user data associated with the vhost
 * \param vhost: Websocket vhost
 *
 * This returns the optional user pointer that can be attached to
 * a vhost when it was created.  Lws never dereferences this pointer, it only
 * sets it when the vhost is created, and returns it using this api.
 */
LWS_VISIBLE LWS_EXTERN void *
lws_vhost_user(struct lws_vhost *vhost);

/**
 * lws_context_user() - get the user data associated with the context
 * \param context: Websocket context
 *
 * This returns the optional user allocation that can be attached to
 * the context the sockets live in at context_create time.  It's a way
 * to let all sockets serviced in the same context share data without
 * using globals statics in the user code.
 */
LWS_VISIBLE LWS_EXTERN void *
lws_context_user(struct lws_context *context);

LWS_VISIBLE LWS_EXTERN const char *
lws_vh_tag(struct lws_vhost *vh);

/**
 * lws_context_is_being_destroyed() - find out if context is being destroyed
 *
 * \param context: the struct lws_context pointer
 *
 * Returns nonzero if the context has had lws_context_destroy() called on it...
 * when using event library loops the destroy process can be asynchronous.  In
 * the special case of libuv foreign loops, the failure to create the context
 * may have to do work on the foreign loop to reverse the partial creation,
 * meaning a failed context create cannot unpick what it did and return NULL.
 *
 * In that condition, a valid context that is already started the destroy
 * process is returned, and this test api will return nonzero as a way to
 * find out the create is in the middle of failing.
 */
LWS_VISIBLE LWS_EXTERN int
lws_context_is_being_destroyed(struct lws_context *context);

/*! \defgroup vhost-mounts Vhost mounts and options
 * \ingroup context-and-vhost-creation
 *
 * ##Vhost mounts and options
 */
///@{
/** struct lws_protocol_vhost_options - linked list of per-vhost protocol
 * 					name=value options
 *
 * This provides a general way to attach a linked-list of name=value pairs,
 * which can also have an optional child link-list using the options member.
 */
struct lws_protocol_vhost_options {
	const struct lws_protocol_vhost_options *next; /**< linked list */
	const struct lws_protocol_vhost_options *options; /**< child linked-list of more options for this node */
	const char *name; /**< name of name=value pair */
	const char *value; /**< value of name=value pair */
};

/** enum lws_mount_protocols
 * This specifies the mount protocol for a mountpoint, whether it is to be
 * served from a filesystem, or it is a cgi etc.
 */
enum lws_mount_protocols {
	LWSMPRO_HTTP		= 0, /**< http reverse proxy */
	LWSMPRO_HTTPS		= 1, /**< https reverse proxy */
	LWSMPRO_FILE		= 2, /**< serve from filesystem directory */
	LWSMPRO_CGI		= 3, /**< pass to CGI to handle */
	LWSMPRO_REDIR_HTTP	= 4, /**< redirect to http:// url */
	LWSMPRO_REDIR_HTTPS	= 5, /**< redirect to https:// url */
	LWSMPRO_CALLBACK	= 6, /**< hand by named protocol's callback */
};

/** enum lws_authentication_mode
 * This specifies the authentication mode of the mount. The basic_auth_login_file mount parameter
 * is ignored unless LWSAUTHM_DEFAULT is set.
 */
enum lws_authentication_mode {
	LWSAUTHM_DEFAULT = 0, /**< default authenticate only if basic_auth_login_file is provided */
	LWSAUTHM_BASIC_AUTH_CALLBACK = 1 << 28 /**< Basic auth with a custom verifier */
};

/** The authentication mode is stored in the top 4 bits of lws_http_mount.auth_mask */
#define AUTH_MODE_MASK 0xF0000000

/** struct lws_http_mount
 *
 * arguments for mounting something in a vhost's url namespace
 */
struct lws_http_mount {
	const struct lws_http_mount *mount_next;
	/**< pointer to next struct lws_http_mount */
	const char *mountpoint;
	/**< mountpoint in http pathspace, eg, "/" */
	const char *origin;
	/**< path to be mounted, eg, "/var/www/warmcat.com" */
	const char *def;
	/**< default target, eg, "index.html" */
	const char *protocol;
	/**<"protocol-name" to handle mount */

	const struct lws_protocol_vhost_options *cgienv;
	/**< optional linked-list of cgi options.  These are created
	 * as environment variables for the cgi process
	 */
	const struct lws_protocol_vhost_options *extra_mimetypes;
	/**< optional linked-list of mimetype mappings */
	const struct lws_protocol_vhost_options *interpret;
	/**< optional linked-list of files to be interpreted */

	int cgi_timeout;
	/**< seconds cgi is allowed to live, if cgi://mount type */
	int cache_max_age;
	/**< max-age for reuse of client cache of files, seconds */
	unsigned int auth_mask;
	/**< bits set here must be set for authorized client session */

	unsigned int cache_reusable:1; /**< set if client cache may reuse this */
	unsigned int cache_revalidate:1; /**< set if client cache should revalidate on use */
	unsigned int cache_intermediaries:1; /**< set if intermediaries are allowed to cache */

	unsigned char origin_protocol; /**< one of enum lws_mount_protocols */
	unsigned char mountpoint_len; /**< length of mountpoint string */

	const char *basic_auth_login_file;
	/**<NULL, or filepath to use to check basic auth logins against. (requires LWSAUTHM_DEFAULT) */

	/* Add new things just above here ---^
	 * This is part of the ABI, don't needlessly break compatibility
	 */
};

///@}
///@}
