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
#include "shrpx_mruby_module_env.h"

#include <mruby/variable.h>
#include <mruby/string.h>
#include <mruby/hash.h>

#include "shrpx_downstream.h"
#include "shrpx_upstream.h"
#include "shrpx_client_handler.h"
#include "shrpx_mruby.h"
#include "shrpx_mruby_module.h"
#include "shrpx_log.h"
#include "shrpx_tls.h"

namespace shrpx {

namespace mruby {

namespace {
mrb_value env_init(mrb_state *mrb, mrb_value self) { return self; }
} // namespace

namespace {
mrb_value env_get_req(mrb_state *mrb, mrb_value self) {
  return mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "req"));
}
} // namespace

namespace {
mrb_value env_get_resp(mrb_state *mrb, mrb_value self) {
  return mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "resp"));
}
} // namespace

namespace {
mrb_value env_get_ctx(mrb_state *mrb, mrb_value self) {
  auto data = reinterpret_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;

  auto dsym = intern_ptr(mrb, downstream);

  auto ctx = mrb_iv_get(mrb, self, dsym);
  if (mrb_nil_p(ctx)) {
    ctx = mrb_hash_new(mrb);
    mrb_iv_set(mrb, self, dsym, ctx);
  }

  return ctx;
}
} // namespace

namespace {
mrb_value env_get_phase(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);

  return mrb_fixnum_value(data->phase);
}
} // namespace

namespace {
mrb_value env_get_remote_addr(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();

  auto &ipaddr = handler->get_ipaddr();

  return mrb_str_new(mrb, ipaddr.data(), ipaddr.size());
}
} // namespace

namespace {
mrb_value env_get_server_port(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto faddr = handler->get_upstream_addr();

  return mrb_fixnum_value(faddr->port);
}
} // namespace

namespace {
mrb_value env_get_server_addr(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto faddr = handler->get_upstream_addr();

  return mrb_str_new(mrb, faddr->host.data(), faddr->host.size());
}
} // namespace

namespace {
mrb_value env_get_tls_used(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();

  return handler->get_ssl() ? mrb_true_value() : mrb_false_value();
}
} // namespace

namespace {
mrb_value env_get_tls_sni(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto sni = handler->get_tls_sni();

  return mrb_str_new(mrb, sni.data(), sni.size());
}
} // namespace

namespace {
mrb_value env_get_tls_client_fingerprint_md(mrb_state *mrb, const EVP_MD *md) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto ssl = handler->get_ssl();

  if (!ssl) {
    return mrb_str_new_static(mrb, "", 0);
  }

#if OPENSSL_3_0_0_API
  auto x = SSL_get0_peer_certificate(ssl);
#else  // !OPENSSL_3_0_0_API
  auto x = SSL_get_peer_certificate(ssl);
#endif // !OPENSSL_3_0_0_API
  if (!x) {
    return mrb_str_new_static(mrb, "", 0);
  }

  // Currently the largest hash value is SHA-256, which is 32 bytes.
  std::array<uint8_t, 32> buf;
  auto slen = tls::get_x509_fingerprint(buf.data(), buf.size(), x, md);
#if !OPENSSL_3_0_0_API
  X509_free(x);
#endif // !OPENSSL_3_0_0_API
  if (slen == -1) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "could not compute client fingerprint");
  }

  auto &balloc = downstream->get_block_allocator();
  auto f =
    util::format_hex(balloc, std::span{buf.data(), static_cast<size_t>(slen)});
  return mrb_str_new(mrb, f.data(), f.size());
}
} // namespace

namespace {
mrb_value env_get_tls_client_fingerprint_sha256(mrb_state *mrb,
                                                mrb_value self) {
  return env_get_tls_client_fingerprint_md(mrb, EVP_sha256());
}
} // namespace

namespace {
mrb_value env_get_tls_client_fingerprint_sha1(mrb_state *mrb, mrb_value self) {
  return env_get_tls_client_fingerprint_md(mrb, EVP_sha1());
}
} // namespace

namespace {
mrb_value env_get_tls_client_subject_name(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto ssl = handler->get_ssl();

  if (!ssl) {
    return mrb_str_new_static(mrb, "", 0);
  }

#if OPENSSL_3_0_0_API
  auto x = SSL_get0_peer_certificate(ssl);
#else  // !OPENSSL_3_0_0_API
  auto x = SSL_get_peer_certificate(ssl);
#endif // !OPENSSL_3_0_0_API
  if (!x) {
    return mrb_str_new_static(mrb, "", 0);
  }

  auto &balloc = downstream->get_block_allocator();
  auto name = tls::get_x509_subject_name(balloc, x);
#if !OPENSSL_3_0_0_API
  X509_free(x);
#endif // !OPENSSL_3_0_0_API
  return mrb_str_new(mrb, name.data(), name.size());
}
} // namespace

namespace {
mrb_value env_get_tls_client_issuer_name(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto ssl = handler->get_ssl();

  if (!ssl) {
    return mrb_str_new_static(mrb, "", 0);
  }

#if OPENSSL_3_0_0_API
  auto x = SSL_get0_peer_certificate(ssl);
#else  // !OPENSSL_3_0_0_API
  auto x = SSL_get_peer_certificate(ssl);
#endif // !OPENSSL_3_0_0_API
  if (!x) {
    return mrb_str_new_static(mrb, "", 0);
  }

  auto &balloc = downstream->get_block_allocator();
  auto name = tls::get_x509_issuer_name(balloc, x);
#if !OPENSSL_3_0_0_API
  X509_free(x);
#endif // !OPENSSL_3_0_0_API
  return mrb_str_new(mrb, name.data(), name.size());
}
} // namespace

