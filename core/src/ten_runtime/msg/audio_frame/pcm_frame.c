//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/msg/audio_frame/audio_frame.h"
#include "include_internal/ten_runtime/msg/audio_frame/field/field_info.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_utils/value/value_path.h"
#include "include_internal/ten_utils/value/value_set.h"
#include "ten_runtime/msg/audio_frame/audio_frame.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"

bool ten_raw_audio_frame_check_integrity(ten_audio_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_AUDIO_FRAME_SIGNATURE) {
    return false;
  }

  if (self->msg_hdr.type != TEN_MSG_TYPE_AUDIO_FRAME) {
    return false;
  }

  return true;
}

int32_t ten_raw_audio_frame_get_samples_per_channel(ten_audio_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_get_int32(&self->samples_per_channel, NULL);
}

ten_buf_t *ten_raw_audio_frame_peek_buf(ten_audio_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_peek_buf(&self->buf, NULL);
}

int32_t ten_raw_audio_frame_get_sample_rate(ten_audio_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_get_int32(&self->sample_rate, NULL);
}

uint64_t ten_raw_audio_frame_get_channel_layout(ten_audio_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_get_uint64(&self->channel_layout, NULL);
}

bool ten_raw_audio_frame_is_eof(ten_audio_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_get_bool(&self->is_eof, NULL);
}

int32_t ten_raw_audio_frame_get_line_size(ten_audio_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_get_int32(&self->line_size, NULL);
}

int32_t ten_raw_audio_frame_get_bytes_per_sample(ten_audio_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_get_int32(&self->bytes_per_sample, NULL);
}

int32_t ten_raw_audio_frame_get_number_of_channel(ten_audio_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_get_int32(&self->number_of_channel, NULL);
}

TEN_AUDIO_FRAME_DATA_FMT ten_raw_audio_frame_get_data_fmt(
    ten_audio_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_get_int32(&self->data_fmt, NULL);
}

int64_t ten_raw_audio_frame_get_timestamp(ten_audio_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_get_int64(&self->timestamp, NULL);
}

bool ten_raw_audio_frame_set_samples_per_channel(ten_audio_frame_t *self,
                                                 int32_t samples_per_channel) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_set_int32(&self->samples_per_channel, samples_per_channel);
}

bool ten_raw_audio_frame_set_sample_rate(ten_audio_frame_t *self,
                                         int32_t sample_rate) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_set_int32(&self->sample_rate, sample_rate);
}

bool ten_raw_audio_frame_set_channel_layout(ten_audio_frame_t *self,
                                            uint64_t channel_layout) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_set_uint64(&self->channel_layout, channel_layout);
}

bool ten_raw_audio_frame_set_eof(ten_audio_frame_t *self, bool is_eof) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_set_bool(&self->is_eof, is_eof);
}

bool ten_raw_audio_frame_set_line_size(ten_audio_frame_t *self,
                                       int32_t line_size) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_set_int32(&self->line_size, line_size);
}

bool ten_raw_audio_frame_set_bytes_per_sample(ten_audio_frame_t *self,
                                              int32_t bytes_per_sample) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_set_int32(&self->bytes_per_sample, bytes_per_sample);
}

bool ten_raw_audio_frame_set_number_of_channel(ten_audio_frame_t *self,
                                               int32_t number) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_set_int32(&self->number_of_channel, number);
}

bool ten_raw_audio_frame_set_data_fmt(ten_audio_frame_t *self,
                                      TEN_AUDIO_FRAME_DATA_FMT data_fmt) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_set_int32(&self->data_fmt, data_fmt);
}

bool ten_raw_audio_frame_set_timestamp(ten_audio_frame_t *self,
                                       int64_t timestamp) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_value_set_int64(&self->timestamp, timestamp);
}

static uint8_t *ten_raw_audio_frame_alloc_buf(ten_audio_frame_t *self,
                                              size_t size) {
  if (size <= 0) {
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }

  ten_value_init_buf(&self->buf, size);

  return ten_value_peek_buf(&self->buf, NULL)->data;
}

