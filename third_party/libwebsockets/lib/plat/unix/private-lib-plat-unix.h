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
 *
 *  Included from lib/private-lib-core.h if no explicit platform
 */

#include <fcntl.h>
#include <strings.h>
#include <unistd.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <netdb.h>

#ifndef __cplusplus
#include <errno.h>
#endif
#include <netdb.h>
#include <signal.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/un.h>
#if defined(LWS_HAVE_EVENTFD)
#include <sys/eventfd.h>
#endif

#if defined(__APPLE__)
#include <machine/endian.h>
#endif
#if defined(__FreeBSD__)
#include <sys/endian.h>
#endif
#if defined(__linux__)
#include <endian.h>
#include <linux/if_packet.h>
#include <net/if.h>
#endif
#if defined(__QNX__)
	#include <gulliver.h>
	#if defined(__LITTLEENDIAN__)
		#define BYTE_ORDER __LITTLEENDIAN__
		#define LITTLE_ENDIAN __LITTLEENDIAN__
		#define BIG_ENDIAN 4321  /* to show byte order (taken from gcc); for suppres warning that BIG_ENDIAN is not defined. */
	#endif
	#if defined(__BIGENDIAN__)
		#define BYTE_ORDER __BIGENDIAN__
		#define LITTLE_ENDIAN 1234  /* to show byte order (taken from gcc); for suppres warning that LITTLE_ENDIAN is not defined. */
		#define BIG_ENDIAN __BIGENDIAN__
	#endif
#endif

#if defined(LWS_HAVE_PTHREAD_H)
#include <pthread.h>
typedef pthread_mutex_t lws_mutex_t;
#define lws_mutex_init(x)	pthread_mutex_init(&(x), NULL)
#define lws_mutex_destroy(x)	pthread_mutex_destroy(&(x))
#define lws_mutex_lock(x)	pthread_mutex_lock(&(x))
#define lws_mutex_unlock(x)	pthread_mutex_unlock(&(x))
#endif

#if defined(__sun) && defined(__GNUC__)

#include <arpa/nameser_compat.h>

#if !defined (BYTE_ORDER)
#define BYTE_ORDER __BYTE_ORDER__
#endif

#if !defined(LITTLE_ENDIAN)
#define LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#endif

#if !defined(BIG_ENDIAN)
#define BIG_ENDIAN __ORDER_BIG_ENDIAN__
#endif

#endif /* sun + GNUC */

#if !defined(BYTE_ORDER)
#define BYTE_ORDER __BYTE_ORDER
#endif
#if !defined(LITTLE_ENDIAN)
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#endif
#if !defined(BIG_ENDIAN)
#define BIG_ENDIAN __BIG_ENDIAN
#endif

#if defined(LWS_BUILTIN_GETIFADDRS)
#include "./misc/getifaddrs.h"
#else

#if defined(__HAIKU__)
#define _BSD_SOURCE
#endif
#include <ifaddrs.h>

#endif

#if defined (__sun) || defined(__HAIKU__) || defined(__QNX__) || defined(__ANDROID__)
#include <syslog.h>

#if defined(__ANDROID__)
#include <sys/resource.h>
#endif

#else
#include <sys/syslog.h>
#endif

#ifdef __QNX__
# include "netinet/tcp_var.h"
# define TCP_KEEPINTVL TCPCTL_KEEPINTVL
# define TCP_KEEPIDLE  TCPCTL_KEEPIDLE
# define TCP_KEEPCNT   TCPCTL_KEEPCNT
#endif

#define LWS_ERRNO errno
#define LWS_EAGAIN EAGAIN
#define LWS_EALREADY EALREADY
#define LWS_EINPROGRESS EINPROGRESS
#define LWS_EINTR EINTR
#define LWS_EISCONN EISCONN
#define LWS_ENOTCONN ENOTCONN
#define LWS_EWOULDBLOCK EWOULDBLOCK
#define LWS_EADDRINUSE EADDRINUSE
#define lws_set_blocking_send(wsi)
#define LWS_SOCK_INVALID (-1)

struct lws_context;

struct lws *
wsi_from_fd(const struct lws_context *context, int fd);

int
insert_wsi(const struct lws_context *context, struct lws *wsi);

struct lws_dhcpc_ifstate;
int
lws_plat_ifconfig(int fd, struct lws_dhcpc_ifstate *is);

void
delete_from_fd(const struct lws_context *context, int fd);

#ifndef LWS_NO_FORK
#ifdef LWS_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#endif

#define compatible_close(x) close(x)
#define compatible_file_close(fd) close(fd)
#define lws_plat_socket_offset() (0)

/*
 * Mac OSX as well as iOS do not define the MSG_NOSIGNAL flag,
 * but happily have something equivalent in the SO_NOSIGPIPE flag.
 */
#ifdef __APPLE__
/* iOS SDK 12+ seems to define it, undef it for compatibility both ways */
#undef MSG_NOSIGNAL
#define MSG_NOSIGNAL SO_NOSIGPIPE
#endif

/*
 * Solaris 11.X only supports POSIX 2001, MSG_NOSIGNAL appears in
 * POSIX 2008.
 */
#if defined(__sun) && !defined(MSG_NOSIGNAL)
 #define MSG_NOSIGNAL 0
#endif

int
lws_plat_rawudp_broadcast(uint8_t *p, const uint8_t *canned, size_t canned_len,
			  size_t n, int fd, const char *iface);

int
lws_plat_if_up(const char *ifname, int fd, int up);
