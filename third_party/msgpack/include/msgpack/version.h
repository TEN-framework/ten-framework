/*
 * MessagePack for C version information
 *
 * Copyright (C) 2008-2009 FURUHASHI Sadayuki
 *
 *    Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *    http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef MSGPACK_VERSION_H
#define MSGPACK_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

MSGPACK_DLLEXPORT
const char* msgpack_version(void);
MSGPACK_DLLEXPORT
int msgpack_version_major(void);
MSGPACK_DLLEXPORT
int msgpack_version_minor(void);
MSGPACK_DLLEXPORT
int msgpack_version_revision(void);

#include "version_master.h"

#define MSGPACK_STR(v) #v
#define MSGPACK_VERSION_I(maj, min, rev) MSGPACK_STR(maj) "." MSGPACK_STR(min) "." MSGPACK_STR(rev)

#define MSGPACK_VERSION MSGPACK_VERSION_I(MSGPACK_VERSION_MAJOR, MSGPACK_VERSION_MINOR, MSGPACK_VERSION_REVISION)

#ifdef __cplusplus
}
#endif

#endif /* msgpack/version.h */