static void ten_raw_audio_frame_init(ten_audio_frame_t *self) {
  ten_raw_msg_init(&self->msg_hdr, TEN_MSG_TYPE_AUDIO_FRAME);
  ten_signature_set(&self->signature, TEN_AUDIO_FRAME_SIGNATURE);

  ten_value_init_int64(&self->timestamp, 0);
  ten_value_init_uint64(&self->channel_layout, 0);

  ten_value_init_int32(&self->sample_rate, 0);
  ten_value_init_int32(&self->bytes_per_sample, 0);
  ten_value_init_int32(&self->samples_per_channel, 0);
  ten_value_init_int32(&self->number_of_channel, 0);
  ten_value_init_int32(&self->data_fmt, TEN_AUDIO_FRAME_DATA_FMT_INTERLEAVE);
  ten_value_init_int32(&self->line_size, 0);
  ten_value_init_buf(&self->buf, 0);
  ten_value_init_bool(&self->is_eof, false);
}

static ten_audio_frame_t *ten_raw_audio_frame_create_empty(void) {
  ten_audio_frame_t *self = TEN_MALLOC(sizeof(ten_audio_frame_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_raw_audio_frame_init(self);

  return self;
}

static ten_audio_frame_t *ten_raw_audio_frame_create(const char *name,
                                                     ten_error_t *err) {
  ten_audio_frame_t *self = ten_raw_audio_frame_create_empty();
  ten_raw_msg_set_name((ten_msg_t *)self, name, err);

  return self;
}

static ten_audio_frame_t *ten_raw_audio_frame_create_with_len(
    const char *name, size_t name_len, ten_error_t *err) {
  ten_audio_frame_t *self = ten_raw_audio_frame_create_empty();
  ten_raw_msg_set_name_with_len((ten_msg_t *)self, name, name_len, err);

  return self;
}

ten_shared_ptr_t *ten_audio_frame_create_empty(void) {
  return ten_shared_ptr_create(ten_raw_audio_frame_create_empty(),
                               ten_raw_audio_frame_destroy);
}

ten_shared_ptr_t *ten_audio_frame_create(const char *name, ten_error_t *err) {
  return ten_shared_ptr_create(ten_raw_audio_frame_create(name, err),
                               ten_raw_audio_frame_destroy);
}

ten_shared_ptr_t *ten_audio_frame_create_with_name_len(const char *name,
                                                       size_t name_len,
                                                       ten_error_t *err) {
  return ten_shared_ptr_create(
      ten_raw_audio_frame_create_with_len(name, name_len, err),
      ten_raw_audio_frame_destroy);
}

void ten_raw_audio_frame_destroy(ten_audio_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_raw_msg_deinit(&self->msg_hdr);
  ten_value_deinit(&self->buf);

  TEN_FREE(self);
}

int64_t ten_audio_frame_get_timestamp(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_get_timestamp(ten_shared_ptr_get_data(self));
}

bool ten_audio_frame_set_timestamp(ten_shared_ptr_t *self, int64_t timestamp) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_set_timestamp(ten_shared_ptr_get_data(self),
                                           timestamp);
}

int32_t ten_audio_frame_get_sample_rate(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_get_sample_rate(ten_shared_ptr_get_data(self));
}

bool ten_audio_frame_set_sample_rate(ten_shared_ptr_t *self,
                                     int32_t sample_rate) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_set_sample_rate(ten_shared_ptr_get_data(self),
                                             sample_rate);
}

uint64_t ten_audio_frame_get_channel_layout(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_get_channel_layout(ten_shared_ptr_get_data(self));
}

bool ten_audio_frame_set_channel_layout(ten_shared_ptr_t *self,
                                        uint64_t channel_layout) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_set_channel_layout(ten_shared_ptr_get_data(self),
                                                channel_layout);
}

bool ten_audio_frame_is_eof(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_is_eof(ten_shared_ptr_get_data(self));
}

bool ten_audio_frame_set_eof(ten_shared_ptr_t *self, bool is_eof) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_set_eof(ten_shared_ptr_get_data(self), is_eof);
}

