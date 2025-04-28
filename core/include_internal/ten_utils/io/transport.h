//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/io/transport.h"

TEN_UTILS_PRIVATE_API enum TEN_TRANSPORT_DROP_TYPE ten_transport_get_drop_type(
    ten_transport_t *self);

TEN_UTILS_PRIVATE_API void ten_transport_set_drop_type(
    ten_transport_t *self, TEN_TRANSPORT_DROP_TYPE drop_type);

TEN_UTILS_PRIVATE_API int ten_transport_drop_required(ten_transport_t *self);

TEN_UTILS_PRIVATE_API void ten_transport_set_drop_when_full(
    ten_transport_t *self, int drop);

TEN_UTILS_PRIVATE_API void ten_transport_on_close(ten_transport_t *self);
