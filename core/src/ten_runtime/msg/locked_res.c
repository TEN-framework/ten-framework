//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/locked_res.h"

#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_ptr.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

static bool ten_msg_locked_res_check_integrity(ten_msg_locked_res_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_MSG_LOCKED_RES_SIGNATURE) {
    return false;
  }
  return true;
}

static void ten_msg_locked_res_init(ten_msg_locked_res_t *self,
                                    TEN_MSG_LOCKED_RES_TYPE type) {
  TEN_ASSERT(self, "Should not happen.");

  ten_signature_set(&self->signature, TEN_MSG_LOCKED_RES_SIGNATURE);
  self->type = type;
}

static ten_msg_locked_res_buf_t *ten_msg_locked_res_buf_create(
    const uint8_t *data) {
  ten_msg_locked_res_buf_t *res =
      (ten_msg_locked_res_buf_t *)TEN_MALLOC(sizeof(ten_msg_locked_res_buf_t));
  TEN_ASSERT(res, "Failed to allocate ten_msg_locked_res_buf_t.");

  ten_msg_locked_res_init((ten_msg_locked_res_t *)res,
                          TEN_MSG_LOCKED_RES_TYPE_BUF);

  res->data = data;

  return res;
}

static void ten_msg_locked_res_buf_destroy(ten_msg_locked_res_buf_t *info) {
  TEN_ASSERT(info && ten_msg_locked_res_check_integrity(&info->base),
             "Invalid argument.");

  info->data = NULL;

  TEN_FREE(info);
}

static void ten_raw_msg_add_locked_res_buf(ten_msg_t *self,
                                           const uint8_t *data) {
  TEN_ASSERT(self && ten_raw_msg_check_integrity(self), "Invalid argument.");

  ten_msg_locked_res_buf_t *res = ten_msg_locked_res_buf_create(data);
  TEN_ASSERT(res && ten_msg_locked_res_check_integrity(&res->base),
             "Should not happen.");

  ten_list_push_ptr_back(
      &self->locked_res, res,
      (ten_ptr_listnode_destroy_func_t)ten_msg_locked_res_buf_destroy);
}

bool ten_raw_msg_remove_locked_res_buf(ten_msg_t *self, const uint8_t *data) {
  TEN_ASSERT(self && ten_raw_msg_check_integrity(self), "Invalid argument.");

  if (!data) {
    return false;
  }

  ten_list_foreach (&self->locked_res, iter) {
    ten_msg_locked_res_buf_t *res = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(res && ten_msg_locked_res_check_integrity(&res->base),
               "Should not happen.");

    if (res->data == data) {
      ten_list_remove_node(&self->locked_res, iter.node);
      return true;
    }
  }

  return false;
}

bool ten_msg_add_locked_res_buf(ten_shared_ptr_t *self, const uint8_t *data,
                                ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Invalid argument.");

  if (!data) {
    TEN_LOGE("Failed to lock res, the data is null.");
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "Try to lock a NULL buf.");
    }
    return false;
  }

  ten_raw_msg_add_locked_res_buf(ten_msg_get_raw_msg(self), data);

  return true;
}

bool ten_msg_remove_locked_res_buf(ten_shared_ptr_t *self, const uint8_t *data,
                                   ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Invalid argument.");

  bool result =
      ten_raw_msg_remove_locked_res_buf(ten_msg_get_raw_msg(self), data);

  if (!result) {
    TEN_LOGE("Fatal, the locked res %p is not found.", data);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "Failed to remove locked res.");
    }
  }

  return result;
}
