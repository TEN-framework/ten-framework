#include <string>
#include <vector>
#include <fuzzer/FuzzedDataProvider.h>

#include <nghttp2/nghttp2.h>

namespace {
int on_frame_recv_callback(nghttp2_session *session, const nghttp2_frame *frame,
                           void *user_data) {
  return 0;
}
} // namespace

namespace {
int on_begin_headers_callback(nghttp2_session *session,
                              const nghttp2_frame *frame, void *user_data) {
  return 0;
}
} // namespace

namespace {
int on_header_callback2(nghttp2_session *session, const nghttp2_frame *frame,
                        nghttp2_rcbuf *name, nghttp2_rcbuf *value,
                        uint8_t flags, void *user_data) {
  return 0;
}
} // namespace

namespace {
int before_frame_send_callback(nghttp2_session *session,
                               const nghttp2_frame *frame, void *user_data) {
  return 0;
}
} // namespace

namespace {
int on_frame_send_callback(nghttp2_session *session, const nghttp2_frame *frame,
                           void *user_data) {
  return 0;
}
} // namespace

namespace {
void send_pending(nghttp2_session *session) {
  for (;;) {
    const uint8_t *data;
    auto n = nghttp2_session_mem_send2(session, &data);
    if (n == 0) {
      return;
    }
  }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  nghttp2_session *session;
  nghttp2_session_callbacks *callbacks;

  nghttp2_session_callbacks_new(&callbacks);
  nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks,
                                                       on_frame_recv_callback);
  nghttp2_session_callbacks_set_on_begin_headers_callback(
    callbacks, on_begin_headers_callback);
  nghttp2_session_callbacks_set_on_header_callback2(callbacks,
                                                    on_header_callback2);
  nghttp2_session_callbacks_set_before_frame_send_callback(
    callbacks, before_frame_send_callback);
  nghttp2_session_callbacks_set_on_frame_send_callback(callbacks,
                                                       on_frame_send_callback);

  nghttp2_session_server_new(&session, callbacks, nullptr);
  nghttp2_session_callbacks_del(callbacks);

  FuzzedDataProvider data_provider(data, size);

  /* Initialise a random iv */
  nghttp2_settings_entry *iv;
  int size_of_iv = data_provider.ConsumeIntegralInRange(1, 10);
  iv = (nghttp2_settings_entry *)malloc(sizeof(nghttp2_settings_entry) *
                                        size_of_iv);
  for (int i = 0; i < size_of_iv; i++) {
    iv[i].settings_id = data_provider.ConsumeIntegralInRange(0, 1000);
    iv[i].value = data_provider.ConsumeIntegralInRange(0, 1000);
  }

  nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv, size_of_iv);
  send_pending(session);

  std::vector<uint8_t> d = data_provider.ConsumeRemainingBytes<uint8_t>();
  nghttp2_session_mem_recv2(session, d.data(), d.size());

  send_pending(session);

  nghttp2_session_del(session);

  free(iv);

  return 0;
}
