/*
 *    Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *    http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef MSGPACK_GCC_ATOMIC_H
#define MSGPACK_GCC_ATOMIC_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef int _msgpack_atomic_counter_t;

int _msgpack_sync_decr_and_fetch(volatile _msgpack_atomic_counter_t* ptr);
int _msgpack_sync_incr_and_fetch(volatile _msgpack_atomic_counter_t* ptr);


#if defined(__cplusplus)
}
#endif


#endif // MSGPACK_GCC_ATOMIC_H
