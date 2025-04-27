//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/msg/audio_frame/audio_frame.h"
#include "ten_runtime/msg/data/data.h"
#include "ten_runtime/msg/video_frame/video_frame.h"
#include "ten_utils/lib/smart_ptr.h"

TEST(TenMsgTest, Create) {
  ten_shared_ptr_t *data = ten_data_create("test", nullptr);
  ten_shared_ptr_destroy(data);

  ten_shared_ptr_t *audio_frame = ten_audio_frame_create("test", nullptr);
  ten_shared_ptr_destroy(audio_frame);

  ten_shared_ptr_t *video_frame = ten_video_frame_create("test", nullptr);
  ten_shared_ptr_destroy(video_frame);
}

TEST(TenMsgTest, CmdClone) {
  auto cmd = ten::cmd_t::create("test", nullptr);
  cmd->set_property("int", 1);
  cmd->set_property("str", "test");
  cmd->set_property("bool", true);

  auto *buf_data = static_cast<uint8_t *>(ten_malloc(10));
  std::memset(buf_data, 1, 10);
  ten::buf_t buf(buf_data, 10);
  cmd->set_property("buf", buf);

  auto cloned_cmd = cmd->clone();

  EXPECT_EQ(cloned_cmd->get_name(), "test");
  EXPECT_EQ(cloned_cmd->get_property_int32("int"), 1);
  EXPECT_EQ(cloned_cmd->get_property_string("str"), "test");
  EXPECT_EQ(cloned_cmd->get_property_bool("bool"), true);

  ten::buf_t cloned_buf = cloned_cmd->get_property_buf("buf");
  EXPECT_EQ(cloned_buf.size(), 10);
  EXPECT_EQ(std::memcmp(cloned_buf.data(), buf_data, 10), 0);

  ten_free(buf_data);
}

TEST(TenMsgTest, DataClone) {
  auto data = ten::data_t::create("test", nullptr);
  data->alloc_buf(10);
  ten::buf_t buf = data->lock_buf();
  std::memset(buf.data(), 1, 10);
  data->unlock_buf(buf);
  data->set_property("test_prop", "test_prop_value");

  auto cloned_data = data->clone();

  EXPECT_EQ(cloned_data->get_name(), "test");
  EXPECT_EQ(cloned_data->get_property_string("test_prop"), "test_prop_value");
  EXPECT_EQ(cloned_data->get_buf().size(), 10);

  auto cloned_buf = cloned_data->get_buf();
  auto origin_buf = data->get_buf();
  EXPECT_EQ(std::memcmp(cloned_buf.data(), origin_buf.data(), 10), 0);
}

TEST(TenMsgTest, VideoFrameClone) {
  auto video_frame = ten::video_frame_t::create("test", nullptr);
  video_frame->alloc_buf(10);
  ten::buf_t buf = video_frame->lock_buf();
  std::memset(buf.data(), 1, 10);
  video_frame->unlock_buf(buf);

  video_frame->set_property("test_prop", "test_prop_value");

  video_frame->set_width(320);
  video_frame->set_height(240);
  video_frame->set_pixel_fmt(TEN_PIXEL_FMT_I420);
  video_frame->set_timestamp(1234567890);
  video_frame->set_eof(true);

  auto cloned_video_frame = video_frame->clone();

  EXPECT_EQ(cloned_video_frame->get_name(), "test");
  EXPECT_EQ(cloned_video_frame->get_property_string("test_prop"),
            "test_prop_value");

  auto cloned_buf = cloned_video_frame->lock_buf();
  auto origin_buf = video_frame->lock_buf();
  EXPECT_EQ(std::memcmp(cloned_buf.data(), origin_buf.data(), 10), 0);
  video_frame->unlock_buf(origin_buf);
  cloned_video_frame->unlock_buf(cloned_buf);

  EXPECT_EQ(cloned_video_frame->get_width(), 320);
  EXPECT_EQ(cloned_video_frame->get_height(), 240);
  EXPECT_EQ(cloned_video_frame->get_pixel_fmt(), TEN_PIXEL_FMT_I420);
  EXPECT_EQ(cloned_video_frame->get_timestamp(), 1234567890);
  EXPECT_EQ(cloned_video_frame->is_eof(), true);
}

TEST(TenMsgTest, AudioFrameClone) {
  auto audio_frame = ten::audio_frame_t::create("test", nullptr);
  audio_frame->alloc_buf(10);
  ten::buf_t buf = audio_frame->lock_buf();
  std::memset(buf.data(), 1, 10);
  audio_frame->unlock_buf(buf);

  audio_frame->set_property("test_prop", "test_prop_value");

  audio_frame->set_bytes_per_sample(2);
  audio_frame->set_sample_rate(48000);
  audio_frame->set_samples_per_channel(2);
  audio_frame->set_number_of_channels(2);
  audio_frame->set_line_size(10);
  audio_frame->set_timestamp(1234567890);
  audio_frame->set_eof(false);
  audio_frame->set_data_fmt(TEN_AUDIO_FRAME_DATA_FMT_INTERLEAVE);

  auto cloned_audio_frame = audio_frame->clone();

  EXPECT_EQ(cloned_audio_frame->get_name(), "test");
  EXPECT_EQ(cloned_audio_frame->get_property_string("test_prop"),
            "test_prop_value");

  auto cloned_buf = cloned_audio_frame->lock_buf();
  auto origin_buf = audio_frame->lock_buf();
  EXPECT_EQ(std::memcmp(cloned_buf.data(), origin_buf.data(), 10), 0);
  audio_frame->unlock_buf(origin_buf);
  cloned_audio_frame->unlock_buf(cloned_buf);

  EXPECT_EQ(cloned_audio_frame->get_bytes_per_sample(), 2);
  EXPECT_EQ(cloned_audio_frame->get_sample_rate(), 48000);
  EXPECT_EQ(cloned_audio_frame->get_samples_per_channel(), 2);
  EXPECT_EQ(cloned_audio_frame->get_number_of_channels(), 2);
  EXPECT_EQ(cloned_audio_frame->get_line_size(), 10);
  EXPECT_EQ(cloned_audio_frame->get_data_fmt(),
            TEN_AUDIO_FRAME_DATA_FMT_INTERLEAVE);
  EXPECT_EQ(cloned_audio_frame->is_eof(), false);
}
