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
#include "http2.h"

#include "llhttp.h"

#include "util.h"

namespace nghttp2 {

namespace http2 {

StringRef get_reason_phrase(unsigned int status_code) {
  switch (status_code) {
  case 100:
    return "Continue"_sr;
  case 101:
    return "Switching Protocols"_sr;
  case 103:
    return "Early Hints"_sr;
  case 200:
    return "OK"_sr;
  case 201:
    return "Created"_sr;
  case 202:
    return "Accepted"_sr;
  case 203:
    return "Non-Authoritative Information"_sr;
  case 204:
    return "No Content"_sr;
  case 205:
    return "Reset Content"_sr;
  case 206:
    return "Partial Content"_sr;
  case 300:
    return "Multiple Choices"_sr;
  case 301:
    return "Moved Permanently"_sr;
  case 302:
    return "Found"_sr;
  case 303:
    return "See Other"_sr;
  case 304:
    return "Not Modified"_sr;
  case 305:
    return "Use Proxy"_sr;
  // case 306: return "(Unused)"_sr;
  case 307:
    return "Temporary Redirect"_sr;
  case 308:
    return "Permanent Redirect"_sr;
  case 400:
    return "Bad Request"_sr;
  case 401:
    return "Unauthorized"_sr;
  case 402:
    return "Payment Required"_sr;
  case 403:
    return "Forbidden"_sr;
  case 404:
    return "Not Found"_sr;
  case 405:
    return "Method Not Allowed"_sr;
  case 406:
    return "Not Acceptable"_sr;
  case 407:
    return "Proxy Authentication Required"_sr;
  case 408:
    return "Request Timeout"_sr;
  case 409:
    return "Conflict"_sr;
  case 410:
    return "Gone"_sr;
  case 411:
    return "Length Required"_sr;
  case 412:
    return "Precondition Failed"_sr;
  case 413:
    return "Payload Too Large"_sr;
  case 414:
    return "URI Too Long"_sr;
  case 415:
    return "Unsupported Media Type"_sr;
  case 416:
    return "Requested Range Not Satisfiable"_sr;
  case 417:
    return "Expectation Failed"_sr;
  case 421:
    return "Misdirected Request"_sr;
  case 425:
    // https://tools.ietf.org/html/rfc8470
    return "Too Early"_sr;
  case 426:
    return "Upgrade Required"_sr;
  case 428:
    return "Precondition Required"_sr;
  case 429:
    return "Too Many Requests"_sr;
  case 431:
    return "Request Header Fields Too Large"_sr;
  case 451:
    return "Unavailable For Legal Reasons"_sr;
  case 500:
    return "Internal Server Error"_sr;
  case 501:
    return "Not Implemented"_sr;
  case 502:
    return "Bad Gateway"_sr;
  case 503:
    return "Service Unavailable"_sr;
  case 504:
    return "Gateway Timeout"_sr;
  case 505:
    return "HTTP Version Not Supported"_sr;
  case 511:
    return "Network Authentication Required"_sr;
  default:
    return StringRef{};
  }
}

StringRef stringify_status(BlockAllocator &balloc, unsigned int status_code) {
  switch (status_code) {
  case 100:
    return "100"_sr;
  case 101:
    return "101"_sr;
  case 103:
    return "103"_sr;
  case 200:
    return "200"_sr;
  case 201:
    return "201"_sr;
  case 202:
    return "202"_sr;
  case 203:
    return "203"_sr;
  case 204:
    return "204"_sr;
  case 205:
    return "205"_sr;
  case 206:
    return "206"_sr;
  case 300:
    return "300"_sr;
  case 301:
    return "301"_sr;
  case 302:
    return "302"_sr;
  case 303:
    return "303"_sr;
  case 304:
    return "304"_sr;
  case 305:
    return "305"_sr;
  // case 306: return "306"_sr;
  case 307:
    return "307"_sr;
  case 308:
    return "308"_sr;
  case 400:
    return "400"_sr;
  case 401:
    return "401"_sr;
  case 402:
    return "402"_sr;
  case 403:
    return "403"_sr;
  case 404:
    return "404"_sr;
  case 405:
    return "405"_sr;
  case 406:
    return "406"_sr;
  case 407:
    return "407"_sr;
  case 408:
    return "408"_sr;
  case 409:
    return "409"_sr;
  case 410:
    return "410"_sr;
  case 411:
    return "411"_sr;
  case 412:
    return "412"_sr;
  case 413:
    return "413"_sr;
  case 414:
    return "414"_sr;
  case 415:
    return "415"_sr;
  case 416:
    return "416"_sr;
  case 417:
    return "417"_sr;
  case 421:
    return "421"_sr;
  case 426:
    return "426"_sr;
  case 428:
    return "428"_sr;
  case 429:
    return "429"_sr;
  case 431:
    return "431"_sr;
  case 451:
    return "451"_sr;
  case 500:
    return "500"_sr;
  case 501:
    return "501"_sr;
  case 502:
    return "502"_sr;
  case 503:
    return "503"_sr;
  case 504:
    return "504"_sr;
  case 505:
    return "505"_sr;
  case 511:
    return "511"_sr;
  default:
    return util::make_string_ref_uint(balloc, status_code);
  }
}

void capitalize(DefaultMemchunks *buf, const StringRef &s) {
  buf->append(util::upcase(s[0]));
  for (size_t i = 1; i < s.size(); ++i) {
    if (s[i - 1] == '-') {
      buf->append(util::upcase(s[i]));
    } else {
      buf->append(s[i]);
    }
  }
}

bool lws(const char *value) {
  for (; *value; ++value) {
    switch (*value) {
    case '\t':
    case ' ':
      continue;
    default:
      return false;
    }
  }
  return true;
}

void copy_url_component(std::string &dest, const urlparse_url *u, int field,
                        const char *url) {
  if (u->field_set & (1 << field)) {
    dest.assign(url + u->field_data[field].off, u->field_data[field].len);
  }
}

Headers::value_type to_header(const StringRef &name, const StringRef &value,
                              bool no_index, int32_t token) {
  return Header(std::string{std::begin(name), std::end(name)},
                std::string{std::begin(value), std::end(value)}, no_index,
                token);
}

void add_header(Headers &nva, const StringRef &name, const StringRef &value,
                bool no_index, int32_t token) {
  nva.push_back(to_header(name, value, no_index, token));
}

const Headers::value_type *get_header(const Headers &nva, const char *name) {
  const Headers::value_type *res = nullptr;
  for (auto &nv : nva) {
    if (nv.name == name) {
      res = &nv;
    }
  }
  return res;
}

bool non_empty_value(const HeaderRefs::value_type *nv) {
  return nv && !nv->value.empty();
}

namespace {
void copy_headers_to_nva_internal(std::vector<nghttp2_nv> &nva,
                                  const HeaderRefs &headers, uint8_t nv_flags,
                                  uint32_t flags) {
  auto it_forwarded = std::end(headers);
  auto it_xff = std::end(headers);
  auto it_xfp = std::end(headers);
  auto it_via = std::end(headers);

  for (auto it = std::begin(headers); it != std::end(headers); ++it) {
    auto kv = &(*it);
    if (kv->name.empty() || kv->name[0] == ':') {
      continue;
    }
    switch (kv->token) {
    case HD_COOKIE:
    case HD_CONNECTION:
    case HD_HOST:
    case HD_HTTP2_SETTINGS:
    case HD_KEEP_ALIVE:
    case HD_PROXY_CONNECTION:
    case HD_SERVER:
    case HD_TE:
    case HD_TRANSFER_ENCODING:
    case HD_UPGRADE:
      continue;
    case HD_EARLY_DATA:
      if (flags & HDOP_STRIP_EARLY_DATA) {
        continue;
      }
      break;
    case HD_SEC_WEBSOCKET_ACCEPT:
      if (flags & HDOP_STRIP_SEC_WEBSOCKET_ACCEPT) {
        continue;
      }
      break;
    case HD_SEC_WEBSOCKET_KEY:
      if (flags & HDOP_STRIP_SEC_WEBSOCKET_KEY) {
        continue;
      }
      break;
    case HD_FORWARDED:
      if (flags & HDOP_STRIP_FORWARDED) {
        continue;
      }

      if (it_forwarded == std::end(headers)) {
        it_forwarded = it;
        continue;
      }

      kv = &(*it_forwarded);
      it_forwarded = it;
      break;
    case HD_X_FORWARDED_FOR:
      if (flags & HDOP_STRIP_X_FORWARDED_FOR) {
        continue;
      }

      if (it_xff == std::end(headers)) {
        it_xff = it;
        continue;
      }

      kv = &(*it_xff);
      it_xff = it;
      break;
    case HD_X_FORWARDED_PROTO:
      if (flags & HDOP_STRIP_X_FORWARDED_PROTO) {
        continue;
      }

      if (it_xfp == std::end(headers)) {
        it_xfp = it;
        continue;
      }

      kv = &(*it_xfp);
      it_xfp = it;
      break;
    case HD_VIA:
      if (flags & HDOP_STRIP_VIA) {
        continue;
      }

      if (it_via == std::end(headers)) {
        it_via = it;
        continue;
      }

      kv = &(*it_via);
      it_via = it;
      break;
    }
    nva.push_back(
      make_field_flags(kv->name, kv->value, nv_flags | no_index(kv->no_index)));
  }
}
} // namespace

void copy_headers_to_nva(std::vector<nghttp2_nv> &nva,
                         const HeaderRefs &headers, uint32_t flags) {
  copy_headers_to_nva_internal(nva, headers, NGHTTP2_NV_FLAG_NONE, flags);
}

void copy_headers_to_nva_nocopy(std::vector<nghttp2_nv> &nva,
                                const HeaderRefs &headers, uint32_t flags) {
  copy_headers_to_nva_internal(
    nva, headers, NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE,
    flags);
}

void build_http1_headers_from_headers(DefaultMemchunks *buf,
                                      const HeaderRefs &headers,
                                      uint32_t flags) {
  auto it_forwarded = std::end(headers);
  auto it_xff = std::end(headers);
  auto it_xfp = std::end(headers);
  auto it_via = std::end(headers);

  for (auto it = std::begin(headers); it != std::end(headers); ++it) {
    auto kv = &(*it);
    if (kv->name.empty() || kv->name[0] == ':') {
      continue;
    }
    switch (kv->token) {
    case HD_CONNECTION:
    case HD_COOKIE:
    case HD_HOST:
    case HD_HTTP2_SETTINGS:
    case HD_KEEP_ALIVE:
    case HD_PROXY_CONNECTION:
    case HD_SERVER:
    case HD_UPGRADE:
      continue;
    case HD_EARLY_DATA:
      if (flags & HDOP_STRIP_EARLY_DATA) {
        continue;
      }
      break;
    case HD_TRANSFER_ENCODING:
      if (flags & HDOP_STRIP_TRANSFER_ENCODING) {
        continue;
      }
      break;
    case HD_FORWARDED:
      if (flags & HDOP_STRIP_FORWARDED) {
        continue;
      }

      if (it_forwarded == std::end(headers)) {
        it_forwarded = it;
        continue;
      }

      kv = &(*it_forwarded);
      it_forwarded = it;
      break;
    case HD_X_FORWARDED_FOR:
      if (flags & HDOP_STRIP_X_FORWARDED_FOR) {
        continue;
      }

      if (it_xff == std::end(headers)) {
        it_xff = it;
        continue;
      }

      kv = &(*it_xff);
      it_xff = it;
      break;
    case HD_X_FORWARDED_PROTO:
      if (flags & HDOP_STRIP_X_FORWARDED_PROTO) {
        continue;
      }

      if (it_xfp == std::end(headers)) {
        it_xfp = it;
        continue;
      }

      kv = &(*it_xfp);
      it_xfp = it;
      break;
    case HD_VIA:
      if (flags & HDOP_STRIP_VIA) {
        continue;
      }

      if (it_via == std::end(headers)) {
        it_via = it;
        continue;
      }

      kv = &(*it_via);
      it_via = it;
      break;
    }
    capitalize(buf, kv->name);
    buf->append(": ");
    buf->append(kv->value);
    buf->append("\r\n");
  }
}

int32_t determine_window_update_transmission(nghttp2_session *session,
                                             int32_t stream_id) {
  int32_t recv_length, window_size;
  if (stream_id == 0) {
    recv_length = nghttp2_session_get_effective_recv_data_length(session);
    window_size = nghttp2_session_get_effective_local_window_size(session);
  } else {
    recv_length =
      nghttp2_session_get_stream_effective_recv_data_length(session, stream_id);
    window_size = nghttp2_session_get_stream_effective_local_window_size(
      session, stream_id);
  }
  if (recv_length != -1 && window_size != -1) {
    if (recv_length >= window_size / 2) {
      return recv_length;
    }
  }
  return -1;
}

void dump_nv(FILE *out, const char **nv) {
  for (size_t i = 0; nv[i]; i += 2) {
    fprintf(out, "%s: %s\n", nv[i], nv[i + 1]);
  }
  fputc('\n', out);
  fflush(out);
}

void dump_nv(FILE *out, const nghttp2_nv *nva, size_t nvlen) {
  auto end = nva + nvlen;
  for (; nva != end; ++nva) {
    fprintf(out, "%s: %s\n", nva->name, nva->value);
  }
  fputc('\n', out);
  fflush(out);
}

void dump_nv(FILE *out, const Headers &nva) {
  for (auto &nv : nva) {
    fprintf(out, "%s: %s\n", nv.name.c_str(), nv.value.c_str());
  }
  fputc('\n', out);
  fflush(out);
}

void dump_nv(FILE *out, const HeaderRefs &nva) {
  for (auto &nv : nva) {
    fprintf(out, "%s: %s\n", nv.name.data(), nv.value.data());
  }
  fputc('\n', out);
  fflush(out);
}

void erase_header(HeaderRef *hd) {
  hd->name = StringRef{};
  hd->token = -1;
}

StringRef rewrite_location_uri(BlockAllocator &balloc, const StringRef &uri,
                               const urlparse_url &u,
                               const StringRef &match_host,
                               const StringRef &request_authority,
                               const StringRef &upstream_scheme) {
  // We just rewrite scheme and authority.
  if ((u.field_set & (1 << URLPARSE_HOST)) == 0) {
    return StringRef{};
  }
  auto field = &u.field_data[URLPARSE_HOST];
  if (!util::starts_with(std::begin(match_host), std::end(match_host),
                         &uri[field->off], &uri[field->off] + field->len) ||
      (match_host.size() != field->len && match_host[field->len] != ':')) {
    return StringRef{};
  }

  auto len = 0;
  if (!request_authority.empty()) {
    len += upstream_scheme.size() + str_size("://") + request_authority.size();
  }

  if (u.field_set & (1 << URLPARSE_PATH)) {
    field = &u.field_data[URLPARSE_PATH];
    len += field->len;
  }

  if (u.field_set & (1 << URLPARSE_QUERY)) {
    field = &u.field_data[URLPARSE_QUERY];
    len += 1 + field->len;
  }

  if (u.field_set & (1 << URLPARSE_FRAGMENT)) {
    field = &u.field_data[URLPARSE_FRAGMENT];
    len += 1 + field->len;
  }

  auto iov = make_byte_ref(balloc, len + 1);
  auto p = std::begin(iov);

  if (!request_authority.empty()) {
    p = std::copy(std::begin(upstream_scheme), std::end(upstream_scheme), p);
    p = util::copy_lit(p, "://");
    p =
      std::copy(std::begin(request_authority), std::end(request_authority), p);
  }
  if (u.field_set & (1 << URLPARSE_PATH)) {
    field = &u.field_data[URLPARSE_PATH];
    p = std::copy_n(&uri[field->off], field->len, p);
  }
  if (u.field_set & (1 << URLPARSE_QUERY)) {
    field = &u.field_data[URLPARSE_QUERY];
    *p++ = '?';
    p = std::copy_n(&uri[field->off], field->len, p);
  }
  if (u.field_set & (1 << URLPARSE_FRAGMENT)) {
    field = &u.field_data[URLPARSE_FRAGMENT];
    *p++ = '#';
    p = std::copy_n(&uri[field->off], field->len, p);
  }

  *p = '\0';

  return StringRef{std::span{std::begin(iov), p}};
}

int parse_http_status_code(const StringRef &src) {
  if (src.size() != 3) {
    return -1;
  }

  int status = 0;
  for (auto c : src) {
    if (!isdigit(c)) {
      return -1;
    }
    status *= 10;
    status += c - '0';
  }

  if (status < 100) {
    return -1;
  }

  return status;
}

// This function was generated by genheaderfunc.py.  Inspired by h2o
// header lookup.  https://github.com/h2o/h2o
int lookup_token(const StringRef &name) {
  switch (name.size()) {
  case 2:
    switch (name[1]) {
    case 'e':
      if (util::streq("t"_sr, name, 1)) {
        return HD_TE;
      }
      break;
    }
    break;
  case 3:
    switch (name[2]) {
    case 'a':
      if (util::streq("vi"_sr, name, 2)) {
        return HD_VIA;
      }
      break;
    }
    break;
  case 4:
    switch (name[3]) {
    case 'e':
      if (util::streq("dat"_sr, name, 3)) {
        return HD_DATE;
      }
      break;
    case 'k':
      if (util::streq("lin"_sr, name, 3)) {
        return HD_LINK;
      }
      break;
    case 't':
      if (util::streq("hos"_sr, name, 3)) {
        return HD_HOST;
      }
      break;
    }
    break;
  case 5:
    switch (name[4]) {
    case 'h':
      if (util::streq(":pat"_sr, name, 4)) {
        return HD__PATH;
      }
      break;
    case 't':
      if (util::streq(":hos"_sr, name, 4)) {
        return HD__HOST;
      }
      break;
    }
    break;
  case 6:
    switch (name[5]) {
    case 'e':
      if (util::streq("cooki"_sr, name, 5)) {
        return HD_COOKIE;
      }
      break;
    case 'r':
      if (util::streq("serve"_sr, name, 5)) {
        return HD_SERVER;
      }
      break;
    case 't':
      if (util::streq("expec"_sr, name, 5)) {
        return HD_EXPECT;
      }
      break;
    }
    break;
  case 7:
    switch (name[6]) {
    case 'c':
      if (util::streq("alt-sv"_sr, name, 6)) {
        return HD_ALT_SVC;
      }
      break;
    case 'd':
      if (util::streq(":metho"_sr, name, 6)) {
        return HD__METHOD;
      }
      break;
    case 'e':
      if (util::streq(":schem"_sr, name, 6)) {
        return HD__SCHEME;
      }
      if (util::streq("upgrad"_sr, name, 6)) {
        return HD_UPGRADE;
      }
      break;
    case 'r':
      if (util::streq("traile"_sr, name, 6)) {
        return HD_TRAILER;
      }
      break;
    case 's':
      if (util::streq(":statu"_sr, name, 6)) {
        return HD__STATUS;
      }
      break;
    }
    break;
  case 8:
    switch (name[7]) {
    case 'n':
      if (util::streq("locatio"_sr, name, 7)) {
        return HD_LOCATION;
      }
      break;
    case 'y':
      if (util::streq("priorit"_sr, name, 7)) {
        return HD_PRIORITY;
      }
      break;
    }
    break;
  case 9:
    switch (name[8]) {
    case 'd':
      if (util::streq("forwarde"_sr, name, 8)) {
        return HD_FORWARDED;
      }
      break;
    case 'l':
      if (util::streq(":protoco"_sr, name, 8)) {
        return HD__PROTOCOL;
      }
      break;
    }
    break;
  case 10:
    switch (name[9]) {
    case 'a':
      if (util::streq("early-dat"_sr, name, 9)) {
        return HD_EARLY_DATA;
      }
      break;
    case 'e':
      if (util::streq("keep-aliv"_sr, name, 9)) {
        return HD_KEEP_ALIVE;
      }
      break;
    case 'n':
      if (util::streq("connectio"_sr, name, 9)) {
        return HD_CONNECTION;
      }
      break;
    case 't':
      if (util::streq("user-agen"_sr, name, 9)) {
        return HD_USER_AGENT;
      }
      break;
    case 'y':
      if (util::streq(":authorit"_sr, name, 9)) {
        return HD__AUTHORITY;
      }
      break;
    }
    break;
  case 12:
    switch (name[11]) {
    case 'e':
      if (util::streq("content-typ"_sr, name, 11)) {
        return HD_CONTENT_TYPE;
      }
      break;
    }
    break;
  case 13:
    switch (name[12]) {
    case 'l':
      if (util::streq("cache-contro"_sr, name, 12)) {
        return HD_CACHE_CONTROL;
      }
      break;
    }
    break;
  case 14:
    switch (name[13]) {
    case 'h':
      if (util::streq("content-lengt"_sr, name, 13)) {
        return HD_CONTENT_LENGTH;
      }
      break;
    case 's':
      if (util::streq("http2-setting"_sr, name, 13)) {
        return HD_HTTP2_SETTINGS;
      }
      break;
    }
    break;
  case 15:
    switch (name[14]) {
    case 'e':
      if (util::streq("accept-languag"_sr, name, 14)) {
        return HD_ACCEPT_LANGUAGE;
      }
      break;
    case 'g':
      if (util::streq("accept-encodin"_sr, name, 14)) {
        return HD_ACCEPT_ENCODING;
      }
      break;
    case 'r':
      if (util::streq("x-forwarded-fo"_sr, name, 14)) {
        return HD_X_FORWARDED_FOR;
      }
      break;
    }
    break;
  case 16:
    switch (name[15]) {
    case 'n':
      if (util::streq("proxy-connectio"_sr, name, 15)) {
        return HD_PROXY_CONNECTION;
      }
      break;
    }
    break;
  case 17:
    switch (name[16]) {
    case 'e':
      if (util::streq("if-modified-sinc"_sr, name, 16)) {
        return HD_IF_MODIFIED_SINCE;
      }
      break;
    case 'g':
      if (util::streq("transfer-encodin"_sr, name, 16)) {
        return HD_TRANSFER_ENCODING;
      }
      break;
    case 'o':
      if (util::streq("x-forwarded-prot"_sr, name, 16)) {
        return HD_X_FORWARDED_PROTO;
      }
      break;
    case 'y':
      if (util::streq("sec-websocket-ke"_sr, name, 16)) {
        return HD_SEC_WEBSOCKET_KEY;
      }
      break;
    }
    break;
  case 20:
    switch (name[19]) {
    case 't':
      if (util::streq("sec-websocket-accep"_sr, name, 19)) {
        return HD_SEC_WEBSOCKET_ACCEPT;
      }
      break;
    }
    break;
  }
  return -1;
}

void init_hdidx(HeaderIndex &hdidx) {
  std::fill(std::begin(hdidx), std::end(hdidx), -1);
}

void index_header(HeaderIndex &hdidx, int32_t token, size_t idx) {
  if (token == -1) {
    return;
  }
  assert(token < HD_MAXIDX);
  hdidx[token] = idx;
}

const Headers::value_type *get_header(const HeaderIndex &hdidx, int32_t token,
                                      const Headers &nva) {
  auto i = hdidx[token];
  if (i == -1) {
    return nullptr;
  }
  return &nva[i];
}

Headers::value_type *get_header(const HeaderIndex &hdidx, int32_t token,
                                Headers &nva) {
  auto i = hdidx[token];
  if (i == -1) {
    return nullptr;
  }
  return &nva[i];
}

namespace {
template <typename InputIt> InputIt skip_lws(InputIt first, InputIt last) {
  for (; first != last; ++first) {
    switch (*first) {
    case ' ':
    case '\t':
      continue;
    default:
      return first;
    }
  }
  return first;
}
} // namespace

namespace {
template <typename InputIt>
InputIt skip_to_next_field(InputIt first, InputIt last) {
  for (; first != last; ++first) {
    switch (*first) {
    case ' ':
    case '\t':
    case ',':
      continue;
    default:
      return first;
    }
  }
  return first;
}
} // namespace

namespace {
// Skip to the right dquote ('"'), handling backslash escapes.
// Returns |last| if input is not terminated with '"'.
template <typename InputIt>
InputIt skip_to_right_dquote(InputIt first, InputIt last) {
  for (; first != last;) {
    switch (*first) {
    case '"':
      return first;
      // quoted-pair
    case '\\':
      ++first;
      if (first == last) {
        return first;
      }

      switch (*first) {
      case '\t':
      case ' ':
        break;
      default:
        if ((0x21 <= *first && *first <= 0x7e) /* VCHAR */ ||
            0x80 <= *first /* obs-text */) {
          break;
        }

        return last;
      }

      break;
      // qdtext
    case '\t':
    case ' ':
    case '!':
      break;
    default:
      if ((0x23 <= *first && *first <= 0x5b) ||
          (0x5d <= *first && *first <= 0x7e)) {
        break;
      }

      return last;
    }
    ++first;
  }
  return first;
}
} // namespace

namespace {
// Returns true if link-param does not match pattern |pat| of length
// |patlen| or it has empty value ("").  |pat| should be parmname
// followed by "=".
bool check_link_param_empty(const char *first, const char *last,
                            const char *pat, size_t patlen) {
  if (first + patlen <= last) {
    if (std::equal(pat, pat + patlen, first, util::CaseCmp())) {
      // we only accept URI if pat is followed by "" (e.g.,
      // loadpolicy="") here.
      if (first + patlen + 2 <= last) {
        if (*(first + patlen) != '"' || *(first + patlen + 1) != '"') {
          return false;
        }
      } else {
        // here we got invalid production (anchor=") or anchor=?
        return false;
      }
    }
  }
  return true;
}
} // namespace

namespace {
// Returns true if link-param consists of only parmname, and it
// matches string [pat, pat + patlen).
bool check_link_param_without_value(const char *first, const char *last,
                                    const char *pat, size_t patlen) {
  if (first + patlen > last) {
    return false;
  }

  if (first + patlen == last) {
    return std::equal(pat, pat + patlen, first, util::CaseCmp());
  }

  switch (*(first + patlen)) {
  case ';':
  case ',':
    return std::equal(pat, pat + patlen, first, util::CaseCmp());
  }

  return false;
}
} // namespace

namespace {
std::pair<LinkHeader, const char *>
parse_next_link_header_once(const char *first, const char *last) {
  first = skip_to_next_field(first, last);
  if (first == last || *first != '<') {
    return {{StringRef{}}, last};
  }
  auto url_first = ++first;
  first = std::find(first, last, '>');
  if (first == last) {
    return {{StringRef{}}, first};
  }
  auto url_last = first++;
  if (first == last) {
    return {{StringRef{}}, first};
  }
  // we expect ';' or ',' here
  switch (*first) {
  case ',':
    return {{StringRef{}}, ++first};
  case ';':
    ++first;
    break;
  default:
    return {{StringRef{}}, last};
  }

  auto ok = false;
  auto ign = false;
  for (;;) {
    first = skip_lws(first, last);
    if (first == last) {
      return {{StringRef{}}, first};
    }
    // we expect link-param

    if (!ign) {
      if (!ok) {
        // rel can take several relations using quoted form.
        static constexpr char PLP[] = "rel=\"";
        static constexpr size_t PLPLEN = str_size(PLP);

        static constexpr char PLT[] = "preload";
        static constexpr size_t PLTLEN = str_size(PLT);
        if (first + PLPLEN < last && *(first + PLPLEN - 1) == '"' &&
            std::equal(PLP, PLP + PLPLEN, first, util::CaseCmp())) {
          // we have to search preload in whitespace separated list:
          // rel="preload something http://example.org/foo"
          first += PLPLEN;
          auto start = first;
          for (; first != last;) {
            if (*first != ' ' && *first != '"') {
              ++first;
              continue;
            }

            if (start == first) {
              return {{StringRef{}}, last};
            }

            if (!ok && start + PLTLEN == first &&
                std::equal(PLT, PLT + PLTLEN, start, util::CaseCmp())) {
              ok = true;
            }

            if (*first == '"') {
              break;
            }
            first = skip_lws(first, last);
            start = first;
          }
          if (first == last) {
            return {{StringRef{}}, last};
          }
          assert(*first == '"');
          ++first;
          if (first == last || *first == ',') {
            goto almost_done;
          }
          if (*first == ';') {
            ++first;
            // parse next link-param
            continue;
          }
          return {{StringRef{}}, last};
        }
      }
      // we are only interested in rel=preload parameter.  Others are
      // simply skipped.
      static constexpr char PL[] = "rel=preload";
      static constexpr size_t PLLEN = str_size(PL);
      if (first + PLLEN == last) {
        if (std::equal(PL, PL + PLLEN, first, util::CaseCmp())) {
          // ok = true;
          // this is the end of sequence
          return {{{url_first, url_last}}, last};
        }
      } else if (first + PLLEN + 1 <= last) {
        switch (*(first + PLLEN)) {
        case ',':
          if (!std::equal(PL, PL + PLLEN, first, util::CaseCmp())) {
            break;
          }
          // ok = true;
          // skip including ','
          first += PLLEN + 1;
          return {{{url_first, url_last}}, first};
        case ';':
          if (!std::equal(PL, PL + PLLEN, first, util::CaseCmp())) {
            break;
          }
          ok = true;
          // skip including ';'
          first += PLLEN + 1;
          // continue parse next link-param
          continue;
        }
      }
      // we have to reject URI if we have nonempty anchor parameter.
      static constexpr char ANCHOR[] = "anchor=";
      static constexpr size_t ANCHORLEN = str_size(ANCHOR);
      if (!ign && !check_link_param_empty(first, last, ANCHOR, ANCHORLEN)) {
        ign = true;
      }

      // reject URI if we have non-empty loadpolicy.  This could be
      // tightened up to just pick up "next" or "insert".
      static constexpr char LOADPOLICY[] = "loadpolicy=";
      static constexpr size_t LOADPOLICYLEN = str_size(LOADPOLICY);
      if (!ign &&
          !check_link_param_empty(first, last, LOADPOLICY, LOADPOLICYLEN)) {
        ign = true;
      }

      // reject URI if we have nopush attribute.
      static constexpr char NOPUSH[] = "nopush";
      static constexpr size_t NOPUSHLEN = str_size(NOPUSH);
      if (!ign &&
          check_link_param_without_value(first, last, NOPUSH, NOPUSHLEN)) {
        ign = true;
      }
    }

    auto param_first = first;
    for (; first != last;) {
      if (util::in_attr_char(*first)) {
        ++first;
        continue;
      }
      // '*' is only allowed at the end of parameter name and must be
      // followed by '='
      if (last - first >= 2 && first != param_first) {
        if (*first == '*' && *(first + 1) == '=') {
          ++first;
          break;
        }
      }
      if (*first == '=' || *first == ';' || *first == ',') {
        break;
      }
      return {{StringRef{}}, last};
    }
    if (param_first == first) {
      // empty parmname
      return {{StringRef{}}, last};
    }
    // link-param without value is acceptable (see link-extension) if
    // it is not followed by '='
    if (first == last || *first == ',') {
      goto almost_done;
    }
    if (*first == ';') {
      ++first;
      // parse next link-param
      continue;
    }
    // now parsing link-param value
    assert(*first == '=');
    ++first;
    if (first == last) {
      // empty value is not acceptable
      return {{StringRef{}}, first};
    }
    if (*first == '"') {
      // quoted-string
      first = skip_to_right_dquote(first + 1, last);
      if (first == last) {
        return {{StringRef{}}, first};
      }
      ++first;
      if (first == last || *first == ',') {
        goto almost_done;
      }
      if (*first == ';') {
        ++first;
        // parse next link-param
        continue;
      }
      return {{StringRef{}}, last};
    }
    // not quoted-string, skip to next ',' or ';'
    if (*first == ',' || *first == ';') {
      // empty value
      return {{StringRef{}}, last};
    }
    for (; first != last; ++first) {
      if (*first == ',' || *first == ';') {
        break;
      }
    }
    if (first == last || *first == ',') {
      goto almost_done;
    }
    assert(*first == ';');
    ++first;
    // parse next link-param
  }

almost_done:
  assert(first == last || *first == ',');

  if (first != last) {
    ++first;
  }
  if (ok && !ign) {
    return {{{url_first, url_last}}, first};
  }
  return {{StringRef{}}, first};
}
} // namespace

std::vector<LinkHeader> parse_link_header(const StringRef &src) {
  std::vector<LinkHeader> res;
  for (auto first = std::begin(src); first != std::end(src);) {
    auto rv = parse_next_link_header_once(first, std::end(src));
    first = rv.second;
    auto &link = rv.first;
    if (!link.uri.empty()) {
      res.push_back(link);
    }
  }
  return res;
}

std::string path_join(const StringRef &base_path, const StringRef &base_query,
                      const StringRef &rel_path, const StringRef &rel_query) {
  BlockAllocator balloc(1024, 1024);

  return std::string{
    path_join(balloc, base_path, base_query, rel_path, rel_query)};
}

bool expect_response_body(int status_code) {
  return status_code == 101 ||
         (status_code / 100 != 1 && status_code != 304 && status_code != 204);
}

bool expect_response_body(const std::string &method, int status_code) {
  return method != "HEAD" && expect_response_body(status_code);
}

bool expect_response_body(int method_token, int status_code) {
  return method_token != HTTP_HEAD && expect_response_body(status_code);
}

// This function was generated by genmethodfunc.py.
int lookup_method_token(const StringRef &name) {
  switch (name.size()) {
  case 3:
    switch (name[2]) {
    case 'L':
      if (util::streq("AC"_sr, name, 2)) {
        return HTTP_ACL;
      }
      break;
    case 'T':
      if (util::streq("GE"_sr, name, 2)) {
        return HTTP_GET;
      }
      if (util::streq("PU"_sr, name, 2)) {
        return HTTP_PUT;
      }
      break;
    }
    break;
  case 4:
    switch (name[3]) {
    case 'D':
      if (util::streq("BIN"_sr, name, 3)) {
        return HTTP_BIND;
      }
      if (util::streq("HEA"_sr, name, 3)) {
        return HTTP_HEAD;
      }
      break;
    case 'E':
      if (util::streq("MOV"_sr, name, 3)) {
        return HTTP_MOVE;
      }
      break;
    case 'K':
      if (util::streq("LIN"_sr, name, 3)) {
        return HTTP_LINK;
      }
      if (util::streq("LOC"_sr, name, 3)) {
        return HTTP_LOCK;
      }
      break;
    case 'T':
      if (util::streq("POS"_sr, name, 3)) {
        return HTTP_POST;
      }
      break;
    case 'Y':
      if (util::streq("COP"_sr, name, 3)) {
        return HTTP_COPY;
      }
      break;
    }
    break;
  case 5:
    switch (name[4]) {
    case 'E':
      if (util::streq("MERG"_sr, name, 4)) {
        return HTTP_MERGE;
      }
      if (util::streq("PURG"_sr, name, 4)) {
        return HTTP_PURGE;
      }
      if (util::streq("TRAC"_sr, name, 4)) {
        return HTTP_TRACE;
      }
      break;
    case 'H':
      if (util::streq("PATC"_sr, name, 4)) {
        return HTTP_PATCH;
      }
      break;
    case 'L':
      if (util::streq("MKCO"_sr, name, 4)) {
        return HTTP_MKCOL;
      }
      break;
    }
    break;
  case 6:
    switch (name[5]) {
    case 'D':
      if (util::streq("REBIN"_sr, name, 5)) {
        return HTTP_REBIND;
      }
      if (util::streq("UNBIN"_sr, name, 5)) {
        return HTTP_UNBIND;
      }
      break;
    case 'E':
      if (util::streq("DELET"_sr, name, 5)) {
        return HTTP_DELETE;
      }
      if (util::streq("SOURC"_sr, name, 5)) {
        return HTTP_SOURCE;
      }
      break;
    case 'H':
      if (util::streq("SEARC"_sr, name, 5)) {
        return HTTP_SEARCH;
      }
      break;
    case 'K':
      if (util::streq("UNLIN"_sr, name, 5)) {
        return HTTP_UNLINK;
      }
      if (util::streq("UNLOC"_sr, name, 5)) {
        return HTTP_UNLOCK;
      }
      break;
    case 'T':
      if (util::streq("REPOR"_sr, name, 5)) {
        return HTTP_REPORT;
      }
      break;
    case 'Y':
      if (util::streq("NOTIF"_sr, name, 5)) {
        return HTTP_NOTIFY;
      }
      break;
    }
    break;
  case 7:
    switch (name[6]) {
    case 'H':
      if (util::streq("MSEARC"_sr, name, 6)) {
        return HTTP_MSEARCH;
      }
      break;
    case 'S':
      if (util::streq("OPTION"_sr, name, 6)) {
        return HTTP_OPTIONS;
      }
      break;
    case 'T':
      if (util::streq("CONNEC"_sr, name, 6)) {
        return HTTP_CONNECT;
      }
      break;
    }
    break;
  case 8:
    switch (name[7]) {
    case 'D':
      if (util::streq("PROPFIN"_sr, name, 7)) {
        return HTTP_PROPFIND;
      }
      break;
    case 'T':
      if (util::streq("CHECKOU"_sr, name, 7)) {
        return HTTP_CHECKOUT;
      }
      break;
    }
    break;
  case 9:
    switch (name[8]) {
    case 'E':
      if (util::streq("SUBSCRIB"_sr, name, 8)) {
        return HTTP_SUBSCRIBE;
      }
      break;
    case 'H':
      if (util::streq("PROPPATC"_sr, name, 8)) {
        return HTTP_PROPPATCH;
      }
      break;
    }
    break;
  case 10:
    switch (name[9]) {
    case 'R':
      if (util::streq("MKCALENDA"_sr, name, 9)) {
        return HTTP_MKCALENDAR;
      }
      break;
    case 'Y':
      if (util::streq("MKACTIVIT"_sr, name, 9)) {
        return HTTP_MKACTIVITY;
      }
      break;
    }
    break;
  case 11:
    switch (name[10]) {
    case 'E':
      if (util::streq("UNSUBSCRIB"_sr, name, 10)) {
        return HTTP_UNSUBSCRIBE;
      }
      break;
    }
    break;
  }
  return -1;
}

StringRef to_method_string(int method_token) {
  // we happened to use same value for method with llhttp.
  return StringRef{
    llhttp_method_name(static_cast<llhttp_method>(method_token))};
}

StringRef get_pure_path_component(const StringRef &uri) {
  int rv;

  urlparse_url u;
  rv = urlparse_parse_url(uri.data(), uri.size(), 0, &u);
  if (rv != 0) {
    return StringRef{};
  }

  if (u.field_set & (1 << URLPARSE_PATH)) {
    auto &f = u.field_data[URLPARSE_PATH];
    return StringRef{uri.data() + f.off, f.len};
  }

  return "/"_sr;
}

int construct_push_component(BlockAllocator &balloc, StringRef &scheme,
                             StringRef &authority, StringRef &path,
                             const StringRef &base, const StringRef &uri) {
  int rv;
  StringRef rel, relq;

  if (uri.size() == 0) {
    return -1;
  }

  urlparse_url u;

  rv = urlparse_parse_url(uri.data(), uri.size(), 0, &u);

  if (rv != 0) {
    if (uri[0] == '/') {
      return -1;
    }

    // treat link_url as relative URI.
    auto end = std::find(std::begin(uri), std::end(uri), '#');
    auto q = std::find(std::begin(uri), end, '?');

    rel = StringRef{std::begin(uri), q};
    if (q != end) {
      relq = StringRef{q + 1, std::end(uri)};
    }
  } else {
    if (u.field_set & (1 << URLPARSE_SCHEMA)) {
      scheme = util::get_uri_field(uri.data(), u, URLPARSE_SCHEMA);
    }

    if (u.field_set & (1 << URLPARSE_HOST)) {
      auto auth = util::get_uri_field(uri.data(), u, URLPARSE_HOST);
      auto len = auth.size();
      auto port_exists = u.field_set & (1 << URLPARSE_PORT);
      if (port_exists) {
        len += 1 + str_size("65535");
      }
      auto iov = make_byte_ref(balloc, len + 1);
      auto p = std::begin(iov);
      p = std::copy(std::begin(auth), std::end(auth), p);
      if (port_exists) {
        *p++ = ':';
        p = util::utos(p, u.port);
      }
      *p = '\0';

      authority = StringRef{std::span{std::begin(iov), p}};
    }

    if (u.field_set & (1 << URLPARSE_PATH)) {
      auto &f = u.field_data[URLPARSE_PATH];
      rel = StringRef{uri.data() + f.off, f.len};
    } else {
      rel = "/"_sr;
    }

    if (u.field_set & (1 << URLPARSE_QUERY)) {
      auto &f = u.field_data[URLPARSE_QUERY];
      relq = StringRef{uri.data() + f.off, f.len};
    }
  }

  path = path_join(balloc, base, StringRef{}, rel, relq);

  return 0;
}

namespace {
template <typename InputIt> InputIt eat_file(InputIt first, InputIt last) {
  if (first == last) {
    *first++ = '/';
    return first;
  }

  if (*(last - 1) == '/') {
    return last;
  }

  auto p = last;
  for (; p != first && *(p - 1) != '/'; --p)
    ;
  if (p == first) {
    // this should not happened in normal case, where we expect path
    // starts with '/'
    *first++ = '/';
    return first;
  }

  return p;
}
} // namespace

namespace {
template <typename InputIt> InputIt eat_dir(InputIt first, InputIt last) {
  auto p = eat_file(first, last);

  --p;

  assert(*p == '/');

  return eat_file(first, p);
}
} // namespace

StringRef path_join(BlockAllocator &balloc, const StringRef &base_path,
                    const StringRef &base_query, const StringRef &rel_path,
                    const StringRef &rel_query) {
  auto res =
    make_byte_ref(balloc, std::max(static_cast<size_t>(1), base_path.size()) +
                            rel_path.size() + 1 +
                            std::max(base_query.size(), rel_query.size()) + 1);
  auto p = std::begin(res);

  if (rel_path.empty()) {
    if (base_path.empty()) {
      *p++ = '/';
    } else {
      p = std::copy(std::begin(base_path), std::end(base_path), p);
    }
    if (rel_query.empty()) {
      if (!base_query.empty()) {
        *p++ = '?';
        p = std::copy(std::begin(base_query), std::end(base_query), p);
      }
      *p = '\0';
      return StringRef{std::span{std::begin(res), p}};
    }
    *p++ = '?';
    p = std::copy(std::begin(rel_query), std::end(rel_query), p);
    *p = '\0';
    return StringRef{std::span{std::begin(res), p}};
  }

  auto first = std::begin(rel_path);
  auto last = std::end(rel_path);

  if (rel_path[0] == '/') {
    *p++ = '/';
    ++first;
    for (; first != last && *first == '/'; ++first)
      ;
  } else if (base_path.empty()) {
    *p++ = '/';
  } else {
    p = std::copy(std::begin(base_path), std::end(base_path), p);
  }

  for (; first != last;) {
    if (*first == '.') {
      if (first + 1 == last) {
        if (*(p - 1) != '/') {
          p = eat_file(std::begin(res), p);
        }
        break;
      }
      if (*(first + 1) == '/') {
        if (*(p - 1) != '/') {
          p = eat_file(std::begin(res), p);
        }
        first += 2;
        continue;
      }
      if (*(first + 1) == '.') {
        if (first + 2 == last) {
          p = eat_dir(std::begin(res), p);
          break;
        }
        if (*(first + 2) == '/') {
          p = eat_dir(std::begin(res), p);
          first += 3;
          continue;
        }
      }
    }
    if (*(p - 1) != '/') {
      p = eat_file(std::begin(res), p);
    }
    auto slash = std::find(first, last, '/');
    if (slash == last) {
      p = std::copy(first, last, p);
      break;
    }
    p = std::copy(first, slash + 1, p);
    first = slash + 1;
    for (; first != last && *first == '/'; ++first)
      ;
  }
  if (!rel_query.empty()) {
    *p++ = '?';
    p = std::copy(std::begin(rel_query), std::end(rel_query), p);
  }
  *p = '\0';
  return StringRef{std::span{std::begin(res), p}};
}

StringRef normalize_path(BlockAllocator &balloc, const StringRef &path,
                         const StringRef &query) {
  // First, decode %XX for unreserved characters, then do
  // http2::path_join

  // We won't find %XX if length is less than 3.
  if (path.size() < 3 ||
      std::find(std::begin(path), std::end(path), '%') == std::end(path)) {
    return path_join(balloc, StringRef{}, StringRef{}, path, query);
  }

  // includes last terminal NULL.
  auto result = make_byte_ref(balloc, path.size() + 1);
  auto p = std::begin(result);

  auto it = std::begin(path);
  for (; it + 2 < std::end(path);) {
    if (*it == '%') {
      if (util::is_hex_digit(*(it + 1)) && util::is_hex_digit(*(it + 2))) {
        auto c =
          (util::hex_to_uint(*(it + 1)) << 4) + util::hex_to_uint(*(it + 2));
        if (util::in_rfc3986_unreserved_chars(c)) {
          *p++ = c;

          it += 3;

          continue;
        }
        *p++ = '%';
        *p++ = util::upcase(*(it + 1));
        *p++ = util::upcase(*(it + 2));

        it += 3;

        continue;
      }
    }
    *p++ = *it++;
  }

  p = std::copy(it, std::end(path), p);
  *p = '\0';

  return path_join(balloc, StringRef{}, StringRef{},
                   StringRef{std::span{std::begin(result), p}}, query);
}

StringRef normalize_path_colon(BlockAllocator &balloc, const StringRef &path,
                               const StringRef &query) {
  // First, decode %XX for unreserved characters and ':', then do
  // http2::path_join

  // We won't find %XX if length is less than 3.
  if (path.size() < 3 ||
      std::find(std::begin(path), std::end(path), '%') == std::end(path)) {
    return path_join(balloc, StringRef{}, StringRef{}, path, query);
  }

  // includes last terminal NULL.
  auto result = make_byte_ref(balloc, path.size() + 1);
  auto p = std::begin(result);

  auto it = std::begin(path);
  for (; it + 2 < std::end(path);) {
    if (*it == '%') {
      if (util::is_hex_digit(*(it + 1)) && util::is_hex_digit(*(it + 2))) {
        auto c =
          (util::hex_to_uint(*(it + 1)) << 4) + util::hex_to_uint(*(it + 2));
        if (util::in_rfc3986_unreserved_chars(c) || c == ':') {
          *p++ = c;

          it += 3;

          continue;
        }
        *p++ = '%';
        *p++ = util::upcase(*(it + 1));
        *p++ = util::upcase(*(it + 2));

        it += 3;

        continue;
      }
    }
    *p++ = *it++;
  }

  p = std::copy(it, std::end(path), p);
  *p = '\0';

  return path_join(balloc, StringRef{}, StringRef{},
                   StringRef{std::span{std::begin(result), p}}, query);
}

std::string normalize_path(const StringRef &path, const StringRef &query) {
  BlockAllocator balloc(1024, 1024);

  return std::string{normalize_path(balloc, path, query)};
}

StringRef rewrite_clean_path(BlockAllocator &balloc, const StringRef &src) {
  if (src.empty() || src[0] != '/') {
    return src;
  }
  // probably, not necessary most of the case, but just in case.
  auto fragment = std::find(std::begin(src), std::end(src), '#');
  auto raw_query = std::find(std::begin(src), fragment, '?');
  auto query = raw_query;
  if (query != fragment) {
    ++query;
  }
  return normalize_path(balloc, StringRef{std::begin(src), raw_query},
                        StringRef{query, fragment});
}

StringRef copy_lower(BlockAllocator &balloc, const StringRef &src) {
  auto iov = make_byte_ref(balloc, src.size() + 1);
  auto p = std::begin(iov);
  p = std::copy(std::begin(src), std::end(src), p);
  *p = '\0';
  util::inp_strlower(std::begin(iov), p);
  return StringRef{std::span{std::begin(iov), p}};
}

bool contains_trailers(const StringRef &s) {
  constexpr auto trailers = "trailers"_sr;

  for (auto p = std::begin(s), end = std::end(s);; ++p) {
    p = std::find_if(p, end, [](char c) { return c != ' ' && c != '\t'; });
    if (p == end || static_cast<size_t>(end - p) < trailers.size()) {
      return false;
    }
    if (util::strieq(trailers, StringRef{p, p + trailers.size()})) {
      // Make sure that there is no character other than white spaces
      // before next "," or end of string.
      p = std::find_if(p + trailers.size(), end,
                       [](char c) { return c != ' ' && c != '\t'; });
      if (p == end || *p == ',') {
        return true;
      }
    }
    // Skip to next ",".
    p = std::find_if(p, end, [](char c) { return c == ','; });
    if (p == end) {
      return false;
    }
  }
}

StringRef make_websocket_accept_token(uint8_t *dest, const StringRef &key) {
  static constexpr char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  std::array<char, base64::encode_length(16) + str_size(magic)> s;
  auto p = std::copy(std::begin(key), std::end(key), std::begin(s));
  std::copy_n(magic, str_size(magic), p);

  std::array<uint8_t, 20> h;
  if (util::sha1(h.data(), StringRef{s}) != 0) {
    return StringRef{};
  }

  auto end = base64::encode(std::begin(h), std::end(h), dest);
  return StringRef{std::span{dest, end}};
}

bool legacy_http1(int major, int minor) {
  return major <= 0 || (major == 1 && minor == 0);
}

bool check_transfer_encoding(const StringRef &s) {
  if (s.empty()) {
    return false;
  }

  auto it = std::begin(s);

  for (;;) {
    // token
    if (!util::in_token(*it)) {
      return false;
    }

    ++it;

    for (; it != std::end(s) && util::in_token(*it); ++it)
      ;

    if (it == std::end(s)) {
      return true;
    }

    for (;;) {
      // OWS
      it = skip_lws(it, std::end(s));
      if (it == std::end(s)) {
        return false;
      }

      if (*it == ',') {
        ++it;

        it = skip_lws(it, std::end(s));
        if (it == std::end(s)) {
          return false;
        }

        break;
      }

      if (*it != ';') {
        return false;
      }

      ++it;

      // transfer-parameter follows

      // OWS
      it = skip_lws(it, std::end(s));
      if (it == std::end(s)) {
        return false;
      }

      // token
      if (!util::in_token(*it)) {
        return false;
      }

      ++it;

      for (; it != std::end(s) && util::in_token(*it); ++it)
        ;

      if (it == std::end(s)) {
        return false;
      }

      // No BWS allowed
      if (*it != '=') {
        return false;
      }

      ++it;

      if (util::in_token(*it)) {
        // token
        ++it;

        for (; it != std::end(s) && util::in_token(*it); ++it)
          ;
      } else if (*it == '"') {
        // quoted-string
        ++it;

        it = skip_to_right_dquote(it, std::end(s));
        if (it == std::end(s)) {
          return false;
        }

        ++it;
      } else {
        return false;
      }

      if (it == std::end(s)) {
        return true;
      }
    }
  }
}

std::string encode_extpri(const nghttp2_extpri &extpri) {
  std::string res = "u=";

  res += extpri.urgency + '0';
  if (extpri.inc) {
    res += ",i";
  }

  return res;
}

} // namespace http2

} // namespace nghttp2
