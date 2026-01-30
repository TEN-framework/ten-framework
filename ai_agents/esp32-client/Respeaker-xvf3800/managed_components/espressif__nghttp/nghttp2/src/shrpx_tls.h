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
#ifndef SHRPX_TLS_H
#define SHRPX_TLS_H

#include "shrpx.h"

#include <vector>
#include <mutex>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/ssl.h>
#  include <wolfssl/openssl/err.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/ssl.h>
#  include <openssl/err.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL

#include <ev.h>

#ifdef HAVE_NEVERBLEED
#  include <neverbleed.h>
#endif // HAVE_NEVERBLEED

#include "network.h"
#include "shrpx_config.h"
#include "shrpx_router.h"

namespace shrpx {

class ClientHandler;
class Worker;
class DownstreamConnectionPool;
struct DownstreamAddr;
struct UpstreamAddr;

namespace tls {

struct TLSSessionCache {
  // ASN1 representation of SSL_SESSION object.  See
  // i2d_SSL_SESSION(3SSL).
  std::vector<uint8_t> session_data;
  // The last time stamp when this cache entry is created or updated.
  std::chrono::steady_clock::time_point last_updated;
};

// This struct stores the additional information per SSL_CTX.  This is
// attached to SSL_CTX using SSL_CTX_set_app_data().
struct TLSContextData {
  // SCT data formatted so that this can be directly sent as
  // extension_data of signed_certificate_timestamp.
  std::vector<uint8_t> sct_data;
#ifndef HAVE_ATOMIC_STD_SHARED_PTR
  // Protects ocsp_data;
  std::mutex mu;
#endif // !HAVE_ATOMIC_STD_SHARED_PTR
  // OCSP response
#ifdef HAVE_ATOMIC_STD_SHARED_PTR
  std::atomic<std::shared_ptr<std::vector<uint8_t>>> ocsp_data;
#else  // !HAVE_ATOMIC_STD_SHARED_PTR
  std::shared_ptr<std::vector<uint8_t>> ocsp_data;
#endif // !HAVE_ATOMIC_STD_SHARED_PTR

  // Path to certificate file
  const char *cert_file;
};

// Create server side SSL_CTX
SSL_CTX *create_ssl_context(const char *private_key_file, const char *cert_file,
                            const std::vector<uint8_t> &sct_data

#ifdef HAVE_NEVERBLEED
                            ,
                            neverbleed_t *nb
#endif // HAVE_NEVERBLEED
);

// Create client side SSL_CTX.  This does not configure ALPN settings.
SSL_CTX *create_ssl_client_context(
#ifdef HAVE_NEVERBLEED
  neverbleed_t *nb,
#endif // HAVE_NEVERBLEED
  const StringRef &cacert, const StringRef &cert_file,
  const StringRef &private_key_file);

ClientHandler *accept_connection(Worker *worker, int fd, sockaddr *addr,
                                 int addrlen, const UpstreamAddr *faddr);

// Check peer's certificate against given |address| and |host|.
int check_cert(SSL *ssl, const Address *addr, const StringRef &host);
// Check peer's certificate against given host name described in
// |addr| and numeric address in |raddr|.  Note that |raddr| might not
// point to &addr->addr.
int check_cert(SSL *ssl, const DownstreamAddr *addr, const Address *raddr);

// Verify |cert| using numeric IP address.  |hostname| and |addr|
// should contain the same numeric IP address.  This function returns
// 0 if it succeeds, or -1.
int verify_numeric_hostname(X509 *cert, const StringRef &hostname,
                            const Address *addr);

// Verify |cert| using DNS name hostname.  This function returns 0 if
// it succeeds, or -1.
int verify_dns_hostname(X509 *cert, const StringRef &hostname);

struct WildcardRevPrefix {
  WildcardRevPrefix(const StringRef &prefix, size_t idx)
    : prefix(std::begin(prefix), std::end(prefix)), idx(idx) {}

