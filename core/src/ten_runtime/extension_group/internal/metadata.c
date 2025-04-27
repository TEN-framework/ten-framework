//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension_group/metadata.h"

#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_group/extension_group_info/extension_group_info.h"
#include "include_internal/ten_runtime/extension_group/on_xxx.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "ten_utils/macro/check.h"

void ten_extension_group_load_metadata(ten_extension_group_t *self) {
  TEN_ASSERT(self &&
                 // This function is safe to be called from the extension
                 // threads, because all the resources it accesses are not be
                 // modified after the app initialization phase.
                 ten_extension_group_check_integrity(self, true),
             "Should not happen.");

  TEN_LOGD("[%s] Load metadata.", ten_extension_group_get_name(self, true));

  ten_metadata_load(ten_extension_group_on_init, self->ten_env);
}

void ten_extension_group_merge_properties_from_graph(
    ten_extension_group_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_extension_group_check_integrity(self, true),
             "Should not happen.");
  TEN_ASSERT(self->extension_group_info, "Invalid argument.");

  // Merge properties in extension_group_info_from_graph into the extension
  // group's property store.
  if (self->extension_group_info->property) {
    ten_value_object_merge_with_clone(&self->property,
                                      self->extension_group_info->property);
  }
}
