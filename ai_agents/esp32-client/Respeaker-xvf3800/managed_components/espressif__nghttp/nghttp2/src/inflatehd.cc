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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // HAVE_CONFIG_H

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif // HAVE_UNISTD_H
#include <getopt.h>

#include <cstdio>
#include <cstring>
#include <assert.h>
#include <cerrno>
#include <cstdlib>
#include <vector>
#include <iostream>

#include <jansson.h>

#define NGHTTP2_NO_SSIZE_T
#include <nghttp2/nghttp2.h>

#include "template.h"
#include "comp_helper.h"

namespace nghttp2 {

typedef struct {
  int dump_header_table;
} inflate_config;

static inflate_config config;

static uint8_t to_ud(char c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A' + 10;
  } else if (c >= 'a' && c <= 'z') {
    return c - 'a' + 10;
  } else {
    return c - '0';
  }
}

static void decode_hex(uint8_t *dest, const char *src, size_t len) {
  size_t i;
  for (i = 0; i < len; i += 2) {
    *dest++ = to_ud(src[i]) << 4 | to_ud(src[i + 1]);
  }
}

static void to_json(nghttp2_hd_inflater *inflater, json_t *headers,
                    json_t *wire, int seq, size_t old_settings_table_size) {
  auto obj = json_object();
  json_object_set_new(obj, "seq", json_integer(seq));
  json_object_set(obj, "wire", wire);
  json_object_set(obj, "headers", headers);
  auto max_dyn_table_size =
    nghttp2_hd_inflate_get_max_dynamic_table_size(inflater);
  if (old_settings_table_size != max_dyn_table_size) {
    json_object_set_new(obj, "header_table_size",
                        json_integer(max_dyn_table_size));
  }
  if (config.dump_header_table) {
    json_object_set_new(obj, "header_table",
                        dump_inflate_header_table(inflater));
  }
  json_dumpf(obj, stdout, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
  json_decref(obj);
  printf("\n");
}

static int inflate_hd(json_t *obj, nghttp2_hd_inflater *inflater, int seq) {
  nghttp2_nv nv;
  int inflate_flags;
  size_t old_settings_table_size =
    nghttp2_hd_inflate_get_max_dynamic_table_size(inflater);

  auto wire = json_object_get(obj, "wire");

  if (wire == nullptr) {
    fprintf(stderr, "'wire' key is missing at %d\n", seq);
    return -1;
  }

  if (!json_is_string(wire)) {
    fprintf(stderr, "'wire' value is not string at %d\n", seq);
    return -1;
  }

  auto table_size = json_object_get(obj, "header_table_size");

  if (table_size) {
    if (!json_is_integer(table_size)) {
      fprintf(stderr,
              "The value of 'header_table_size key' is not integer at %d\n",
              seq);
      return -1;
    }
    auto rv = nghttp2_hd_inflate_change_table_size(
      inflater, json_integer_value(table_size));
    if (rv != 0) {
      fprintf(stderr,
              "nghttp2_hd_change_table_size() failed with error %s at %d\n",
              nghttp2_strerror(rv), seq);
      return -1;
    }
  }

  auto inputlen = strlen(json_string_value(wire));

  if (inputlen & 1) {
    fprintf(stderr, "Badly formatted output value at %d\n", seq);
    exit(EXIT_FAILURE);
  }

  auto buflen = inputlen / 2;
  auto buf = std::vector<uint8_t>(buflen);

  decode_hex(buf.data(), json_string_value(wire), inputlen);

  auto headers = json_array();

  auto p = buf.data();
  for (;;) {
    inflate_flags = 0;
    auto rv =
      nghttp2_hd_inflate_hd3(inflater, &nv, &inflate_flags, p, buflen, 1);
    if (rv < 0) {
      fprintf(stderr, "inflate failed with error code %zd at %d\n", rv, seq);
      exit(EXIT_FAILURE);
    }
    p += rv;
    buflen -= rv;
    if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
      json_array_append_new(
        headers, dump_header(nv.name, nv.namelen, nv.value, nv.valuelen));
    }
    if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL) {
      break;
    }
  }
  assert(buflen == 0);
  nghttp2_hd_inflate_end_headers(inflater);
  to_json(inflater, headers, wire, seq, old_settings_table_size);
  json_decref(headers);

  return 0;
}

static int perform(void) {
  nghttp2_hd_inflater *inflater = nullptr;
  json_error_t error;

  auto json = json_loadf(stdin, 0, &error);

  if (json == nullptr) {
    fprintf(stderr, "JSON loading failed\n");
    exit(EXIT_FAILURE);
  }

  auto cases = json_object_get(json, "cases");

  if (cases == nullptr) {
    fprintf(stderr, "Missing 'cases' key in root object\n");
    exit(EXIT_FAILURE);
  }

  if (!json_is_array(cases)) {
    fprintf(stderr, "'cases' must be JSON array\n");
    exit(EXIT_FAILURE);
  }

  nghttp2_hd_inflate_new(&inflater);
  output_json_header();
  auto len = json_array_size(cases);

  for (size_t i = 0; i < len; ++i) {
    auto obj = json_array_get(cases, i);
    if (!json_is_object(obj)) {
      fprintf(stderr, "Unexpected JSON type at %zu. It should be object.\n", i);
      continue;
    }
    if (inflate_hd(obj, inflater, i) != 0) {
      continue;
    }
    if (i + 1 < len) {
      printf(",\n");
    }
  }
  output_json_footer();
  nghttp2_hd_inflate_del(inflater);
  json_decref(json);

  return 0;
}

static void print_help(void) {
  std::cout << R"(HPACK HTTP/2 header decoder
Usage: inflatehd [OPTIONS] < INPUT

Reads JSON  data from stdin  and outputs inflated name/value  pairs in
JSON.

The root JSON object must contain "context" key, which indicates which
compression context is used.  If  it is "request", request compression
context  is used.   Otherwise, response  compression context  is used.
The value  of "cases" key  contains the sequence of  compressed header
block.  They share  the same compression context and  are processed in
the order they appear.  Each item in the sequence is a JSON object and
it must  have at least "wire"  key.  Its value is  a string containing
compressed header block in hex string.

Example:

{
  "context": "request",
  "cases":
  [
    { "wire": "0284f77778ff" },
    { "wire": "0185fafd3c3c7f81" }
  ]
}

The output of this program can be used as input for deflatehd.

OPTIONS:
    -d, --dump-header-table
                      Output dynamic header table.)"
            << std::endl;
  ;
}

constexpr static struct option long_options[] = {
  {"dump-header-table", no_argument, nullptr, 'd'}, {nullptr, 0, nullptr, 0}};

int main(int argc, char **argv) {
  config.dump_header_table = 0;
  while (1) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "dh", long_options, &option_index);
    if (c == -1) {
      break;
    }
    switch (c) {
    case 'h':
      print_help();
      exit(EXIT_SUCCESS);
    case 'd':
      // --dump-header-table
      config.dump_header_table = 1;
      break;
    case '?':
      exit(EXIT_FAILURE);
    default:
      break;
    }
  }
  perform();
  return 0;
}

} // namespace nghttp2

int main(int argc, char **argv) {
  return nghttp2::run_app(nghttp2::main, argc, argv);
}
