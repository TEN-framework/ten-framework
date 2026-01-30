/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
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
#ifndef SHRPX_SIGNAL_H
#define SHRPX_SIGNAL_H

#include "shrpx.h"

#include <signal.h>

namespace shrpx {

constexpr int REOPEN_LOG_SIGNAL = SIGUSR1;
constexpr int EXEC_BINARY_SIGNAL = SIGUSR2;
constexpr int GRACEFUL_SHUTDOWN_SIGNAL = SIGQUIT;
constexpr int RELOAD_SIGNAL = SIGHUP;

// Blocks all signals.  The previous signal mask is stored into
// |oldset| if it is not nullptr.  This function returns 0 if it
// succeeds, or -1.  The errno will indicate the error.
int shrpx_signal_block_all(sigset_t *oldset);

// Unblocks all signals.  This function returns 0 if it succeeds, or
// -1.  The errno will indicate the error.
int shrpx_signal_unblock_all();

// Sets signal mask |set|.  This function returns 0 if it succeeds, or
// -1.  The errno will indicate the error.
int shrpx_signal_set(sigset_t *set);

int shrpx_signal_set_main_proc_ign_handler();
int shrpx_signal_unset_main_proc_ign_handler();

int shrpx_signal_set_worker_proc_ign_handler();
int shrpx_signal_unset_worker_proc_ign_handler();

} // namespace shrpx

#endif // SHRPX_SIGNAL_H
