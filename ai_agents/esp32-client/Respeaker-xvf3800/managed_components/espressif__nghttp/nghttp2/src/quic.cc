/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2019 nghttp2 contributors
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
#include "quic.h"

#include <cassert>

#include <ngtcp2/ngtcp2.h>
#include <nghttp3/nghttp3.h>

#include "template.h"

using namespace nghttp2;

namespace quic {

Error err_transport(int liberr) {
  if (liberr == NGTCP2_ERR_RECV_VERSION_NEGOTIATION) {
    return {ErrorType::TransportVersionNegotiation, 0};
  }
  return {ErrorType::Transport,
          ngtcp2_err_infer_quic_transport_error_code(liberr)};
}

Error err_transport_idle_timeout() {
  return {ErrorType::TransportIdleTimeout, 0};
}

Error err_transport_tls(int alert) {
  return {ErrorType::Transport, ngtcp2_err_infer_quic_transport_error_code(
                                  NGTCP2_CRYPTO_ERROR | alert)};
}

Error err_application(int liberr) {
  return {ErrorType::Application,
          nghttp3_err_infer_quic_app_error_code(liberr)};
}

} // namespace quic
