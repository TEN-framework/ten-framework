//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/msg_dest_info/all_msg_type_dest_info.h"

#include "include_internal/ten_runtime/extension/msg_dest_info/msg_dest_info.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"

void ten_all_msg_type_dest_info_init(ten_all_msg_type_dest_info_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_list_init(&self->cmd);
  ten_list_init(&self->video_frame);
  ten_list_init(&self->audio_frame);
  ten_list_init(&self->data);
}

void ten_all_msg_type_dest_info_deinit(ten_all_msg_type_dest_info_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_list_clear(&self->cmd);
  ten_list_clear(&self->video_frame);
  ten_list_clear(&self->audio_frame);
  ten_list_clear(&self->data);
}

static void translate_localhost_to_app_uri_for_msg_dest(ten_shared_ptr_t *dest,
                                                        const char *uri) {
  ten_msg_dest_info_t *raw_dest = ten_shared_ptr_get_data(dest);
  ten_msg_dest_info_translate_localhost_to_app_uri(raw_dest, uri);
}

static void translate_localhost_to_app_uri_for_dest(
    ten_listnode_t *node, const char *uri,
    void (*translate_func)(ten_shared_ptr_t *dest, const char *uri)) {
  TEN_ASSERT(node, "Invalid argument.");
  TEN_ASSERT(uri, "Invalid argument.");
  TEN_ASSERT(translate_func, "Invalid argument.");

  ten_shared_ptr_t *shared_dest = ten_smart_ptr_listnode_get(node);
  TEN_ASSERT(shared_dest, "Should not happen.");

  translate_func(shared_dest, uri);
}

void ten_all_msg_type_dest_info_translate_localhost_to_app_uri(
    ten_all_msg_type_dest_info_t *self, const char *uri) {
  TEN_ASSERT(self && uri, "Invalid argument.");

  ten_list_foreach (&self->cmd, iter) {
    translate_localhost_to_app_uri_for_dest(
        iter.node, uri, translate_localhost_to_app_uri_for_msg_dest);
  }

  ten_list_foreach (&self->data, iter) {
    translate_localhost_to_app_uri_for_dest(
        iter.node, uri, translate_localhost_to_app_uri_for_msg_dest);
  }

  ten_list_foreach (&self->video_frame, iter) {
    translate_localhost_to_app_uri_for_dest(
        iter.node, uri, translate_localhost_to_app_uri_for_msg_dest);
  }

  ten_list_foreach (&self->audio_frame, iter) {
    translate_localhost_to_app_uri_for_dest(
        iter.node, uri, translate_localhost_to_app_uri_for_msg_dest);
  }
}
