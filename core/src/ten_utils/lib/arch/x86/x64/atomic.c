//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/atomic.h"

/*
 * From Intel Software Development Manual; Vol 3;
 * 8.2.2 Memory Ordering in P6 and More Recent Processor Families:
 * ...
 * . Reads are not reordered with other reads.
 * . Writes are not reordered with older reads.
 * . Writes to memory are not reordered with other writes,
 *   with the following exceptions:
 *   . streaming stores (writes) executed with the non-temporal move
 *     instructions (MOVNTI, MOVNTQ, MOVNTDQ, MOVNTPS, and MOVNTPD); and
 *   . string operations (see Section 8.2.4.1).
 *  ...
 * . Reads may be reordered with older writes to different locations but not
 * with older writes to the same location.
 * . Reads or writes cannot be reordered with I/O instructions,
 * locked instructions, or serializing instructions.
 * . Reads cannot pass earlier LFENCE and MFENCE instructions.
 * . Writes ... cannot pass earlier LFENCE, SFENCE, and MFENCE instructions.
 * . LFENCE instructions cannot pass earlier reads.
 * . SFENCE instructions cannot pass earlier writes ...
 * . MFENCE instructions cannot pass earlier reads, writes ...
 *
 * As pointed by Java guys, that makes possible to use lock-prefixed
 * instructions to get the same effect as mfence and on most modern HW
 * that gives a better performance then using mfence:
 * https://shipilev.net/blog/2014/on-the-fence-with-dependencies/
 * Basic idea is to use lock prefixed add with some dummy memory location
 * as the destination. From their experiments 128B(2 cache lines) below
 * current stack pointer looks like a good candidate.
 * So below we use that techinque for ten_smp_mb() implementation.
 */

void ten_memory_barrier(void) {
  asm volatile("lock addl $0, -128(%%rsp); " ::: "memory");
}