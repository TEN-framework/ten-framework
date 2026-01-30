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
#ifndef HTTP2_H
#define HTTP2_H

#include "nghttp2_config.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <array>

#include <nghttp2/nghttp2.h>

#include "urlparse.h"

#include "util.h"
#include "memchunk.h"
#include "template.h"
#include "allocator.h"
#include "base64.h"

namespace nghttp2 {

struct Header {
  Header(std::string name, std::string value, bool no_index = false,
         int32_t token = -1)
    : name(std::move(name)),
      value(std::move(value)),
      token(token),
      no_index(no_index) {}

  Header() : token(-1), no_index(false) {}

  bool operator==(const Header &other) const {
    return name == other.name && value == other.value;
  }

  bool operator<(const Header &rhs) const {
    return name < rhs.name || (name == rhs.name && value < rhs.value);
  }

  std::string name;
  std::string value;
  int32_t token;
  bool no_index;
};

struct HeaderRef {
  HeaderRef(const StringRef &name, const StringRef &value,
            bool no_index = false, int32_t token = -1)
    : name(name), value(value), token(token), no_index(no_index) {}

  HeaderRef() : token(-1), no_index(false) {}

  bool operator==(const HeaderRef &other) const {
    return name == other.name && value == other.value;
  }

  bool operator<(const HeaderRef &rhs) const {
    return name < rhs.name || (name == rhs.name && value < rhs.value);
  }

