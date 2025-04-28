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

/** \defgroup sock-adopt Socket adoption helpers
 * ##Socket adoption helpers
 *
 * When integrating with an external app with its own event loop, these can
 * be used to accept connections from someone else's listening socket.
 *
 * When using lws own event loop, these are not needed.
 */
///@{

/**
 * lws_adopt_socket() - adopt foreign socket as if listen socket accepted it
 * for the default vhost of context.
 *
 * \param context: lws context
 * \param accept_fd: fd of already-accepted socket to adopt
 *
 * Either returns new wsi bound to accept_fd, or closes accept_fd and
 * returns NULL, having cleaned up any new wsi pieces.
 *
 * LWS adopts the socket in http serving mode, it's ready to accept an upgrade
 * to ws or just serve http.
 */
LWS_VISIBLE LWS_EXTERN struct lws *
lws_adopt_socket(struct lws_context *context, lws_sockfd_type accept_fd);
/**
 * lws_adopt_socket_vhost() - adopt foreign socket as if listen socket accepted
 * it for vhost
 *
 * \param vh: lws vhost
 * \param accept_fd: fd of already-accepted socket to adopt
 *
 * Either returns new wsi bound to accept_fd, or closes accept_fd and
 * returns NULL, having cleaned up any new wsi pieces.
 *
 * LWS adopts the socket in http serving mode, it's ready to accept an upgrade
 * to ws or just serve http.
 */
LWS_VISIBLE LWS_EXTERN struct lws *
lws_adopt_socket_vhost(struct lws_vhost *vh, lws_sockfd_type accept_fd);

typedef enum {
	LWS_ADOPT_RAW_FILE_DESC		=  0,	/* convenience constant */
	LWS_ADOPT_HTTP			=  1,	/* flag: absent implies RAW */
	LWS_ADOPT_SOCKET		=  2,	/* flag: absent implies file */
	LWS_ADOPT_ALLOW_SSL		=  4,	/* flag: use tls */
	LWS_ADOPT_FLAG_UDP		= 16,	/* flag: socket is UDP */
	LWS_ADOPT_FLAG_RAW_PROXY	= 32,	/* flag: raw proxy */

	LWS_ADOPT_RAW_SOCKET_UDP = LWS_ADOPT_SOCKET | LWS_ADOPT_FLAG_UDP,
} lws_adoption_type;

typedef union {
	lws_sockfd_type sockfd;
	lws_filefd_type filefd;
} lws_sock_file_fd_type;

#if defined(LWS_ESP_PLATFORM)
#include <lwip/sockets.h>
#endif

typedef union {
#if defined(LWS_WITH_IPV6)
	struct sockaddr_in6 sa6;
#else
#if defined(LWS_ESP_PLATFORM)
	uint8_t _pad_sa6[28];
#endif
#endif
	struct sockaddr_in sa4;
} lws_sockaddr46;

#define sa46_sockaddr(_sa46) ((struct sockaddr *)(_sa46))

#if defined(LWS_WITH_IPV6)
#define sa46_socklen(_sa46) (socklen_t)((_sa46)->sa4.sin_family == AF_INET ? \
				sizeof(struct sockaddr_in) : \
				sizeof(struct sockaddr_in6))
#define sa46_sockport(_sa46, _sp)  { if ((_sa46)->sa4.sin_family == AF_INET) \
					(_sa46)->sa4.sin_port = (_sp); else \
					(_sa46)->sa6.sin6_port = (_sp); }
#define sa46_address(_sa46) ((uint8_t *)((_sa46)->sa4.sin_family == AF_INET ? \
		     &_sa46->sa4.sin_addr : &_sa46->sa6.sin6_addr ))
#else
#define sa46_socklen(_sa46) (socklen_t)sizeof(struct sockaddr_in)
#define sa46_sockport(_sa46, _sp)  (_sa46)->sa4.sin_port = (_sp)
#define sa46_address(_sa46) (uint8_t *)&_sa46->sa4.sin_addr
#endif

#define sa46_address_len(_sa46) ((_sa46)->sa4.sin_family == AF_INET ? 4 : 16)

#if defined(LWS_WITH_UDP)
struct lws_udp {
	lws_sockaddr46		sa46;
	lws_sockaddr46		sa46_pending;
	uint8_t			connected:1;
};
#endif

/**
* lws_adopt_descriptor_vhost() - adopt foreign socket or file descriptor
* if socket descriptor, should already have been accepted from listen socket
*
* \param vh: lws vhost
* \param type: OR-ed combinations of lws_adoption_type flags
* \param fd: union with either .sockfd or .filefd set
* \param vh_prot_name: NULL or vh protocol name to bind raw connection to
* \param parent: NULL or struct lws to attach new_wsi to as a child
*
* Either returns new wsi bound to accept_fd, or closes accept_fd and
* returns NULL, having cleaned up any new wsi pieces.
*
* If LWS_ADOPT_SOCKET is set, LWS adopts the socket in http serving mode, it's
* ready to accept an upgrade to ws or just serve http.
*
* parent may be NULL, if given it should be an existing wsi that will become the
* parent of the new wsi created by this call.
*/
LWS_VISIBLE LWS_EXTERN struct lws *
lws_adopt_descriptor_vhost(struct lws_vhost *vh, lws_adoption_type type,
			   lws_sock_file_fd_type fd, const char *vh_prot_name,
			   struct lws *parent);

