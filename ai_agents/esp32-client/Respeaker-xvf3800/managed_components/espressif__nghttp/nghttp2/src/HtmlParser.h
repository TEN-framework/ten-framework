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
#ifndef HTML_PARSER_H
#define HTML_PARSER_H

#include "nghttp2_config.h"

#include <vector>
#include <string>

#ifdef HAVE_LIBXML2

#  include <libxml/HTMLparser.h>

#endif // HAVE_LIBXML2

namespace nghttp2 {

enum ResourceType {
  REQ_CSS = 1,
  REQ_JS,
  REQ_UNBLOCK_JS,
  REQ_IMG,
  REQ_OTHERS,
};

struct ParserData {
  std::string base_uri;
  std::vector<std::pair<std::string, ResourceType>> links;
  // > 0 if we are inside "head" element.
  int inside_head;
  ParserData(const std::string &base_uri);
};

#ifdef HAVE_LIBXML2

class HtmlParser {
public:
  HtmlParser(const std::string &base_uri);
  ~HtmlParser();
  int parse_chunk(const char *chunk, size_t size, int fin);
  const std::vector<std::pair<std::string, ResourceType>> &get_links() const;
  void clear_links();

private:
  int parse_chunk_internal(const char *chunk, size_t size, int fin);

  std::string base_uri_;
  htmlParserCtxtPtr parser_ctx_;
  ParserData parser_data_;
};

#else // !HAVE_LIBXML2

class HtmlParser {
public:
  HtmlParser(const std::string &base_uri) {}
  int parse_chunk(const char *chunk, size_t size, int fin) { return 0; }
  const std::vector<std::pair<std::string, ResourceType>> &get_links() const {
    return links_;
  }
  void clear_links() {}

private:
  std::vector<std::pair<std::string, ResourceType>> links_;
};

#endif // !HAVE_LIBXML2

} // namespace nghttp2

#endif // HTML_PARSER_H