  // "Prefix" of wildcard pattern.  It is reversed from original form.
  // For example, if the original wildcard is "test*.nghttp2.org",
  // prefix would be "tset".
  ImmutableString prefix;
  // The index of SSL_CTX.  See ConnectionHandler::get_ssl_ctx().
  size_t idx;
};

struct WildcardPattern {
  // Wildcard host sharing only suffix is probably rare, so we just do
  // linear search.
  std::vector<WildcardRevPrefix> rev_prefix;
};

class CertLookupTree {
public:
  CertLookupTree();

  // Adds hostname pattern |hostname| to the lookup tree, associating
  // value |index|.  When the queried host matches this pattern,
  // |index| is returned.  We support wildcard pattern.  The left most
  // '*' is considered as wildcard character, and it must match at
  // least one character.  If the same pattern has been already added,
  // this function does not alter the tree, and returns the existing
  // matching index.
  //
  // The caller should lower-case |hostname| since this function does
  // do that, and lookup function performs case-sensitive match.
  //
  // TODO Treat wildcard pattern described as RFC 6125.
  //
  // This function returns the index.  It returns -1 if it fails
  // (e.g., hostname is too long).  If the returned index equals to
  // |index|, then hostname is added to the tree with the value
  // |index|.  If it is not -1, and does not equal to |index|, same
  // hostname has already been added to the tree.
  ssize_t add_cert(const StringRef &hostname, size_t index);

  // Looks up index using the given |hostname|.  The exact match takes
  // precedence over wildcard match.  For wildcard match, longest
  // match (sum of matched suffix and prefix length in bytes) is
  // preferred, breaking a tie with longer suffix.
  //
  // The caller should lower-case |hostname| since this function
  // performs case-sensitive match.
  ssize_t lookup(const StringRef &hostname);