typedef struct lws_adopt_desc {
	struct lws_vhost *vh;		/**< vhost the wsi should belong to */
	lws_adoption_type type;		/**< OR-ed combinations of lws_adoption_type flags */
	lws_sock_file_fd_type fd;	/**< union with either .sockfd or .filefd set */
	const char *vh_prot_name;	/**< NULL or vh protocol name to bind raw connection to */
	struct lws *parent;		/**< NULL or struct lws to attach new_wsi to as a child */
	void *opaque;			/**< opaque pointer to set on created wsi */
	const char *fi_wsi_name;	/**< NULL, or Fault Injection inheritence filter for wsi=string/ context faults */
} lws_adopt_desc_t;

/**
* lws_adopt_descriptor_vhost_via_info() - adopt foreign socket or file descriptor
* if socket descriptor, should already have been accepted from listen socket
*
* \param info: the struct containing the parameters
*
*  - vh: lws vhost
*  - type: OR-ed combinations of lws_adoption_type flags
*  - fd: union with either .sockfd or .filefd set
*  - vh_prot_name: NULL or vh protocol name to bind raw connection to
*  - parent: NULL or struct lws to attach new_wsi to as a child
*  - opaque: opaque pointer to set on created wsi
*
* Either returns new wsi bound to accept_fd, or closes accept_fd and
* returns NULL, having cleaned up any new wsi pieces.
*
* If LWS_ADOPT_SOCKET is set, LWS adopts the socket in http serving mode, it's
* ready to accept an upgrade to ws or just serve http.
*
* parent may be NULL, if given it should be an existing wsi that will become the
* parent of the new wsi created by this call.
*/
LWS_VISIBLE LWS_EXTERN struct lws *
lws_adopt_descriptor_vhost_via_info(const lws_adopt_desc_t *info);

/**
 * lws_adopt_socket_readbuf() - adopt foreign socket and first rx as if listen socket accepted it
 * for the default vhost of context.
 * \param context:	lws context
 * \param accept_fd:	fd of already-accepted socket to adopt
 * \param readbuf:	NULL or pointer to data that must be drained before reading from
 *		accept_fd
 * \param len:	The length of the data held at \p readbuf
 *
 * Either returns new wsi bound to accept_fd, or closes accept_fd and
 * returns NULL, having cleaned up any new wsi pieces.
 *
 * LWS adopts the socket in http serving mode, it's ready to accept an upgrade
 * to ws or just serve http.
 *
 * If your external code did not already read from the socket, you can use
 * lws_adopt_socket() instead.
 *
 * This api is guaranteed to use the data at \p readbuf first, before reading from
 * the socket.
 *
 * \p readbuf is limited to the size of the ah rx buf, currently 2048 bytes.
 */
LWS_VISIBLE LWS_EXTERN struct lws *
lws_adopt_socket_readbuf(struct lws_context *context, lws_sockfd_type accept_fd,
                         const char *readbuf, size_t len);
/**
 * lws_adopt_socket_vhost_readbuf() - adopt foreign socket and first rx as if listen socket
 * accepted it for vhost.
 * \param vhost:	lws vhost
 * \param accept_fd:	fd of already-accepted socket to adopt
 * \param readbuf:	NULL or pointer to data that must be drained before reading from accept_fd
 * \param len:		The length of the data held at \p readbuf
 *
 * Either returns new wsi bound to accept_fd, or closes accept_fd and
 * returns NULL, having cleaned up any new wsi pieces.
 *
 * LWS adopts the socket in http serving mode, it's ready to accept an upgrade
 * to ws or just serve http.
 *
 * If your external code did not already read from the socket, you can use
 * lws_adopt_socket() instead.
 *
 * This api is guaranteed to use the data at \p readbuf first, before reading from
 * the socket.
 *
 * \p readbuf is limited to the size of the ah rx buf, currently 2048 bytes.
 */
LWS_VISIBLE LWS_EXTERN struct lws *
lws_adopt_socket_vhost_readbuf(struct lws_vhost *vhost,
			       lws_sockfd_type accept_fd, const char *readbuf,
			       size_t len);

#define LWS_CAUDP_BIND (1 << 0)
#define LWS_CAUDP_BROADCAST (1 << 1)
#define LWS_CAUDP_PF_PACKET (1 << 2)

#if defined(LWS_WITH_UDP)
/**
 * lws_create_adopt_udp() - create, bind and adopt a UDP socket
 *
 * \param vhost:	 lws vhost
 * \param ads:		 NULL or address to do dns lookup on
 * \param port:		 UDP port to bind to, -1 means unbound
 * \param flags:	 0 or LWS_CAUDP_NO_BIND
 * \param protocol_name: Name of protocol on vhost to bind wsi to
 * \param ifname:	 NULL, for network interface name to bind socket to
 * \param parent_wsi:	 NULL or parent wsi new wsi will be a child of
 * \param opaque:	 set created wsi opaque ptr to this
 * \param retry_policy:	 NULL for vhost default policy else wsi specific policy
 * \param fi_wsi_name:	 NULL, or string to inherit Fault Injection rules in
 *			 form "wsi=string/rule".  "wsi/rule" faults will be
 *			 automatically applied as well.  It's done at creation
 *			 time so the rules can, eg, inject faults related to
 *			 creation.
 *
 * Either returns new wsi bound to accept_fd, or closes accept_fd and
 * returns NULL, having cleaned up any new wsi pieces.
 * */
LWS_VISIBLE LWS_EXTERN struct lws *
lws_create_adopt_udp(struct lws_vhost *vhost, const char *ads, int port,
		     int flags, const char *protocol_name, const char *ifname,
		     struct lws *parent_wsi, void *opaque,
		     const lws_retry_bo_t *retry_policy, const char *fi_wsi_name);
#endif



///@}
