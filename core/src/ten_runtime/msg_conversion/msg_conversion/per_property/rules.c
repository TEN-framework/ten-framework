//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg_conversion/msg_conversion/per_property/rules.h"

#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/field/field.h"
#include "include_internal/ten_runtime/msg/field/field_info.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/msg_conversion/msg_conversion/per_property/rule.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_kv.h"

static ten_msg_conversion_per_property_rules_t *
ten_msg_conversion_per_property_rules_create(void) {
  ten_msg_conversion_per_property_rules_t *self =
      (ten_msg_conversion_per_property_rules_t *)TEN_MALLOC(
          sizeof(ten_msg_conversion_per_property_rules_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_list_init(&self->rules);
  self->keep_original = false;

  return self;
}

void ten_msg_conversion_per_property_rules_destroy(
    ten_msg_conversion_per_property_rules_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_list_clear(&self->rules);

  TEN_FREE(self);
}

ten_shared_ptr_t *ten_msg_conversion_per_property_rules_convert(
    ten_msg_conversion_per_property_rules_t *self, ten_shared_ptr_t *msg,
    ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");

  ten_shared_ptr_t *new_msg = NULL;

  if (self->keep_original) {
    new_msg = ten_msg_clone(msg, NULL);
  } else {
    // Do _not_ clone 'properties' field.
    ten_list_t excluded_field_ids = TEN_LIST_INIT_VAL;
    ten_list_push_back(
        &excluded_field_ids,
        ten_int32_listnode_create(
            ten_msg_fields_info[TEN_MSG_FIELD_PROPERTIES].field_id));
    new_msg = ten_msg_clone(msg, &excluded_field_ids);
    ten_list_clear(&excluded_field_ids);
  }

  ten_list_foreach (&self->rules, iter) {
    ten_msg_conversion_per_property_rule_t *property_rule =
        ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(property_rule, "Invalid argument.");

    if (!ten_msg_conversion_per_property_rule_convert(property_rule, msg,
                                                      new_msg, err)) {
      ten_shared_ptr_destroy(new_msg);
      new_msg = NULL;
      break;
    }
  }

  return new_msg;
}

ten_shared_ptr_t *ten_result_conversion_per_property_rules_convert(
    ten_msg_conversion_per_property_rules_t *self, ten_shared_ptr_t *msg,
    ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");

  ten_shared_ptr_t *new_msg = NULL;

  if (self->keep_original) {
    new_msg = ten_msg_clone(msg, NULL);
  } else {
    // Do _not_ clone 'properties' field.
    ten_list_t excluded_field_ids = TEN_LIST_INIT_VAL;
    ten_list_push_back(
        &excluded_field_ids,
        ten_int32_listnode_create(
            ten_msg_fields_info[TEN_MSG_FIELD_PROPERTIES].field_id));
    new_msg = ten_msg_clone(msg, &excluded_field_ids);
    ten_list_clear(&excluded_field_ids);
  }

  // The command ID of the cloned cmd result should be equal to the original
  // cmd result.
  //
  // Note: In the TEN runtime, if a command A is cloned from a command B, then
  // the command ID of A & B must be different. However, here is the _ONLY_
  // location where the command ID of the cloned command will be equal to the
  // original command.
  ten_cmd_base_set_cmd_id(new_msg, ten_cmd_base_get_cmd_id(msg));

  // Properties.
  ten_list_foreach (&self->rules, iter) {
    ten_msg_conversion_per_property_rule_t *property_rule =
        ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(property_rule, "Invalid argument.");

    if (!ten_msg_conversion_per_property_rule_convert(property_rule, msg,
                                                      new_msg, err)) {
      ten_shared_ptr_destroy(new_msg);
      new_msg = NULL;
      break;
    }
  }

  return new_msg;
}

ten_msg_conversion_per_property_rules_t *
ten_msg_conversion_per_property_rules_from_json(ten_json_t *json,
                                                ten_error_t *err) {
  TEN_ASSERT(json, "Invalid argument.");

  ten_msg_conversion_per_property_rules_t *rules =
      ten_msg_conversion_per_property_rules_create();

  size_t index = 0;
  ten_json_t *value = NULL;
  ten_json_array_foreach(json, index, value) {
    ten_msg_conversion_per_property_rule_t *rule =
        ten_msg_conversion_per_property_rule_from_json(value, err);
    if (!rule) {
      ten_msg_conversion_per_property_rules_destroy(rules);
      return NULL;
    }

    ten_list_push_ptr_back(&rules->rules, rule,
                           (ten_ptr_listnode_destroy_func_t)
                               ten_msg_conversion_per_property_rule_destroy);
  }

  return rules;
}

bool ten_msg_conversion_per_property_rules_to_json(
    ten_msg_conversion_per_property_rules_t *self, ten_json_t *json,
    ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_json_t rules_json = TEN_JSON_INIT_VAL(json->ctx, false);
  ten_json_object_peek_or_create_array(json, TEN_STR_RULES, &rules_json);

  if (ten_list_size(&self->rules) > 0) {
    ten_list_foreach (&self->rules, iter) {
      ten_msg_conversion_per_property_rule_t *rule =
          ten_ptr_listnode_get(iter.node);
      TEN_ASSERT(rule, "Invalid argument.");

      bool success =
          ten_msg_conversion_per_property_rule_to_json(rule, &rules_json, err);
      if (!success) {
        return false;
      }
    }
  }

  return true;
}

ten_msg_conversion_per_property_rules_t *
ten_msg_conversion_per_property_rules_from_value(ten_value_t *value,
                                                 ten_error_t *err) {
  TEN_ASSERT(value, "Invalid argument.");

  if (!ten_value_is_array(value)) {
    return NULL;
  }

  ten_msg_conversion_per_property_rules_t *rules =
      ten_msg_conversion_per_property_rules_create();

  ten_list_foreach (&value->content.array, iter) {
    ten_value_t *array_item_value = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(array_item_value && ten_value_check_integrity(array_item_value),
               "Invalid argument.");

    ten_msg_conversion_per_property_rule_t *rule =
        ten_msg_conversion_per_property_rule_from_value(array_item_value, err);
    TEN_ASSERT(rule, "Invalid argument.");

    ten_list_push_ptr_back(&rules->rules, rule,
                           (ten_ptr_listnode_destroy_func_t)
                               ten_msg_conversion_per_property_rule_destroy);
  }

  return rules;
}

TEN_RUNTIME_PRIVATE_API ten_value_t *
ten_msg_conversion_per_property_rules_to_value(
    ten_msg_conversion_per_property_rules_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_t *rules_value = ten_value_create_array_with_move(NULL);

  if (ten_list_size(&self->rules) > 0) {
    ten_list_foreach (&self->rules, iter) {
      ten_msg_conversion_per_property_rule_t *rule =
          ten_ptr_listnode_get(iter.node);
      TEN_ASSERT(rule, "Invalid argument.");

      ten_value_t *rule_value =
          ten_msg_conversion_per_property_rule_to_value(rule, err);
      if (!rule_value) {
        ten_value_destroy(rules_value);
        return NULL;
      }

      ten_list_push_ptr_back(
          &rules_value->content.array, rule_value,
          (ten_ptr_listnode_destroy_func_t)ten_value_destroy);
    }
  }

  return rules_value;
}
