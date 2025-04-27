//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#include "include_internal/ten_runtime/addon/protocol/protocol.h"

#include "core_protocols/msgpack/common/constant_str.h"
#include "core_protocols/msgpack/common/parser.h"
#include "core_protocols/msgpack/msg/msg.h"
#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/metadata/metadata.h"
#include "include_internal/ten_runtime/protocol/integrated/protocol_integrated.h"
#include "include_internal/ten_runtime/ten_env/metadata.h"
#include "ten_runtime/addon/addon.h"
#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

typedef struct ten_protocol_msgpack_t {
  ten_protocol_integrated_t base;

  ten_msgpack_parser_t parser;
} ten_protocol_msgpack_t;

static void ten_protocol_msgpack_on_input(ten_protocol_msgpack_t *self,
                                          ten_buf_t input_buf,
                                          ten_list_t *result_msgs) {
  TEN_ASSERT(self && ten_protocol_check_integrity(&self->base.base, true),
             "Invalid argument.");
  TEN_ASSERT(input_buf.data && input_buf.content_size && result_msgs,
             "Invalid argument.");

  ten_msgpack_deserialize_msgs(&self->parser, input_buf, result_msgs);
}

static ten_buf_t ten_protocol_msgpack_on_output(ten_protocol_msgpack_t *self,
                                                ten_list_t *output_msgs) {
  TEN_ASSERT(self && ten_protocol_check_integrity(&self->base.base, true),
             "Invalid argument.");
  TEN_ASSERT(self->base.on_output, "Invalid argument.");

  return ten_msgpack_serialize_msgs(output_msgs, NULL);
}

static void ten_protocol_msgpack_on_destroy_instance(
    TEN_UNUSED ten_addon_t *addon, ten_env_t *ten_env, void *_self,
    void *context) {
  ten_protocol_msgpack_t *self = (ten_protocol_msgpack_t *)_self;
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: The belonging thread of the 'protocol' is
                 // ended when this function is called, so we can not check
                 // thread integrity here.
                 ten_protocol_check_integrity(&self->base.base, false),
             "Invalid argument.");

  ten_msgpack_parser_deinit(&self->parser);
  ten_protocol_deinit(&self->base.base);

  TEN_FREE(self);

  ten_env_on_destroy_instance_done(ten_env, context, NULL);
}

static void ten_protocol_msgpack_on_create_instance(
    TEN_UNUSED ten_addon_t *addon, ten_env_t *ten_env,
    TEN_UNUSED const char *name, void *context) {
  ten_protocol_msgpack_t *self =
      (ten_protocol_msgpack_t *)TEN_MALLOC(sizeof(ten_protocol_msgpack_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_protocol_integrated_init(
      &self->base, TEN_STR_MSGPACK,
      (ten_protocol_integrated_on_input_func_t)ten_protocol_msgpack_on_input,
      (ten_protocol_integrated_on_output_func_t)ten_protocol_msgpack_on_output);

  // Configure the retry mechanism.
  self->base.retry_config.enable = true;
  self->base.retry_config.interval_ms = 500;
  self->base.retry_config.max_retries = 5;

  ten_msgpack_parser_init(&self->parser);

  ten_env_on_create_instance_done(ten_env, self, context, NULL);
}

static void ten_protocol_msgpack_on_init(TEN_UNUSED ten_addon_t *addon,
                                         ten_env_t *ten_env) {
  bool result = ten_env_init_manifest_from_json(ten_env,
                                                // clang-format off
                        "{\
                          \"type\": \"protocol\",\
                          \"" TEN_STR_NAME "\": \"" TEN_STR_MSGPACK "\",\
                          \"" TEN_STR_PROTOCOL "\": [\
                            \"" TEN_STR_MSGPACK "\"\
                          ],\
                          \"version\": \"1.0.0\"\
                         }",
                                                // clang-format on
                                                NULL);
  TEN_ASSERT(result, "Should not happen.");

  ten_env_on_init_done(ten_env, NULL);
}

static ten_addon_t msgpack_protocol_factory = {
    NULL,
    TEN_ADDON_SIGNATURE,
    ten_protocol_msgpack_on_init,
    NULL,
    ten_protocol_msgpack_on_create_instance,
    ten_protocol_msgpack_on_destroy_instance,
    NULL,
    NULL,
};

TEN_REGISTER_ADDON_AS_PROTOCOL(msgpack, &msgpack_protocol_factory);