  // Dumps the contents of this lookup tree to stderr.
  void dump() const;

private:
  // Exact match
  Router router_;
  // Wildcard reversed suffix match.  The returned index is into
  // wildcard_patterns_.
  Router rev_wildcard_router_;
  // Stores wildcard suffix patterns.
  std::vector<WildcardPattern> wildcard_patterns_;
};

// Adds hostnames in certificate in |ssl_ctx| to lookup tree |lt|.
// The subjectAltNames and commonName are considered as eligible
// hostname.  If there is at least one dNSName in subjectAltNames,
// commonName is not considered.  |ssl_ctx| is also added to
// |indexed_ssl_ctx|.  This function returns 0 if it succeeds, or -1.
int cert_lookup_tree_add_ssl_ctx(
  CertLookupTree *lt, std::vector<std::vector<SSL_CTX *>> &indexed_ssl_ctx,
  SSL_CTX *ssl_ctx);

// Returns true if |proto| is included in the
// protocol list |protos|.
bool in_proto_list(const std::vector<StringRef> &protos,
                   const StringRef &proto);

// Returns true if security requirement for HTTP/2 is fulfilled.
bool check_http2_requirement(SSL *ssl);

// Returns SSL/TLS option mask to disable SSL/TLS protocol version not
// included in |tls_proto_list|.  The returned mask can be directly
// passed to SSL_CTX_set_options().
long int create_tls_proto_mask(const std::vector<StringRef> &tls_proto_list);

int set_alpn_prefs(std::vector<unsigned char> &out,
                   const std::vector<StringRef> &protos);

// Setups server side SSL_CTX.  This function inspects get_config()
// and if upstream_no_tls is true, returns nullptr.  Otherwise
// construct default SSL_CTX.  If subcerts are available
// (get_config()->subcerts), caller should provide CertLookupTree
// object as |cert_tree| parameter, otherwise SNI does not work.  All
// the created SSL_CTX is stored into |all_ssl_ctx|.  They are also
// added to |indexed_ssl_ctx|.  |cert_tree| uses its index to
// associate hostname to the SSL_CTX.
SSL_CTX *
setup_server_ssl_context(std::vector<SSL_CTX *> &all_ssl_ctx,
                         std::vector<std::vector<SSL_CTX *>> &indexed_ssl_ctx,
                         CertLookupTree *cert_tree
#ifdef HAVE_NEVERBLEED
                         ,
                         neverbleed_t *nb
#endif // HAVE_NEVERBLEED
);

#ifdef ENABLE_HTTP3
SSL_CTX *setup_quic_server_ssl_context(
  std::vector<SSL_CTX *> &all_ssl_ctx,
  std::vector<std::vector<SSL_CTX *>> &indexed_ssl_ctx,
  CertLookupTree *cert_tree
#  ifdef HAVE_NEVERBLEED
  ,
  neverbleed_t *nb
#  endif // HAVE_NEVERBLEED
);
#endif // ENABLE_HTTP3

// Setups client side SSL_CTX.
SSL_CTX *setup_downstream_client_ssl_context(
#ifdef HAVE_NEVERBLEED
  neverbleed_t *nb
#endif // HAVE_NEVERBLEED
);

// Sets ALPN settings in |SSL| suitable for HTTP/2 use.
void setup_downstream_http2_alpn(SSL *ssl);
// Sets ALPN settings in |SSL| suitable for HTTP/1.1 use.
void setup_downstream_http1_alpn(SSL *ssl);

// Creates CertLookupTree.  If frontend is configured not to use TLS,
// this function returns nullptr.
std::unique_ptr<CertLookupTree> create_cert_lookup_tree();

SSL *create_ssl(SSL_CTX *ssl_ctx);

// Returns true if SSL/TLS is enabled on upstream
bool upstream_tls_enabled(const ConnectionConfig &connconf);

// Performs TLS hostname match.  |pattern| can contain wildcard
// character '*', which matches prefix of target hostname.  There are
// several restrictions to make wildcard work.  The matching algorithm
// is based on RFC 6125.
bool tls_hostname_match(const StringRef &pattern, const StringRef &hostname);

// Caches |session|.  |session| is serialized into ASN1
// representation, and stored.  |t| is used as a time stamp.
// Depending on the existing cache's time stamp, |session| might not
// be cached.
void try_cache_tls_session(TLSSessionCache *cache, SSL_SESSION *session,
                           const std::chrono::steady_clock::time_point &t);

// Returns cached session associated |addr|.  If no cache entry is
// found associated to |addr|, nullptr will be returned.
SSL_SESSION *reuse_tls_session(const TLSSessionCache &addr);

// Loads certificate form file |filename|.  The caller should delete
// the returned object using X509_free().
X509 *load_certificate(const char *filename);

// Returns TLS version from |v|.  The returned value is defined in
// OpenSSL header file.  This function returns -1 if |v| is not valid
// TLS version string.
int proto_version_from_string(const StringRef &v);

// Verifies OCSP response |ocsp_resp| of length |ocsp_resplen|.  This
// function returns 0 if it succeeds, or -1.
int verify_ocsp_response(SSL_CTX *ssl_ctx, const uint8_t *ocsp_resp,
                         size_t ocsp_resplen);

// Stores fingerprint of |x| in |dst| of length |dstlen|.  |md|
// specifies hash function to use, and |dstlen| must be large enough
// to include hash value (e.g., 32 bytes for SHA-256).  This function
// returns the number of bytes written in |dst|, or -1.
ssize_t get_x509_fingerprint(uint8_t *dst, size_t dstlen, const X509 *x,
                             const EVP_MD *md);

// Returns subject name of |x|.  If this function fails to get subject
// name, it returns an empty string.
StringRef get_x509_subject_name(BlockAllocator &balloc, X509 *x);

// Returns issuer name of |x|.  If this function fails to get issuer
// name, it returns an empty string.
StringRef get_x509_issuer_name(BlockAllocator &balloc, X509 *x);

// Returns serial number of |x|.  If this function fails to get serial
// number, it returns an empty string.  number
StringRef get_x509_serial(BlockAllocator &balloc, X509 *x);

// Fills NotBefore of |x| in |t|.  This function returns 0 if it
// succeeds, or -1.
int get_x509_not_before(time_t &t, X509 *x);

// Fills NotAfter of |x| in |t|.  This function returns 0 if it
// succeeds, or -1.
int get_x509_not_after(time_t &t, X509 *x);

} // namespace tls

} // namespace shrpx

#endif // SHRPX_TLS_H
