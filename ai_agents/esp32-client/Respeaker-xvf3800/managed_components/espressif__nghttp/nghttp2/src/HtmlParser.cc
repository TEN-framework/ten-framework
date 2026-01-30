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
#include "HtmlParser.h"

#include <libxml/uri.h>

#include "util.h"

namespace nghttp2 {

ParserData::ParserData(const std::string &base_uri)
  : base_uri(base_uri), inside_head(0) {}

HtmlParser::HtmlParser(const std::string &base_uri)
  : base_uri_(base_uri), parser_ctx_(nullptr), parser_data_(base_uri) {}

HtmlParser::~HtmlParser() { htmlFreeParserCtxt(parser_ctx_); }

namespace {
StringRef get_attr(const xmlChar **attrs, const StringRef &name) {
  if (attrs == nullptr) {
    return StringRef{};
  }
  for (; *attrs; attrs += 2) {
    if (util::strieq(
          StringRef{attrs[0], strlen(reinterpret_cast<const char *>(attrs[0]))},
          name)) {
      return StringRef{attrs[1],
                       strlen(reinterpret_cast<const char *>(attrs[1]))};
    }
  }
  return StringRef{};
}
} // namespace

namespace {
ResourceType
get_resource_type_for_preload_as(const StringRef &attribute_value) {
  if (util::strieq("image"_sr, attribute_value)) {
    return REQ_IMG;
  } else if (util::strieq("style"_sr, attribute_value)) {
    return REQ_CSS;
  } else if (util::strieq("script"_sr, attribute_value)) {
    return REQ_UNBLOCK_JS;
  } else {
    return REQ_OTHERS;
  }
}
} // namespace

namespace {
void add_link(ParserData *parser_data, const StringRef &uri,
              ResourceType res_type) {
  auto u = xmlBuildURI(
    reinterpret_cast<const xmlChar *>(uri.data()),
    reinterpret_cast<const xmlChar *>(parser_data->base_uri.c_str()));
  if (u) {
    parser_data->links.push_back(
      std::make_pair(reinterpret_cast<char *>(u), res_type));
    xmlFree(u);
  }
}
} // namespace

namespace {
void start_element_func(void *user_data, const xmlChar *src_name,
                        const xmlChar **attrs) {
  auto parser_data = static_cast<ParserData *>(user_data);
  auto name =
    StringRef{src_name, strlen(reinterpret_cast<const char *>(src_name))};
  if (util::strieq("head"_sr, name)) {
    ++parser_data->inside_head;
  }
  if (util::strieq("link"_sr, name)) {
    auto rel_attr = get_attr(attrs, "rel"_sr);
    auto href_attr = get_attr(attrs, "href"_sr);
    if (rel_attr.empty() || href_attr.empty()) {
      return;
    }
    if (util::strieq("shortcut icon"_sr, rel_attr)) {
      add_link(parser_data, href_attr, REQ_OTHERS);
    } else if (util::strieq("stylesheet"_sr, rel_attr)) {
      add_link(parser_data, href_attr, REQ_CSS);
    } else if (util::strieq("preload"_sr, rel_attr)) {
      auto as_attr = get_attr(attrs, "as"_sr);
      if (as_attr.empty()) {
        return;
      }
      add_link(parser_data, href_attr,
               get_resource_type_for_preload_as(as_attr));
    }
  } else if (util::strieq("img"_sr, name)) {
    auto src_attr = get_attr(attrs, "src"_sr);
    if (src_attr.empty()) {
      return;
    }
    add_link(parser_data, src_attr, REQ_IMG);
  } else if (util::strieq("script"_sr, name)) {
    auto src_attr = get_attr(attrs, "src"_sr);
    if (src_attr.empty()) {
      return;
    }
    if (parser_data->inside_head) {
      add_link(parser_data, src_attr, REQ_JS);
    } else {
      add_link(parser_data, src_attr, REQ_UNBLOCK_JS);
    }
  }
}
} // namespace

namespace {
void end_element_func(void *user_data, const xmlChar *name) {
  auto parser_data = static_cast<ParserData *>(user_data);
  if (util::strieq(
        "head"_sr,
        StringRef{name, strlen(reinterpret_cast<const char *>(name))})) {
    --parser_data->inside_head;
  }
}
} // namespace

namespace {
xmlSAXHandler saxHandler = {
  nullptr,             // internalSubsetSAXFunc
  nullptr,             // isStandaloneSAXFunc
  nullptr,             // hasInternalSubsetSAXFunc
  nullptr,             // hasExternalSubsetSAXFunc
  nullptr,             // resolveEntitySAXFunc
  nullptr,             // getEntitySAXFunc
  nullptr,             // entityDeclSAXFunc
  nullptr,             // notationDeclSAXFunc
  nullptr,             // attributeDeclSAXFunc
  nullptr,             // elementDeclSAXFunc
  nullptr,             // unparsedEntityDeclSAXFunc
  nullptr,             // setDocumentLocatorSAXFunc
  nullptr,             // startDocumentSAXFunc
  nullptr,             // endDocumentSAXFunc
  &start_element_func, // startElementSAXFunc
  &end_element_func,   // endElementSAXFunc
  nullptr,             // referenceSAXFunc
  nullptr,             // charactersSAXFunc
  nullptr,             // ignorableWhitespaceSAXFunc
  nullptr,             // processingInstructionSAXFunc
  nullptr,             // commentSAXFunc
  nullptr,             // warningSAXFunc
  nullptr,             // errorSAXFunc
  nullptr,             // fatalErrorSAXFunc
  nullptr,             // getParameterEntitySAXFunc
  nullptr,             // cdataBlockSAXFunc
  nullptr,             // externalSubsetSAXFunc
  0,                   // unsigned int initialized
  nullptr,             // void * _private
  nullptr,             // startElementNsSAX2Func
  nullptr,             // endElementNsSAX2Func
  nullptr,             // xmlStructuredErrorFunc
};
} // namespace

int HtmlParser::parse_chunk(const char *chunk, size_t size, int fin) {
  if (!parser_ctx_) {
    parser_ctx_ =
      htmlCreatePushParserCtxt(&saxHandler, &parser_data_, chunk, size,
                               base_uri_.c_str(), XML_CHAR_ENCODING_NONE);
    if (!parser_ctx_) {
      return -1;
    } else {
      if (fin) {
        return parse_chunk_internal(nullptr, 0, fin);
      } else {
        return 0;
      }
    }
  } else {
    return parse_chunk_internal(chunk, size, fin);
  }
}

int HtmlParser::parse_chunk_internal(const char *chunk, size_t size, int fin) {
  int rv = htmlParseChunk(parser_ctx_, chunk, size, fin);
  if (rv == 0) {
    return 0;
  } else {
    return -1;
  }
}

const std::vector<std::pair<std::string, ResourceType>> &
HtmlParser::get_links() const {
  return parser_data_.links;
}

void HtmlParser::clear_links() { parser_data_.links.clear(); }

} // namespace nghttp2
