//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/ten_env/ten_env.h"

extern inline ten_extension_t *ten_env_get_attached_extension(ten_env_t *self);

extern inline ten_extension_group_t *ten_env_get_attached_extension_group(
    ten_env_t *self);

extern inline ten_app_t *ten_env_get_attached_app(ten_env_t *self);

extern inline ten_addon_host_t *ten_env_get_attached_addon(ten_env_t *self);

extern inline ten_engine_t *ten_env_get_attached_engine(ten_env_t *self);

extern inline ten_addon_loader_t *ten_env_get_attached_addon_loader(
    ten_env_t *self);