int32_t ten_audio_frame_get_samples_per_channel(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_get_samples_per_channel(
      ten_shared_ptr_get_data(self));
}

bool ten_audio_frame_set_samples_per_channel(ten_shared_ptr_t *self,
                                             int32_t samples_per_channel) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_set_samples_per_channel(
      ten_shared_ptr_get_data(self), samples_per_channel);
}

ten_buf_t *ten_audio_frame_peek_buf(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_peek_buf(ten_shared_ptr_get_data(self));
}

int32_t ten_audio_frame_get_line_size(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_get_line_size(ten_shared_ptr_get_data(self));
}

bool ten_audio_frame_set_line_size(ten_shared_ptr_t *self, int32_t line_size) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_set_line_size(ten_shared_ptr_get_data(self),
                                           line_size);
}

int32_t ten_audio_frame_get_bytes_per_sample(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_get_bytes_per_sample(
      ten_shared_ptr_get_data(self));
}

bool ten_audio_frame_set_bytes_per_sample(ten_shared_ptr_t *self,
                                          int32_t size) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_set_bytes_per_sample(ten_shared_ptr_get_data(self),
                                                  size);
}

int32_t ten_audio_frame_get_number_of_channel(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_get_number_of_channel(
      ten_shared_ptr_get_data(self));
}

bool ten_audio_frame_set_number_of_channel(ten_shared_ptr_t *self,
                                           int32_t number) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_set_number_of_channel(
      ten_shared_ptr_get_data(self), number);
}

TEN_AUDIO_FRAME_DATA_FMT ten_audio_frame_get_data_fmt(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_get_data_fmt(ten_shared_ptr_get_data(self));
}

bool ten_audio_frame_set_data_fmt(ten_shared_ptr_t *self,
                                  TEN_AUDIO_FRAME_DATA_FMT data_fmt) {
  TEN_ASSERT(self, "Should not happen.");
  return ten_raw_audio_frame_set_data_fmt(ten_shared_ptr_get_data(self),
                                          data_fmt);
}

uint8_t *ten_audio_frame_alloc_buf(ten_shared_ptr_t *self, size_t size) {
  TEN_ASSERT(self, "Should not happen.");

  return ten_raw_audio_frame_alloc_buf(ten_shared_ptr_get_data(self), size);
}

ten_msg_t *ten_raw_audio_frame_as_msg_clone(ten_msg_t *self,
                                            ten_list_t *excluded_field_ids) {
  TEN_ASSERT(self && ten_raw_msg_check_integrity(self) &&
                 ten_raw_msg_get_type(self) == TEN_MSG_TYPE_AUDIO_FRAME,
             "Should not happen.");

  ten_audio_frame_t *new_frame =
      (ten_audio_frame_t *)TEN_MALLOC(sizeof(ten_audio_frame_t));
  TEN_ASSERT(new_frame, "Failed to allocate memory.");

  ten_raw_audio_frame_init(new_frame);

  ten_audio_frame_t *self_frame = (ten_audio_frame_t *)self;

  ten_value_copy(&self_frame->timestamp, &new_frame->timestamp);
  ten_value_copy(&self_frame->sample_rate, &new_frame->sample_rate);
  ten_value_copy(&self_frame->bytes_per_sample, &new_frame->bytes_per_sample);
  ten_value_copy(&self_frame->samples_per_channel,
                 &new_frame->samples_per_channel);
  ten_value_copy(&self_frame->number_of_channel, &new_frame->number_of_channel);
  ten_value_copy(&self_frame->channel_layout, &new_frame->channel_layout);
  ten_value_copy(&self_frame->data_fmt, &new_frame->data_fmt);
  ten_value_copy(&self_frame->line_size, &new_frame->line_size);
  ten_value_copy(&self_frame->is_eof, &new_frame->is_eof);
  ten_value_copy(&self_frame->buf, &new_frame->buf);

  for (size_t i = 0; i < ten_audio_frame_fields_info_size; ++i) {
    if (excluded_field_ids) {
      bool skip = false;

      ten_list_foreach (excluded_field_ids, iter) {
        if (ten_audio_frame_fields_info[i].field_id ==
            ten_int32_listnode_get(iter.node)) {
          skip = true;
          break;
        }
      }

      if (skip) {
        continue;
      }
    }

    ten_msg_copy_field_func_t copy_field =
        ten_audio_frame_fields_info[i].copy_field;
    if (copy_field) {
      copy_field((ten_msg_t *)new_frame, self, excluded_field_ids);
    }
  }

  return (ten_msg_t *)new_frame;
}

