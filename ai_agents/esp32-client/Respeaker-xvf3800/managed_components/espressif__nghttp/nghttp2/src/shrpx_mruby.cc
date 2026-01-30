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
#include "shrpx_mruby.h"

#include <mruby/compile.h>
#include <mruby/string.h>

#include "shrpx_downstream.h"
#include "shrpx_config.h"
#include "shrpx_mruby_module.h"
#include "shrpx_downstream_connection.h"
#include "shrpx_log.h"

namespace shrpx {

namespace mruby {

MRubyContext::MRubyContext(mrb_state *mrb, mrb_value app, mrb_value env)
  : mrb_(mrb), app_(std::move(app)), env_(std::move(env)) {}

MRubyContext::~MRubyContext() {
  if (mrb_) {
    mrb_close(mrb_);
  }
}

int MRubyContext::run_app(Downstream *downstream, int phase) {
  if (!mrb_) {
    return 0;
  }

  MRubyAssocData data{downstream, phase};

  mrb_->ud = &data;

  int rv = 0;
  auto ai = mrb_gc_arena_save(mrb_);
  auto ai_d = defer([ai, this]() { mrb_gc_arena_restore(mrb_, ai); });

  const char *method;
  switch (phase) {
  case PHASE_REQUEST:
    if (!mrb_respond_to(mrb_, app_, mrb_intern_lit(mrb_, "on_req"))) {
      return 0;
    }
    method = "on_req";
    break;
  case PHASE_RESPONSE:
    if (!mrb_respond_to(mrb_, app_, mrb_intern_lit(mrb_, "on_resp"))) {
      return 0;
    }
    method = "on_resp";
    break;
  default:
    assert(0);
    abort();
  }

  auto res = mrb_funcall(mrb_, app_, method, 1, env_);
  (void)res;

  if (mrb_->exc) {
    // If response has been committed, ignore error
    if (downstream->get_response_state() != DownstreamState::MSG_COMPLETE) {
      rv = -1;
    }

    auto exc = mrb_obj_value(mrb_->exc);
    auto inspect = mrb_inspect(mrb_, exc);

    LOG(ERROR) << "Exception caught while executing mruby code: "
               << mrb_str_to_cstr(mrb_, inspect);
  }

  mrb_->ud = nullptr;

  return rv;
}

int MRubyContext::run_on_request_proc(Downstream *downstream) {
  return run_app(downstream, PHASE_REQUEST);
}

int MRubyContext::run_on_response_proc(Downstream *downstream) {
  return run_app(downstream, PHASE_RESPONSE);
}

void MRubyContext::delete_downstream(Downstream *downstream) {
  if (!mrb_) {
    return;
  }
  delete_downstream_from_module(mrb_, downstream);
}

namespace {
mrb_value instantiate_app(mrb_state *mrb, RProc *proc) {
  mrb->ud = nullptr;

  auto res = mrb_top_run(mrb, proc, mrb_top_self(mrb), 0);

  if (mrb->exc) {
    auto exc = mrb_obj_value(mrb->exc);
    auto inspect = mrb_inspect(mrb, exc);

    LOG(ERROR) << "Exception caught while executing mruby code: "
               << mrb_str_to_cstr(mrb, inspect);

    return mrb_nil_value();
  }

  return res;
}
} // namespace

// Based on
// https://github.com/h2o/h2o/blob/master/lib/handler/mruby.c.  It is
// very hard to write these kind of code because mruby has almost no
// documentation about compiling or generating code, at least at the
// time of this writing.
RProc *compile(mrb_state *mrb, const StringRef &filename) {
  if (filename.empty()) {
    return nullptr;
  }

  auto infile = fopen(filename.data(), "rb");
  if (infile == nullptr) {
    LOG(ERROR) << "Could not open mruby file " << filename;
    return nullptr;
  }
  auto infile_d = defer(fclose, infile);

  auto mrbc = mrb_ccontext_new(mrb);
  if (mrbc == nullptr) {
    LOG(ERROR) << "mrb_context_new failed";
    return nullptr;
  }
  auto mrbc_d = defer(mrb_ccontext_free, mrb, mrbc);

  auto parser = mrb_parse_file(mrb, infile, nullptr);
  if (parser == nullptr) {
    LOG(ERROR) << "mrb_parse_nstring failed";
    return nullptr;
  }
  auto parser_d = defer(mrb_parser_free, parser);

  if (parser->nerr != 0) {
    LOG(ERROR) << "mruby parser detected parse error";
    return nullptr;
  }

  auto proc = mrb_generate_code(mrb, parser);
  if (proc == nullptr) {
    LOG(ERROR) << "mrb_generate_code failed";
    return nullptr;
  }

  return proc;
}

std::unique_ptr<MRubyContext> create_mruby_context(const StringRef &filename) {
  if (filename.empty()) {
    return std::make_unique<MRubyContext>(nullptr, mrb_nil_value(),
                                          mrb_nil_value());
  }

  auto mrb = mrb_open();
  if (mrb == nullptr) {
    LOG(ERROR) << "mrb_open failed";
    return nullptr;
  }

  auto ai = mrb_gc_arena_save(mrb);

  auto req_proc = compile(mrb, filename);

  if (!req_proc) {
    mrb_gc_arena_restore(mrb, ai);
    LOG(ERROR) << "Could not compile mruby code " << filename;
    mrb_close(mrb);
    return nullptr;
  }

  auto env = init_module(mrb);

  auto app = instantiate_app(mrb, req_proc);
  if (mrb_nil_p(app)) {
    mrb_gc_arena_restore(mrb, ai);
    LOG(ERROR) << "Could not instantiate mruby app from " << filename;
    mrb_close(mrb);
    return nullptr;
  }

  mrb_gc_arena_restore(mrb, ai);

  // TODO These are not necessary, because we retain app and env?
  mrb_gc_protect(mrb, env);
  mrb_gc_protect(mrb, app);

  return std::make_unique<MRubyContext>(mrb, std::move(app), std::move(env));
}

mrb_sym intern_ptr(mrb_state *mrb, void *ptr) {
  auto p = reinterpret_cast<uintptr_t>(ptr);

  return mrb_intern(mrb, reinterpret_cast<const char *>(&p), sizeof(p));
}

void check_phase(mrb_state *mrb, int phase, int phase_mask) {
  if ((phase & phase_mask) == 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "operation was not allowed in this phase");
  }
}

} // namespace mruby

} // namespace shrpx
