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
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <vector>
#include <iostream>

#include <jansson.h>

#define NGHTTP2_NO_SSIZE_T
#include <nghttp2/nghttp2.h>

#include "template.h"
#include "comp_helper.h"
#include "util.h"

namespace nghttp2 {

typedef struct {
  size_t table_size;
  size_t deflate_table_size;
  int http1text;
  int dump_header_table;
} deflate_config;

static deflate_config config;

static size_t input_sum;
static size_t output_sum;

static char to_hex_digit(uint8_t n) {
  if (n > 9) {
    return n - 10 + 'a';
  }
  return n + '0';
}

static void to_hex(char *dest, const uint8_t *src, size_t len) {
  size_t i;
  for (i = 0; i < len; ++i) {
    *dest++ = to_hex_digit(src[i] >> 4);
    *dest++ = to_hex_digit(src[i] & 0xf);
  }
}

static void output_to_json(nghttp2_hd_deflater *deflater, const uint8_t *buf,
                           size_t buflen, size_t inputlen,
                           const std::vector<nghttp2_nv> &nva, int seq) {
  auto hex = std::vector<char>(buflen * 2);
  auto obj = json_object();
  auto comp_ratio = inputlen == 0 ? 0.0 : (double)buflen / inputlen * 100;

  json_object_set_new(obj, "seq", json_integer(seq));
  json_object_set_new(obj, "input_length", json_integer(inputlen));
  json_object_set_new(obj, "output_length", json_integer(buflen));
  json_object_set_new(obj, "percentage_of_original_size",
                      json_real(comp_ratio));

  if (buflen == 0) {
    json_object_set_new(obj, "wire", json_string(""));
  } else {
    to_hex(hex.data(), buf, buflen);
    json_object_set_new(obj, "wire", json_pack("s#", hex.data(), hex.size()));
  }
  json_object_set_new(obj, "headers", dump_headers(nva.data(), nva.size()));
  if (seq == 0) {
    // We only change the header table size only once at the beginning
    json_object_set_new(obj, "header_table_size",
                        json_integer(config.table_size));
  }
  if (config.dump_header_table) {
    json_object_set_new(obj, "header_table",
                        dump_deflate_header_table(deflater));
  }
  json_dumpf(obj, stdout, JSON_PRESERVE_ORDER | JSON_INDENT(2));
  printf("\n");
  json_decref(obj);
}

static void deflate_hd(nghttp2_hd_deflater *deflater,
                       const std::vector<nghttp2_nv> &nva, size_t inputlen,
                       int seq) {
  std::array<uint8_t, 64_k> buf;

  auto rv = nghttp2_hd_deflate_hd2(deflater, buf.data(), buf.size(),
                                   (nghttp2_nv *)nva.data(), nva.size());
  if (rv < 0) {
    fprintf(stderr, "deflate failed with error code %zd at %d\n", rv, seq);
    exit(EXIT_FAILURE);
  }

  input_sum += inputlen;
  output_sum += rv;

  output_to_json(deflater, buf.data(), rv, inputlen, nva, seq);
}

static int deflate_hd_json(json_t *obj, nghttp2_hd_deflater *deflater,
                           int seq) {
  size_t inputlen = 0;

  auto js = json_object_get(obj, "headers");
  if (js == nullptr) {
    fprintf(stderr, "'headers' key is missing at %d\n", seq);
    return -1;
  }
  if (!json_is_array(js)) {
    fprintf(stderr, "The value of 'headers' key must be an array at %d\n", seq);
    return -1;
  }

  auto len = json_array_size(js);
  auto nva = std::vector<nghttp2_nv>(len);

  for (size_t i = 0; i < len; ++i) {
    auto nv_pair = json_array_get(js, i);
    const char *name;
    json_t *value;

    if (!json_is_object(nv_pair) || json_object_size(nv_pair) != 1) {
      fprintf(stderr, "bad formatted name/value pair object at %d\n", seq);
      return -1;
    }

    json_object_foreach(nv_pair, name, value) {
      nva[i].name = (uint8_t *)name;
      nva[i].namelen = strlen(name);

      if (!json_is_string(value)) {
        fprintf(stderr, "value is not string at %d\n", seq);
        return -1;
      }

      nva[i].value = (uint8_t *)json_string_value(value);
      nva[i].valuelen = strlen(json_string_value(value));

      nva[i].flags = NGHTTP2_NV_FLAG_NONE;
    }

    inputlen += nva[i].namelen + nva[i].valuelen;
  }

  deflate_hd(deflater, nva, inputlen, seq);

  return 0;
}

static nghttp2_hd_deflater *init_deflater() {
  nghttp2_hd_deflater *deflater;
  nghttp2_hd_deflate_new(&deflater, config.deflate_table_size);
  if (config.table_size != NGHTTP2_DEFAULT_HEADER_TABLE_SIZE) {
    nghttp2_hd_deflate_change_table_size(deflater, config.table_size);
  }
  return deflater;
}

static void deinit_deflater(nghttp2_hd_deflater *deflater) {
  nghttp2_hd_deflate_del(deflater);
}

static int perform(void) {
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

  auto deflater = init_deflater();
  output_json_header();
  auto len = json_array_size(cases);

  for (size_t i = 0; i < len; ++i) {
    auto obj = json_array_get(cases, i);
    if (!json_is_object(obj)) {
      fprintf(stderr, "Unexpected JSON type at %zu. It should be object.\n", i);
      continue;
    }
    if (deflate_hd_json(obj, deflater, i) != 0) {
      continue;
    }
    if (i + 1 < len) {
      printf(",\n");
    }
  }
  output_json_footer();
  deinit_deflater(deflater);
  json_decref(json);
  return 0;
}

static int perform_from_http1text(void) {
  char line[1 << 14];
  int seq = 0;

  auto deflater = init_deflater();
  output_json_header();
  for (;;) {
    std::vector<nghttp2_nv> nva;
    int end = 0;
    size_t inputlen = 0;

    for (;;) {
      char *rv = fgets(line, sizeof(line), stdin);
      char *val, *val_end;
      if (rv == nullptr) {
        end = 1;
        break;
      } else if (line[0] == '\n') {
        break;
      }

      nva.emplace_back();
      auto &nv = nva.back();

      val = strchr(line + 1, ':');
      if (val == nullptr) {
        fprintf(stderr, "Bad HTTP/1 header field format at %d.\n", seq);
        exit(EXIT_FAILURE);
      }
      *val = '\0';
      ++val;
      for (; *val && (*val == ' ' || *val == '\t'); ++val)
        ;
      for (val_end = val; *val_end && (*val_end != '\r' && *val_end != '\n');
           ++val_end)
        ;
      *val_end = '\0';

      nv.namelen = strlen(line);
      nv.valuelen = strlen(val);
      nv.name = (uint8_t *)strdup(line);
      nv.value = (uint8_t *)strdup(val);
      nv.flags = NGHTTP2_NV_FLAG_NONE;

      inputlen += nv.namelen + nv.valuelen;
    }

    if (!end) {
      if (seq > 0) {
        printf(",\n");
      }
      deflate_hd(deflater, nva, inputlen, seq);
    }

    for (auto &nv : nva) {
      free(nv.name);
      free(nv.value);
    }

    if (end)
      break;
    ++seq;
  }
  output_json_footer();
  deinit_deflater(deflater);
  return 0;
}

static void print_help(void) {
  std::cout << R"(HPACK HTTP/2 header encoder
Usage: deflatehd [OPTIONS] < INPUT

Reads JSON data  or HTTP/1-style header fields from  stdin and outputs
deflated header block in JSON array.

For the JSON  input, the root JSON object must  contain "context" key,
which  indicates  which  compression  context   is  used.   If  it  is
"request", request  compression context is used.   Otherwise, response
compression context  is used.  The  value of "cases" key  contains the
sequence of input header set.  They share the same compression context
and are processed in the order they appear.  Each item in the sequence
is a JSON object  and it must have at least  "headers" key.  Its value
is an array of a JSON object containing exactly one name/value pair.

Example:
{
  "context": "request",
  "cases":
  [
    {
      "headers": [
        { ":method": "GET" },
        { ":path": "/" }
      ]
    },
    {
      "headers": [
        { ":method": "POST" },
        { ":path": "/" }
      ]
    }
  ]
}

With  -t option,  the program  can accept  more familiar  HTTP/1 style
header field  block.  Each header  set must  be followed by  one empty
line:

Example:

:method: GET
:scheme: https
:path: /

:method: POST
user-agent: nghttp2

The output of this program can be used as input for inflatehd.

OPTIONS:
    -t, --http1text   Use  HTTP/1 style  header field  text as  input.
                      Each  header set  is delimited  by single  empty
                      line.
    -s, --table-size=<N>
                      Set   dynamic   table   size.   In   the   HPACK
                      specification,   this   value  is   denoted   by
                      SETTINGS_HEADER_TABLE_SIZE.
                      Default: 4096
    -S, --deflate-table-size=<N>
                      Use  first  N  bytes  of  dynamic  header  table
                      buffer.
                      Default: 4096
    -d, --dump-header-table
                      Output dynamic header table.)"
            << std::endl;
}

