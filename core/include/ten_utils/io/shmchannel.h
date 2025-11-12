//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/io/runloop.h"

typedef struct ten_shm_channel_t ten_shm_channel_t;

TEN_UTILS_API int ten_shm_channel_create(const char *name,
                                         ten_shm_channel_t *channel[2]);

TEN_UTILS_API void ten_shm_channel_close(ten_shm_channel_t *channel);

TEN_UTILS_API int ten_shm_channel_active(ten_shm_channel_t *channel, int read);

TEN_UTILS_API int ten_shm_channel_inactive(ten_shm_channel_t *channel,
                                           int read);

TEN_UTILS_API int ten_shm_channel_wait_remote(ten_shm_channel_t *channel,
                                              int wait_ms);

TEN_UTILS_API int ten_shm_channel_send(ten_shm_channel_t *channel, void *data,
                                       size_t size, int nonblock);

TEN_UTILS_API int ten_shm_channel_recv(ten_shm_channel_t *channel, void *data,
                                       size_t size, int nonblock);

TEN_UTILS_API int ten_shm_channel_get_capacity(ten_shm_channel_t *channel);

TEN_UTILS_API int ten_shm_channel_set_signal(ten_shm_channel_t *channel,
                                             ten_runloop_async_t *signal,
                                             int read);
