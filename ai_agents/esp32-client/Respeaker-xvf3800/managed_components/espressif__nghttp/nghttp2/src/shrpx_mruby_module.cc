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
#include "shrpx_mruby_module.h"

#include <array>

#include <mruby/variable.h>
#include <mruby/string.h>
#include <mruby/hash.h>
#include <mruby/array.h>

#include "shrpx_mruby.h"
#include "shrpx_mruby_module_env.h"
#include "shrpx_mruby_module_request.h"
#include "shrpx_mruby_module_response.h"

namespace shrpx {

namespace mruby {

namespace {
mrb_value create_env(mrb_state *mrb) {
  auto module = mrb_module_get(mrb, "Nghttpx");

  auto env_class = mrb_class_get_under(mrb, module, "Env");
  auto request_class = mrb_class_get_under(mrb, module, "Request");
  auto response_class = mrb_class_get_under(mrb, module, "Response");

  auto env = mrb_obj_new(mrb, env_class, 0, nullptr);
  auto req = mrb_obj_new(mrb, request_class, 0, nullptr);
  auto resp = mrb_obj_new(mrb, response_class, 0, nullptr);

  mrb_iv_set(mrb, env, mrb_intern_lit(mrb, "req"), req);
  mrb_iv_set(mrb, env, mrb_intern_lit(mrb, "resp"), resp);

  return env;
}
} // namespace

void delete_downstream_from_module(mrb_state *mrb, Downstream *downstream) {
  auto module = mrb_module_get(mrb, "Nghttpx");
  auto env = mrb_obj_iv_get(mrb, reinterpret_cast<RObject *>(module),
                            mrb_intern_lit(mrb, "env"));
  if (mrb_nil_p(env)) {
    return;
  }

  mrb_iv_remove(mrb, env, intern_ptr(mrb, downstream));
}

mrb_value init_module(mrb_state *mrb) {
  auto module = mrb_define_module(mrb, "Nghttpx");

  mrb_define_const(mrb, module, "REQUEST_PHASE",
                   mrb_fixnum_value(PHASE_REQUEST));
  mrb_define_const(mrb, module, "RESPONSE_PHASE",
                   mrb_fixnum_value(PHASE_RESPONSE));

  init_env_class(mrb, module);
  init_request_class(mrb, module);
  init_response_class(mrb, module);

  return create_env(mrb);
}

mrb_value create_headers_hash(mrb_state *mrb, const HeaderRefs &headers) {
  auto hash = mrb_hash_new(mrb);

  for (auto &hd : headers) {
    if (hd.name.empty() || hd.name[0] == ':') {
      continue;
    }
    auto ai = mrb_gc_arena_save(mrb);

    auto key = mrb_str_new(mrb, hd.name.data(), hd.name.size());
    auto ary = mrb_hash_get(mrb, hash, key);
    if (mrb_nil_p(ary)) {
      ary = mrb_ary_new(mrb);
      mrb_hash_set(mrb, hash, key, ary);
    }
    mrb_ary_push(mrb, ary, mrb_str_new(mrb, hd.value.data(), hd.value.size()));

    mrb_gc_arena_restore(mrb, ai);
  }

  return hash;
}

} // namespace mruby

} // namespace shrpx
