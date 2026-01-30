/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2013 Tatsuhiro Tsujikawa
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
#include "timegm.h"

#include <inttypes.h>

/* Counter the number of leap year in the range [0, y). The |y| is the
   year, including century (e.g., 2012) */
static int count_leap_year(int y) {
  y -= 1;
  return y / 4 - y / 100 + y / 400;
}

/* Based on the algorithm of Python 2.7 calendar.timegm. */
time_t nghttp2_timegm(struct tm *tm) {
  int days;
  int num_leap_year;
  int64_t t;
  if (tm->tm_mon > 11) {
    return -1;
  }
  num_leap_year = count_leap_year(tm->tm_year + 1900) - count_leap_year(1970);
  days = (tm->tm_year - 70) * 365 + num_leap_year + tm->tm_yday;
  t = ((int64_t)days * 24 + tm->tm_hour) * 3600 + tm->tm_min * 60 + tm->tm_sec;

  if (sizeof(time_t) == 4) {
    if (t < INT32_MIN || t > INT32_MAX) {
      return -1;
    }
  }

  return (time_t)t;
}

/* Returns nonzero if the |y| is the leap year. The |y| is the year,
   including century (e.g., 2012) */
static int is_leap_year(int y) {
  return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

/* The number of days before ith month begins */
static int daysum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

time_t nghttp2_timegm_without_yday(struct tm *tm) {
  int days;
  int num_leap_year;
  int64_t t;
  if (tm->tm_mon > 11) {
    return -1;
  }
  num_leap_year = count_leap_year(tm->tm_year + 1900) - count_leap_year(1970);
  days = (tm->tm_year - 70) * 365 + num_leap_year + daysum[tm->tm_mon] +
         tm->tm_mday - 1;
  if (tm->tm_mon >= 2 && is_leap_year(tm->tm_year + 1900)) {
    ++days;
  }
  t = ((int64_t)days * 24 + tm->tm_hour) * 3600 + tm->tm_min * 60 + tm->tm_sec;

  if (sizeof(time_t) == 4) {
    if (t < INT32_MIN || t > INT32_MAX) {
      return -1;
    }
  }

  return (time_t)t;
}
