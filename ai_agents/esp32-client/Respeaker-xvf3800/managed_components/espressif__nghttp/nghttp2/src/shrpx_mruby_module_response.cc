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
#include "shrpx_mruby_module_response.h"

#include <mruby/variable.h>
#include <mruby/string.h>
#include <mruby/hash.h>
#include <mruby/array.h>

#include "shrpx_downstream.h"
#include "shrpx_upstream.h"
#include "shrpx_client_handler.h"
#include "shrpx_mruby.h"
#include "shrpx_mruby_module.h"
#include "shrpx_log.h"
#include "util.h"
#include "http2.h"

namespace shrpx {

namespace mruby {

namespace {
mrb_value response_init(mrb_state *mrb, mrb_value self) { return self; }
} // namespace

namespace {
mrb_value response_get_http_version_major(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  const auto &resp = downstream->response();
  return mrb_fixnum_value(resp.http_major);
}
} // namespace

namespace {
mrb_value response_get_http_version_minor(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  const auto &resp = downstream->response();
  return mrb_fixnum_value(resp.http_minor);
}
} // namespace

namespace {
mrb_value response_get_status(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  const auto &resp = downstream->response();
  return mrb_fixnum_value(resp.http_status);
}
} // namespace

namespace {
mrb_value response_set_status(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto &resp = downstream->response();

  mrb_int status;
  mrb_get_args(mrb, "i", &status);
  // We don't support 1xx status code for mruby scripting yet.
  if (status < 200 || status > 999) {
    mrb_raise(mrb, E_RUNTIME_ERROR,
              "invalid status; it should be [200, 999], inclusive");
  }

  resp.http_status = status;

  return self;
}
} // namespace

namespace {
mrb_value response_get_headers(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  const auto &resp = downstream->response();

  return create_headers_hash(mrb, resp.fs.headers());
}
} // namespace

namespace {
mrb_value response_mod_header(mrb_state *mrb, mrb_value self, bool repl) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto &resp = downstream->response();
  auto &balloc = downstream->get_block_allocator();

  mrb_value key, values;
  mrb_get_args(mrb, "So", &key, &values);

  if (RSTRING_LEN(key) == 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "empty key is not allowed");
  }

  auto ai = mrb_gc_arena_save(mrb);

  key = mrb_funcall(mrb, key, "downcase", 0);

  auto keyref = make_string_ref(
    balloc, StringRef{RSTRING_PTR(key), static_cast<size_t>(RSTRING_LEN(key))});

  mrb_gc_arena_restore(mrb, ai);

  auto token = http2::lookup_token(keyref);

  if (repl) {
    size_t p = 0;
    auto &headers = resp.fs.headers();
    for (size_t i = 0; i < headers.size(); ++i) {
      auto &kv = headers[i];
      if (kv.name == keyref) {
        continue;
      }
      if (i != p) {
        headers[p] = std::move(kv);
      }
      ++p;
    }
    headers.resize(p);
  }

  if (mrb_array_p(values)) {
    auto n = RARRAY_LEN(values);
    for (int i = 0; i < n; ++i) {
      auto value = mrb_ary_ref(mrb, values, i);
      if (!mrb_string_p(value)) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "value must be string");
      }

      resp.fs.add_header_token(
        keyref,
        make_string_ref(balloc,
                        StringRef{RSTRING_PTR(value),
                                  static_cast<size_t>(RSTRING_LEN(value))}),
        false, token);
    }
  } else if (mrb_string_p(values)) {
    resp.fs.add_header_token(
      keyref,
      make_string_ref(balloc,
                      StringRef{RSTRING_PTR(values),
                                static_cast<size_t>(RSTRING_LEN(values))}),
      false, token);
  } else {
    mrb_raise(mrb, E_RUNTIME_ERROR, "value must be string");
  }

  return mrb_nil_value();
}
} // namespace

namespace {
mrb_value response_set_header(mrb_state *mrb, mrb_value self) {
  return response_mod_header(mrb, self, true);
}
} // namespace

namespace {
mrb_value response_add_header(mrb_state *mrb, mrb_value self) {
  return response_mod_header(mrb, self, false);
}
} // namespace

namespace {
mrb_value response_clear_headers(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto &resp = downstream->response();

  resp.fs.clear_headers();

  return mrb_nil_value();
}
} // namespace

