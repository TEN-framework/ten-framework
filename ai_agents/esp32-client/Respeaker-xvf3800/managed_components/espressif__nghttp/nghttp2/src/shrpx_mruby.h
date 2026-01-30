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
#ifndef SHRPX_MRUBY_H
#define SHRPX_MRUBY_H

#include "shrpx.h"

#include <memory>

#include <mruby.h>
#include <mruby/proc.h>

#include "template.h"

using namespace nghttp2;

namespace shrpx {

class Downstream;

namespace mruby {

class MRubyContext {
public:
  MRubyContext(mrb_state *mrb, mrb_value app, mrb_value env);
  ~MRubyContext();

  int run_on_request_proc(Downstream *downstream);
  int run_on_response_proc(Downstream *downstream);

  int run_app(Downstream *downstream, int phase);

  void delete_downstream(Downstream *downstream);

private:
  mrb_state *mrb_;
  mrb_value app_;
  mrb_value env_;
};

enum {
  PHASE_NONE = 0,
  PHASE_REQUEST = 1,
  PHASE_RESPONSE = 1 << 1,
};

struct MRubyAssocData {
  Downstream *downstream;
  int phase;
};

RProc *compile(mrb_state *mrb, const StringRef &filename);

std::unique_ptr<MRubyContext> create_mruby_context(const StringRef &filename);

// Return interned |ptr|.
mrb_sym intern_ptr(mrb_state *mrb, void *ptr);

// Checks that |phase| is set in |phase_mask|.  If not set, raise
// exception.
void check_phase(mrb_state *mrb, int phase, int phase_mask);

} // namespace mruby

} // namespace shrpx

#endif // SHRPX_MRUBY_H