namespace {
mrb_value env_get_tls_client_serial(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto ssl = handler->get_ssl();

  if (!ssl) {
    return mrb_str_new_static(mrb, "", 0);
  }

#if OPENSSL_3_0_0_API
  auto x = SSL_get0_peer_certificate(ssl);
#else  // !OPENSSL_3_0_0_API
  auto x = SSL_get_peer_certificate(ssl);
#endif // !OPENSSL_3_0_0_API
  if (!x) {
    return mrb_str_new_static(mrb, "", 0);
  }

  auto &balloc = downstream->get_block_allocator();
  auto sn = tls::get_x509_serial(balloc, x);
#if !OPENSSL_3_0_0_API
  X509_free(x);
#endif // !OPENSSL_3_0_0_API
  return mrb_str_new(mrb, sn.data(), sn.size());
}
} // namespace

namespace {
mrb_value env_get_tls_client_not_before(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto ssl = handler->get_ssl();

  if (!ssl) {
    return mrb_fixnum_value(0);
  }

#if OPENSSL_3_0_0_API
  auto x = SSL_get0_peer_certificate(ssl);
#else  // !OPENSSL_3_0_0_API
  auto x = SSL_get_peer_certificate(ssl);
#endif // !OPENSSL_3_0_0_API
  if (!x) {
    return mrb_fixnum_value(0);
  }

  time_t t;
  if (tls::get_x509_not_before(t, x) != 0) {
    t = 0;
  }

#if !OPENSSL_3_0_0_API
  X509_free(x);
#endif // !OPENSSL_3_0_0_API

  return mrb_fixnum_value(t);
}
} // namespace

namespace {
mrb_value env_get_tls_client_not_after(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto ssl = handler->get_ssl();

  if (!ssl) {
    return mrb_fixnum_value(0);
  }

#if OPENSSL_3_0_0_API
  auto x = SSL_get0_peer_certificate(ssl);
#else  // !OPENSSL_3_0_0_API
  auto x = SSL_get_peer_certificate(ssl);
#endif // !OPENSSL_3_0_0_API
  if (!x) {
    return mrb_fixnum_value(0);
  }

  time_t t;
  if (tls::get_x509_not_after(t, x) != 0) {
    t = 0;
  }

#if !OPENSSL_3_0_0_API
  X509_free(x);
#endif // !OPENSSL_3_0_0_API

  return mrb_fixnum_value(t);
}
} // namespace

namespace {
mrb_value env_get_tls_cipher(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto ssl = handler->get_ssl();

  if (!ssl) {
    return mrb_str_new_static(mrb, "", 0);
  }

  return mrb_str_new_cstr(mrb, SSL_get_cipher_name(ssl));
}
} // namespace

namespace {
mrb_value env_get_tls_protocol(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto ssl = handler->get_ssl();

  if (!ssl) {
    return mrb_str_new_static(mrb, "", 0);
  }

  return mrb_str_new_cstr(mrb, nghttp2::tls::get_tls_protocol(ssl));
}
} // namespace

namespace {
mrb_value env_get_tls_session_id(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto ssl = handler->get_ssl();

  if (!ssl) {
    return mrb_str_new_static(mrb, "", 0);
  }

  auto session = SSL_get_session(ssl);
  if (!session) {
    return mrb_str_new_static(mrb, "", 0);
  }

  unsigned int session_id_length = 0;
  auto session_id = SSL_SESSION_get_id(session, &session_id_length);

  auto &balloc = downstream->get_block_allocator();
  auto id = util::format_hex(balloc, std::span{session_id, session_id_length});
  return mrb_str_new(mrb, id.data(), id.size());
}
} // namespace

namespace {
mrb_value env_get_tls_session_reused(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto ssl = handler->get_ssl();

  if (!ssl) {
    return mrb_false_value();
  }

  return SSL_session_reused(ssl) ? mrb_true_value() : mrb_false_value();
}
} // namespace

namespace {
mrb_value env_get_alpn(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto alpn = handler->get_alpn();
  return mrb_str_new(mrb, alpn.data(), alpn.size());
}
} // namespace

namespace {
mrb_value env_get_tls_handshake_finished(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto upstream = downstream->get_upstream();
  auto handler = upstream->get_client_handler();
  auto conn = handler->get_connection();
  return SSL_is_init_finished(conn->tls.ssl) ? mrb_true_value()
                                             : mrb_false_value();
}
} // namespace

void init_env_class(mrb_state *mrb, RClass *module) {
  auto env_class =
    mrb_define_class_under(mrb, module, "Env", mrb->object_class);

  mrb_define_method(mrb, env_class, "initialize", env_init, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "req", env_get_req, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "resp", env_get_resp, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "ctx", env_get_ctx, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "phase", env_get_phase, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "remote_addr", env_get_remote_addr,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "server_addr", env_get_server_addr,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "server_port", env_get_server_port,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_used", env_get_tls_used,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_sni", env_get_tls_sni,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_client_fingerprint_sha256",
                    env_get_tls_client_fingerprint_sha256, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_client_fingerprint_sha1",
                    env_get_tls_client_fingerprint_sha1, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_client_issuer_name",
                    env_get_tls_client_issuer_name, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_client_subject_name",
                    env_get_tls_client_subject_name, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_client_serial",
                    env_get_tls_client_serial, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_client_not_before",
                    env_get_tls_client_not_before, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_client_not_after",
                    env_get_tls_client_not_after, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_cipher", env_get_tls_cipher,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_protocol", env_get_tls_protocol,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_session_id", env_get_tls_session_id,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_session_reused",
                    env_get_tls_session_reused, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "alpn", env_get_alpn, MRB_ARGS_NONE());
  mrb_define_method(mrb, env_class, "tls_handshake_finished",
                    env_get_tls_handshake_finished, MRB_ARGS_NONE());
}

} // namespace mruby

} // namespace shrpx
