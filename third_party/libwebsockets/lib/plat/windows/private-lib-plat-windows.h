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
 * Included from lib/private-lib-core.h if defined(WIN32) || defined(_WIN32)
 */

 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif

 #if defined(WINVER) && (WINVER < 0x0501)
  #undef WINVER
  #undef _WIN32_WINNT
  #define WINVER 0x0501
  #define _WIN32_WINNT WINVER
 #endif

 #define LWS_NO_DAEMONIZE
 #define LWS_ERRNO WSAGetLastError()
 #define LWS_EAGAIN WSAEWOULDBLOCK
 #define LWS_EALREADY WSAEALREADY
 #define LWS_EINPROGRESS WSAEINPROGRESS
 #define LWS_EINTR WSAEINTR
 #define LWS_EISCONN WSAEISCONN
 #define LWS_ENOTCONN WSAENOTCONN
 #define LWS_EWOULDBLOCK WSAEWOULDBLOCK
 #define LWS_EADDRINUSE WSAEADDRINUSE
 #define MSG_NOSIGNAL 0
 #define SHUT_RDWR SD_BOTH
 #define SOL_TCP IPPROTO_TCP
 #define SHUT_WR SD_SEND

 #define compatible_close(fd) closesocket(fd)
 #define compatible_file_close(fd) CloseHandle(fd)
 #define lws_set_blocking_send(wsi) wsi->sock_send_blocking = 1

 #include <winsock2.h>
 #include <ws2tcpip.h>
 #include <windows.h>
 #include <tchar.h>
 #ifdef LWS_HAVE_IN6ADDR_H
  #include <in6addr.h>
 #endif
 #include <mstcpip.h>
 #include <io.h>

#if defined(LWS_WITH_UNIX_SOCK)
#include <afunix.h>
#endif

#if defined(LWS_WITH_TLS)
#include <wincrypt.h>
#endif

#if defined(LWS_HAVE_PTHREAD_H)
#define lws_mutex_t		pthread_mutex_t
#define lws_mutex_init(x)	pthread_mutex_init(&(x), NULL)
#define lws_mutex_destroy(x)	pthread_mutex_destroy(&(x))
#define lws_mutex_lock(x)	pthread_mutex_lock(&(x))
#define lws_mutex_unlock(x)	pthread_mutex_unlock(&(x))
#endif

 #if !defined(LWS_HAVE_ATOLL)
  #if defined(LWS_HAVE__ATOI64)
   #define atoll _atoi64
  #else
   #warning No atoll or _atoi64 available, using atoi
   #define atoll atoi
  #endif
 #endif

 #ifndef __func__
  #define __func__ __FUNCTION__
 #endif

 #ifdef LWS_HAVE__VSNPRINTF
  #define vsnprintf _vsnprintf
 #endif

/* we don't have an implementation for this on windows... */
int kill(int pid, int sig);
int fork(void);
#ifndef SIGINT
#define SIGINT 2
#endif

#include <gettimeofday.h>

#ifndef BIG_ENDIAN
 #define BIG_ENDIAN    4321  /* to show byte order (taken from gcc) */
#endif
#ifndef LITTLE_ENDIAN
 #define LITTLE_ENDIAN 1234
#endif
#ifndef BYTE_ORDER
 #define BYTE_ORDER LITTLE_ENDIAN
#endif

#undef __P
#ifndef __P
 #if __STDC__
  #define __P(protos) protos
 #else
  #define __P(protos) ()
 #endif
#endif

#ifdef _WIN32
 #ifndef FD_HASHTABLE_MODULUS
  #define FD_HASHTABLE_MODULUS 32
 #endif
#endif

#define lws_plat_socket_offset() (0)

struct lws;
struct lws_context;

#define LWS_FD_HASH(fd) ((fd ^ (fd >> 8) ^ (fd >> 16)) % FD_HASHTABLE_MODULUS)
struct lws_fd_hashtable {
	struct lws **wsi;
	int length;
};

#if !defined(LWS_EXTERN)
#ifdef LWS_DLL
#ifdef LWS_INTERNAL
#define LWS_EXTERN extern __declspec(dllexport)
#else
#define LWS_EXTERN extern __declspec(dllimport)
#endif
#else
#define LWS_EXTERN
#endif
#endif

typedef SOCKET lws_sockfd_type;
#if defined(__MINGW32__)
typedef int lws_filefd_type;
#else
typedef HANDLE lws_filefd_type;
#endif
#define LWS_WIN32_HANDLE_TYPES

LWS_EXTERN struct lws *
wsi_from_fd(const struct lws_context *context, lws_sockfd_type fd);

LWS_EXTERN int
insert_wsi(struct lws_context *context, struct lws *wsi);

LWS_EXTERN int
delete_from_fd(struct lws_context *context, lws_sockfd_type fd);
