/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2014 Tatsuhiro Tsujikawa
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
#ifndef SHRPX_LOG_CONFIG_H
#define SHRPX_LOG_CONFIG_H

#include "shrpx.h"

#include <sys/types.h>

#include <chrono>

#include "template.h"

using namespace nghttp2;

namespace shrpx {

struct Timestamp {
  Timestamp(const std::chrono::system_clock::time_point &tp);

  std::array<char, sizeof("03/Jul/2014:00:19:38 +0900")> time_local_buf;
  std::array<char, sizeof("2014-11-15T12:58:24.741+09:00")> time_iso8601_buf;
  std::array<char, sizeof("Mon, 10 Oct 2016 10:25:58 GMT")> time_http_buf;
  StringRef time_local;
  StringRef time_iso8601;
  StringRef time_http;
};

struct LogConfig {
  std::chrono::system_clock::time_point time_str_updated;
  std::shared_ptr<Timestamp> tstamp;
  std::string thread_id;
  pid_t pid;
  int accesslog_fd;
  int errorlog_fd;
  // true if errorlog_fd is referring to a terminal.
  bool errorlog_tty;

  LogConfig();
  // Updates time stamp if difference between time_str_updated and now
  // is 1 or more milliseconds.
  void update_tstamp_millis(const std::chrono::system_clock::time_point &now);
  // Updates time stamp if difference between time_str_updated and
  // now, converted to time_t, is 1 or more seconds.
  void update_tstamp(const std::chrono::system_clock::time_point &now);
};

// We need LogConfig per thread to avoid data race around opening file
// descriptor for log files.
LogConfig *log_config();

// Deletes log_config
void delete_log_config();

} // namespace shrpx

#endif // SHRPX_LOG_CONFIG_H