ten_value_t *ten_raw_audio_frame_peek_ten_property(ten_msg_t *self,
                                                   ten_list_t *paths,
                                                   ten_error_t *err) {
  TEN_ASSERT(self && ten_raw_msg_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(paths && ten_list_check_integrity(paths),
             "path should not be empty.");

  ten_value_t *result = NULL;

  ten_error_t tmp_err;
  bool use_tmp_err = false;
  if (!err) {
    use_tmp_err = true;
    ten_error_init(&tmp_err);
    err = &tmp_err;
  }

  ten_audio_frame_t *audio_frame = (ten_audio_frame_t *)self;

  ten_list_foreach (paths, item_iter) {
    ten_value_path_item_t *item = ten_ptr_listnode_get(item_iter.node);
    TEN_ASSERT(item, "Invalid argument.");

    switch (item->type) {
    case TEN_VALUE_PATH_ITEM_TYPE_OBJECT_ITEM: {
      if (!strcmp(TEN_STR_BYTES_PER_SAMPLE,
                  ten_string_get_raw_str(&item->obj_item_str))) {
        result = &audio_frame->bytes_per_sample;
      } else if (!strcmp(TEN_STR_TIMESTAMP,
                         ten_string_get_raw_str(&item->obj_item_str))) {
        result = &audio_frame->timestamp;
      } else if (!strcmp(TEN_STR_CHANNEL_LAYOUT,
                         ten_string_get_raw_str(&item->obj_item_str))) {
        result = &audio_frame->channel_layout;
      } else if (!strcmp(TEN_STR_DATA_FMT,
                         ten_string_get_raw_str(&item->obj_item_str))) {
        result = &audio_frame->data_fmt;
      } else if (!strcmp(TEN_STR_IS_EOF,
                         ten_string_get_raw_str(&item->obj_item_str))) {
        result = &audio_frame->is_eof;
      } else if (!strcmp(TEN_STR_LINE_SIZE,
                         ten_string_get_raw_str(&item->obj_item_str))) {
        result = &audio_frame->line_size;
      } else if (!strcmp(TEN_STR_NUMBER_OF_CHANNEL,
                         ten_string_get_raw_str(&item->obj_item_str))) {
        result = &audio_frame->number_of_channel;
      } else if (!strcmp(TEN_STR_SAMPLE_RATE,
                         ten_string_get_raw_str(&item->obj_item_str))) {
        result = &audio_frame->sample_rate;
      } else if (!strcmp(TEN_STR_SAMPLES_PER_CHANNEL,
                         ten_string_get_raw_str(&item->obj_item_str))) {
        result = &audio_frame->samples_per_channel;
      }
      break;
    }

    default:
      break;
    }
  }

  if (use_tmp_err) {
    ten_error_deinit(&tmp_err);
  }

  return result;
}

bool ten_raw_audio_frame_loop_all_fields(
    ten_msg_t *self, ten_raw_msg_process_one_field_func_t cb, void *user_data,
    ten_error_t *err) {
  TEN_ASSERT(
      self && ten_raw_audio_frame_check_integrity((ten_audio_frame_t *)self),
      "Invalid argument.");

  for (size_t i = 0; i < ten_audio_frame_fields_info_size; ++i) {
    ten_msg_process_field_func_t process_field =
        ten_audio_frame_fields_info[i].process_field;
    if (process_field) {
      if (!process_field(self, cb, user_data, err)) {
        return false;
      }
    }
  }

  return true;
}
