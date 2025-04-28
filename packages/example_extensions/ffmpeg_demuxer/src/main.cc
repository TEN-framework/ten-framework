//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#include "demuxer_thread.h"
#include "ten_runtime/binding/cpp/ten.h"

namespace ten {
namespace ffmpeg_extension {

class demuxer_extension_t : public extension_t {
 public:
  explicit demuxer_extension_t(const char *name) : extension_t(name) {}

  void on_start(ten_env_t &ten_env) override { ten_env.on_start_done(); }

  void on_cmd(ten_env_t &ten_env, std::unique_ptr<ten::cmd_t> cmd) override {
    const auto cmd_name = cmd->get_name();

    if (std::string(cmd_name) == "prepare_demuxer") {
      auto input_stream_name = cmd->get_property_string("input_stream");
      if (input_stream_name.empty()) {
        input_stream_name =
            "ten_packages/extension/ffmpeg_demuxer/res/test.mp4";
      }

      auto *ten_env_proxy = ten::ten_env_proxy_t::create(ten_env);

      // Start the demuxer thread. ffmpeg is living in its own thread.
      demuxer_thread_ = new demuxer_thread_t(ten_env_proxy, std::move(cmd),
                                             this, input_stream_name);
      demuxer_thread_->start();
      // The cmd will be replied in the demuxer thread.
    }

    if (std::string(cmd_name) == "start_demuxer") {
      if (demuxer_thread_ == nullptr) {
        auto cmd_result =
            ten::cmd_result_t::create(TEN_STATUS_CODE_ERROR, *cmd);
        cmd_result->set_property("detail", "You should prepare first.");
        ten_env.return_result(std::move(cmd_result));
      } else {
        demuxer_thread_->start_demuxing();
        auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
        cmd_result->set_property("detail", "The demuxer has been started.");
        ten_env.return_result(std::move(cmd_result));
      }
    }
  }

  void on_stop(TEN_UNUSED ten_env_t &ten_env) override {
    // Stop the demuxer thread. ffmpeg is living in its own thread.
    if (demuxer_thread_ != nullptr) {
      demuxer_thread_->stop();
      demuxer_thread_->wait_for_stop_completed();
      delete demuxer_thread_;
    }
    ten_env.on_stop_done();
  }

 private:
  demuxer_thread_t *demuxer_thread_{};
};

}  // namespace ffmpeg_extension
}  // namespace ten

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(ffmpeg_demuxer,
                                    ten::ffmpeg_extension::demuxer_extension_t);
