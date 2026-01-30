/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef SHRPX_H
#define SHRPX_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // HAVE_CONFIG_H

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif // HAVE_SYS_SOCKET_H

#include <cassert>

#define NGHTTP2_NO_SSIZE_T

#ifndef HAVE__EXIT
#  define nghttp2_Exit(status) _exit(status)
#else // HAVE__EXIT
#  define nghttp2_Exit(status) _Exit(status)
#endif // HAVE__EXIT

#define DIE() nghttp2_Exit(EXIT_FAILURE)

#if defined(HAVE_DECL_INITGROUPS) && !HAVE_DECL_INITGROUPS
inline int initgroups(const char *user, gid_t group) { return 0; }
#endif // defined(HAVE_DECL_INITGROUPS) && !HAVE_DECL_INITGROUPS

#ifndef HAVE_BPF_STATS_TYPE
/* Newer kernel should have this defined in linux/bpf.h */
enum bpf_stats_type {
  BPF_STATS_RUN_TIME = 0,
};
#endif // !HAVE_BPF_STATS_TYPE

#endif // SHRPX_H