constexpr static struct option long_options[] = {
  {"http1text", no_argument, nullptr, 't'},
  {"table-size", required_argument, nullptr, 's'},
  {"deflate-table-size", required_argument, nullptr, 'S'},
  {"dump-header-table", no_argument, nullptr, 'd'},
  {nullptr, 0, nullptr, 0}};

int main(int argc, char **argv) {
  config.table_size = 4_k;
  config.deflate_table_size = 4_k;
  config.http1text = 0;
  config.dump_header_table = 0;
  while (1) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "S:dhs:t", long_options, &option_index);
    if (c == -1) {
      break;
    }
    switch (c) {
    case 'h':
      print_help();
      exit(EXIT_SUCCESS);
    case 't':
      // --http1text
      config.http1text = 1;
      break;
    case 's': {
      // --table-size
      auto n = util::parse_uint(optarg);
      if (!n) {
        fprintf(stderr, "-s: Bad option value\n");
        exit(EXIT_FAILURE);
      }
      config.table_size = *n;
      break;
    }
    case 'S': {
      // --deflate-table-size
      auto n = util::parse_uint(optarg);
      if (!n) {
        fprintf(stderr, "-S: Bad option value\n");
        exit(EXIT_FAILURE);
      }
      config.deflate_table_size = *n;
      break;
    }
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
  if (config.http1text) {
    perform_from_http1text();
  } else {
    perform();
  }

  auto comp_ratio = input_sum == 0 ? 0.0 : (double)output_sum / input_sum;

  fprintf(stderr, "Overall: input=%zu output=%zu ratio=%.02f\n", input_sum,
          output_sum, comp_ratio);
  return 0;
}

} // namespace nghttp2

int main(int argc, char **argv) {
  return nghttp2::run_app(nghttp2::main, argc, argv);
}