  StringRef name;
  StringRef value;
  int32_t token;
  bool no_index;
};

using Headers = std::vector<Header>;
using HeaderRefs = std::vector<HeaderRef>;

namespace http2 {

// Returns reason-phrase for given |status code|.  If there is no
// known reason-phrase for the given code, returns empty string.
StringRef get_reason_phrase(unsigned int status_code);

// Returns string version of |status_code|. (e.g., "404")
StringRef stringify_status(BlockAllocator &balloc, unsigned int status_code);

void capitalize(DefaultMemchunks *buf, const StringRef &s);

// Returns true if |value| is LWS
bool lws(const char *value);

// Copies the |field| component value from |u| and |url| to the
// |dest|. If |u| does not have |field|, then this function does
// nothing.
void copy_url_component(std::string &dest, const urlparse_url *u, int field,
                        const char *url);

Headers::value_type to_header(const StringRef &name, const StringRef &value,
                              bool no_index, int32_t token);

// Add name/value pairs to |nva|.  If |no_index| is true, this
// name/value pair won't be indexed when it is forwarded to the next
// hop.
void add_header(Headers &nva, const StringRef &name, const StringRef &value,
                bool no_index, int32_t token);

// Returns pointer to the entry in |nva| which has name |name|.  If
// more than one entries which have the name |name|, last occurrence
// in |nva| is returned.  If no such entry exist, returns nullptr.
const Headers::value_type *get_header(const Headers &nva, const char *name);

// Returns true if the value of |nv| is not empty.
bool non_empty_value(const HeaderRefs::value_type *nv);

// Create nghttp2_nv from |name|, |value| and |flags|.
inline nghttp2_nv make_field_flags(const StringRef &name,
                                   const StringRef &value,
                                   uint8_t flags = NGHTTP2_NV_FLAG_NONE) {
  auto ns = as_uint8_span(std::span{name});
  auto vs = as_uint8_span(std::span{value});

  return {const_cast<uint8_t *>(ns.data()), const_cast<uint8_t *>(vs.data()),
          ns.size(), vs.size(), flags};
}

// Creates nghttp2_nv from |name|, |value| and |flags|.  nghttp2
// library does not copy them.
inline nghttp2_nv make_field(const StringRef &name, const StringRef &value,
                             uint8_t flags = NGHTTP2_NV_FLAG_NONE) {
  return make_field_flags(name, value,
                          static_cast<uint8_t>(NGHTTP2_NV_FLAG_NO_COPY_NAME |
                                               NGHTTP2_NV_FLAG_NO_COPY_VALUE |
                                               flags));
}

// Creates nghttp2_nv from |name|, |value| and |flags|.  nghttp2
// library copies |value| unless |flags| includes
// NGHTTP2_NV_FLAG_NO_COPY_VALUE.
inline nghttp2_nv make_field_v(const StringRef &name, const StringRef &value,
                               uint8_t flags = NGHTTP2_NV_FLAG_NONE) {
  return make_field_flags(
    name, value, static_cast<uint8_t>(NGHTTP2_NV_FLAG_NO_COPY_NAME | flags));
}

// Creates nghttp2_nv from |name|, |value| and |flags|.  nghttp2
// library copies |name| and |value| unless |flags| includes
// NGHTTP2_NV_FLAG_NO_COPY_NAME or NGHTTP2_NV_FLAG_NO_COPY_VALUE.
inline nghttp2_nv make_field_nv(const StringRef &name, const StringRef &value,
                                uint8_t flags = NGHTTP2_NV_FLAG_NONE) {
  return make_field_flags(name, value, flags);
}

// Returns NGHTTP2_NV_FLAG_NO_INDEX if |no_index| is true, otherwise
// NGHTTP2_NV_FLAG_NONE.
inline uint8_t no_index(bool no_index) {
  return no_index ? NGHTTP2_NV_FLAG_NO_INDEX : NGHTTP2_NV_FLAG_NONE;
}

enum HeaderBuildOp {
  HDOP_NONE,
  // Forwarded header fields must be stripped.  If this flag is not
  // set, all Forwarded header fields other than last one are added.
  HDOP_STRIP_FORWARDED = 1,
  // X-Forwarded-For header fields must be stripped.  If this flag is
  // not set, all X-Forwarded-For header fields other than last one
  // are added.
  HDOP_STRIP_X_FORWARDED_FOR = 1 << 1,
  // X-Forwarded-Proto header fields must be stripped.  If this flag
  // is not set, all X-Forwarded-Proto header fields other than last
  // one are added.
  HDOP_STRIP_X_FORWARDED_PROTO = 1 << 2,
  // Via header fields must be stripped.  If this flag is not set, all
  // Via header fields other than last one are added.
  HDOP_STRIP_VIA = 1 << 3,
  // Early-Data header fields must be stripped.  If this flag is not
  // set, all Early-Data header fields are added.
  HDOP_STRIP_EARLY_DATA = 1 << 4,
  // Strip above all header fields.
  HDOP_STRIP_ALL = HDOP_STRIP_FORWARDED | HDOP_STRIP_X_FORWARDED_FOR |
                   HDOP_STRIP_X_FORWARDED_PROTO | HDOP_STRIP_VIA |
                   HDOP_STRIP_EARLY_DATA,
  // Sec-WebSocket-Accept header field must be stripped.  If this flag
  // is not set, all Sec-WebSocket-Accept header fields are added.
  HDOP_STRIP_SEC_WEBSOCKET_ACCEPT = 1 << 5,
  // Sec-WebSocket-Key header field must be stripped.  If this flag is
  // not set, all Sec-WebSocket-Key header fields are added.
  HDOP_STRIP_SEC_WEBSOCKET_KEY = 1 << 6,
  // Transfer-Encoding header field must be stripped.  If this flag is
  // not set, all Transfer-Encoding header fields are added.
  HDOP_STRIP_TRANSFER_ENCODING = 1 << 7,
};

// Appends headers in |headers| to |nv|.  |headers| must be indexed
// before this call (its element's token field is assigned).  Certain
// headers, including disallowed headers in HTTP/2 spec and headers
// which require special handling (i.e. via), are not copied.  |flags|
// is one or more of HeaderBuildOp flags.  They tell function that
// certain header fields should not be added.
void copy_headers_to_nva(std::vector<nghttp2_nv> &nva,
                         const HeaderRefs &headers, uint32_t flags);

// Just like copy_headers_to_nva(), but this adds
// NGHTTP2_NV_FLAG_NO_COPY_NAME and NGHTTP2_NV_FLAG_NO_COPY_VALUE.
void copy_headers_to_nva_nocopy(std::vector<nghttp2_nv> &nva,
                                const HeaderRefs &headers, uint32_t flags);

// Appends HTTP/1.1 style header lines to |buf| from headers in
// |headers|.  |headers| must be indexed before this call (its
// element's token field is assigned).  Certain headers, which
// requires special handling (i.e. via and cookie), are not appended.
// |flags| is one or more of HeaderBuildOp flags.  They tell function
// that certain header fields should not be added.
void build_http1_headers_from_headers(DefaultMemchunks *buf,
                                      const HeaderRefs &headers,
                                      uint32_t flags);

// Return positive window_size_increment if WINDOW_UPDATE should be
// sent for the stream |stream_id|. If |stream_id| == 0, this function
// determines the necessity of the WINDOW_UPDATE for a connection.
//
// If the function determines WINDOW_UPDATE is not necessary at the
// moment, it returns -1.
int32_t determine_window_update_transmission(nghttp2_session *session,
                                             int32_t stream_id);

// Dumps name/value pairs in |nv| to |out|. The |nv| must be
// terminated by nullptr.
void dump_nv(FILE *out, const char **nv);

// Dumps name/value pairs in |nva| to |out|.
void dump_nv(FILE *out, const nghttp2_nv *nva, size_t nvlen);

// Dumps name/value pairs in |nva| to |out|.
void dump_nv(FILE *out, const Headers &nva);

void dump_nv(FILE *out, const HeaderRefs &nva);

// Ereases header in |hd|.
void erase_header(HeaderRef *hd);

// Rewrites redirection URI which usually appears in location header
// field. The |uri| is the URI in the location header field. The |u|
// stores the result of parsed |uri|. The |request_authority| is the
// host or :authority header field value in the request. The
// |upstream_scheme| is either "https" or "http" in the upstream
// interface.  Rewrite is done only if location header field value
// contains |match_host| as host excluding port.  The |match_host| and
// |request_authority| could be different.  If |request_authority| is
// empty, strip authority.
//
// This function returns the new rewritten URI on success. If the
// location URI is not subject to the rewrite, this function returns
// empty string.
StringRef rewrite_location_uri(BlockAllocator &balloc, const StringRef &uri,
                               const urlparse_url &u,
                               const StringRef &match_host,
                               const StringRef &request_authority,
                               const StringRef &upstream_scheme);

// Returns parsed HTTP status code.  Returns -1 on failure.
int parse_http_status_code(const StringRef &src);

// Header fields to be indexed, except HD_MAXIDX which is convenient
// member to get maximum value.
//
// generated by genheaderfunc.py
enum {
  HD__AUTHORITY,
  HD__HOST,
  HD__METHOD,
  HD__PATH,
  HD__PROTOCOL,
  HD__SCHEME,
  HD__STATUS,
  HD_ACCEPT_ENCODING,
  HD_ACCEPT_LANGUAGE,
  HD_ALT_SVC,
  HD_CACHE_CONTROL,
  HD_CONNECTION,
  HD_CONTENT_LENGTH,
  HD_CONTENT_TYPE,
  HD_COOKIE,
  HD_DATE,
  HD_EARLY_DATA,
  HD_EXPECT,
  HD_FORWARDED,
  HD_HOST,
  HD_HTTP2_SETTINGS,
  HD_IF_MODIFIED_SINCE,
  HD_KEEP_ALIVE,
  HD_LINK,
  HD_LOCATION,
  HD_PRIORITY,
  HD_PROXY_CONNECTION,
  HD_SEC_WEBSOCKET_ACCEPT,
  HD_SEC_WEBSOCKET_KEY,
  HD_SERVER,
  HD_TE,
  HD_TRAILER,
  HD_TRANSFER_ENCODING,
  HD_UPGRADE,
  HD_USER_AGENT,
  HD_VIA,
  HD_X_FORWARDED_FOR,
  HD_X_FORWARDED_PROTO,
  HD_MAXIDX,
};

using HeaderIndex = std::array<int16_t, HD_MAXIDX>;

// Looks up header token for header name |name|.  Only headers we are
// interested in are tokenized.  If header name cannot be tokenized,
// returns -1.
int lookup_token(const StringRef &name);

// Initializes |hdidx|, header index.  The |hdidx| must point to the
// array containing at least HD_MAXIDX elements.
void init_hdidx(HeaderIndex &hdidx);
// Indexes header |token| using index |idx|.
void index_header(HeaderIndex &hdidx, int32_t token, size_t idx);

// Returns header denoted by |token| using index |hdidx|.
const Headers::value_type *get_header(const HeaderIndex &hdidx, int32_t token,
                                      const Headers &nva);

Headers::value_type *get_header(const HeaderIndex &hdidx, int32_t token,
                                Headers &nva);

struct LinkHeader {
  // The region of URI.  This might not be NULL-terminated.
  StringRef uri;
};

// Returns next URI-reference in Link header field value |src|.  If no
// URI-reference found after searching all input, returned uri field
// is empty.  This imply that empty URI-reference is ignored during
// parsing.
std::vector<LinkHeader> parse_link_header(const StringRef &src);

// Constructs path by combining base path |base_path| with another
// path |rel_path|.  The base path and another path can have optional
// query component.  This function assumes |base_path| is normalized.
// In other words, it does not contain ".." or "."  path components
// and starts with "/" if it is not empty.
std::string path_join(const StringRef &base, const StringRef &base_query,
                      const StringRef &rel_path, const StringRef &rel_query);

StringRef path_join(BlockAllocator &balloc, const StringRef &base_path,
                    const StringRef &base_query, const StringRef &rel_path,
                    const StringRef &rel_query);

// true if response has body, taking into account the request method
// and status code.
bool expect_response_body(const std::string &method, int status_code);
bool expect_response_body(int method_token, int status_code);

// true if response has body, taking into account status code only.
bool expect_response_body(int status_code);

// Looks up method token for method name |name|.  Only methods defined
// in llhttp.h (llhttp_method) are tokenized.  If method name cannot
// be tokenized, returns -1.
int lookup_method_token(const StringRef &name);

// Returns string representation of |method_token|.  This is wrapper
// around llhttp_method_name from llhttp.  If |method_token| is
// unknown, program aborts.  The returned StringRef is guaranteed to
// be NULL-terminated.
StringRef to_method_string(int method_token);

StringRef normalize_path(BlockAllocator &balloc, const StringRef &path,
                         const StringRef &query);

// normalize_path_colon is like normalize_path, but it additionally
// does percent-decoding %3A in order to workaround the issue that ':'
// cannot be included in backend pattern.
StringRef normalize_path_colon(BlockAllocator &balloc, const StringRef &path,
                               const StringRef &query);

std::string normalize_path(const StringRef &path, const StringRef &query);

StringRef rewrite_clean_path(BlockAllocator &balloc, const StringRef &src);

// Returns path component of |uri|.  The returned path does not
// include query component.  This function returns empty string if it
// fails.
StringRef get_pure_path_component(const StringRef &uri);

// Deduces scheme, authority and path from given |uri|, and stores
// them in |scheme|, |authority|, and |path| respectively.  If |uri|
// is relative path, path resolution takes place using path given in
// |base| of length |baselen|.  This function returns 0 if it
// succeeds, or -1.
int construct_push_component(BlockAllocator &balloc, StringRef &scheme,
                             StringRef &authority, StringRef &path,
                             const StringRef &base, const StringRef &uri);

// Copies |src| and return its lower-cased version.
StringRef copy_lower(BlockAllocator &balloc, const StringRef &src);

// Returns true if te header field value |s| contains "trailers".
bool contains_trailers(const StringRef &s);

// Creates Sec-WebSocket-Accept value for |key|.  The capacity of
// buffer pointed by |dest| must have at least 24 bytes (base64
// encoded length of 16 bytes data).  It returns empty string in case
// of error.
StringRef make_websocket_accept_token(uint8_t *dest, const StringRef &key);

// Returns true if HTTP version represents pre-HTTP/1.1 (e.g.,
// HTTP/0.9 or HTTP/1.0).
bool legacy_http1(int major, int minor);

// Returns true if transfer-encoding field value |s| conforms RFC
// strictly.  This function does not allow empty value, BWS, and empty
// list elements.
bool check_transfer_encoding(const StringRef &s);

// Encodes |extpri| in the wire format.
std::string encode_extpri(const nghttp2_extpri &extpri);

} // namespace http2

} // namespace nghttp2

#endif // HTTP2_H