namespace {
mrb_value response_return(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto &req = downstream->request();
  auto &resp = downstream->response();
  int rv;

  auto &balloc = downstream->get_block_allocator();

  if (downstream->get_response_state() == DownstreamState::MSG_COMPLETE) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "response has already been committed");
  }

  const char *val;
  mrb_int vallen;
  mrb_get_args(mrb, "|s", &val, &vallen);

  const uint8_t *body = nullptr;
  size_t bodylen = 0;

  if (resp.http_status == 0) {
    resp.http_status = 200;
  }

  if (downstream->expect_response_body() && vallen > 0) {
    body = reinterpret_cast<const uint8_t *>(val);
    bodylen = vallen;
  }

  auto cl = resp.fs.header(http2::HD_CONTENT_LENGTH);

  if (resp.http_status == 204 ||
      (resp.http_status == 200 && req.method == HTTP_CONNECT)) {
    if (cl) {
      // Delete content-length here
      http2::erase_header(cl);
    }

    resp.fs.content_length = -1;
  } else {
    auto content_length = util::make_string_ref_uint(balloc, vallen);

    if (cl) {
      cl->value = content_length;
    } else {
      resp.fs.add_header_token("content-length"_sr, content_length, false,
                               http2::HD_CONTENT_LENGTH);
    }

    resp.fs.content_length = vallen;
  }

  auto date = resp.fs.header(http2::HD_DATE);
  if (!date) {
    auto lgconf = log_config();
    lgconf->update_tstamp(std::chrono::system_clock::now());
    resp.fs.add_header_token("date"_sr,
                             make_string_ref(balloc, lgconf->tstamp->time_http),
                             false, http2::HD_DATE);
  }

  auto upstream = downstream->get_upstream();

  rv = upstream->send_reply(downstream, body, bodylen);
  if (rv != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "could not send response");
  }

  auto handler = upstream->get_client_handler();

  handler->signal_write();

  return self;
}
} // namespace

namespace {
mrb_value response_send_info(mrb_state *mrb, mrb_value self) {
  auto data = static_cast<MRubyAssocData *>(mrb->ud);
  auto downstream = data->downstream;
  auto &resp = downstream->response();
  int rv;

  if (downstream->get_response_state() == DownstreamState::MSG_COMPLETE) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "response has already been committed");
  }

  mrb_int http_status;
  mrb_value hash;
  mrb_get_args(mrb, "iH", &http_status, &hash);

  if (http_status / 100 != 1) {
    mrb_raise(mrb, E_RUNTIME_ERROR,
              "status_code must be in range [100, 199], inclusive");
  }

  auto &balloc = downstream->get_block_allocator();

  auto keys = mrb_hash_keys(mrb, hash);
  auto keyslen = RARRAY_LEN(keys);

  for (int i = 0; i < keyslen; ++i) {
    auto key = mrb_ary_ref(mrb, keys, i);
    if (!mrb_string_p(key)) {
      mrb_raise(mrb, E_RUNTIME_ERROR, "key must be string");
    }

    auto values = mrb_hash_get(mrb, hash, key);

    auto ai = mrb_gc_arena_save(mrb);

    key = mrb_funcall(mrb, key, "downcase", 0);

    auto keyref =
      make_string_ref(balloc, StringRef{RSTRING_PTR(key),
                                        static_cast<size_t>(RSTRING_LEN(key))});

    mrb_gc_arena_restore(mrb, ai);

    auto token = http2::lookup_token(keyref);

    if (mrb_array_p(values)) {
      auto n = RARRAY_LEN(values);
      for (int i = 0; i < n; ++i) {
        auto value = mrb_ary_ref(mrb, values, i);
        if (!mrb_string_p(value)) {
          mrb_raise(mrb, E_RUNTIME_ERROR, "value must be string");
        }

        resp.fs.add_header_token(
          keyref,
          make_string_ref(balloc,
                          StringRef{RSTRING_PTR(value),
                                    static_cast<size_t>(RSTRING_LEN(value))}),
          false, token);
      }
    } else if (mrb_string_p(values)) {
      resp.fs.add_header_token(
        keyref,
        make_string_ref(balloc,
                        StringRef{RSTRING_PTR(values),
                                  static_cast<size_t>(RSTRING_LEN(values))}),
        false, token);
    } else {
      mrb_raise(mrb, E_RUNTIME_ERROR, "value must be string");
    }
  }

  resp.http_status = http_status;

  auto upstream = downstream->get_upstream();

  rv = upstream->on_downstream_header_complete(downstream);
  if (rv != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "could not send non-final response");
  }

  auto handler = upstream->get_client_handler();

  handler->signal_write();

  return self;
}
} // namespace

void init_response_class(mrb_state *mrb, RClass *module) {
  auto response_class =
    mrb_define_class_under(mrb, module, "Response", mrb->object_class);

  mrb_define_method(mrb, response_class, "initialize", response_init,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, response_class, "http_version_major",
                    response_get_http_version_major, MRB_ARGS_NONE());
  mrb_define_method(mrb, response_class, "http_version_minor",
                    response_get_http_version_minor, MRB_ARGS_NONE());
  mrb_define_method(mrb, response_class, "status", response_get_status,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, response_class, "status=", response_set_status,
                    MRB_ARGS_REQ(1));
  mrb_define_method(mrb, response_class, "headers", response_get_headers,
                    MRB_ARGS_NONE());
  mrb_define_method(mrb, response_class, "add_header", response_add_header,
                    MRB_ARGS_REQ(2));
  mrb_define_method(mrb, response_class, "set_header", response_set_header,
                    MRB_ARGS_REQ(2));
  mrb_define_method(mrb, response_class, "clear_headers",
                    response_clear_headers, MRB_ARGS_NONE());
  mrb_define_method(mrb, response_class, "return", response_return,
                    MRB_ARGS_OPT(1));
  mrb_define_method(mrb, response_class, "send_info", response_send_info,
                    MRB_ARGS_REQ(2));
}

} // namespace mruby

} // namespace shrpx
