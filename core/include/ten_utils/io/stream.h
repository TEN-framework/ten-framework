//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once
#include "ten_utils/ten_config.h"

#include <stdbool.h>
#include <stdint.h>

#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/signature.h"

#define TEN_STREAM_SIGNATURE 0xDE552052E7F8EE10U
#define TEN_STREAM_DEFAULT_BUF_SIZE (64 * 1024)

typedef struct ten_stream_t ten_stream_t;
typedef struct ten_transport_t ten_transport_t;
typedef struct ten_streambackend_t ten_streambackend_t;

struct ten_stream_t {
  ten_signature_t signature;
  ten_atomic_t close;

  ten_transport_t *transport;
  ten_streambackend_t *backend;

  void *user_data;

  void (*on_message_read)(ten_stream_t *stream, void *msg, int size);
  void (*on_message_sent)(ten_stream_t *stream, int status, void *args);
  void (*on_message_free)(ten_stream_t *stream, int status, void *args);

  void (*on_closed)(void *on_closed_data);
  void *on_closed_data;
};

// Public interface
/**
 * @brief Begin read from stream.
 * @param self The stream to read from.
 * @return 0 if success, otherwise -1.
 */
TEN_UTILS_API int ten_stream_start_read(ten_stream_t *self);

/**
 * @brief Stop read from stream.
 * @param self The stream to read from.
 * @return 0 if success, otherwise -1.
 */
TEN_UTILS_API int ten_stream_stop_read(ten_stream_t *self);

/**
 * @brief Send a message to stream.
 * @param self The stream to send to.
 * @param msg The message to send.
 * @param size The size of message.
 * @return 0 if success, otherwise -1.
 */
TEN_UTILS_API int ten_stream_send(ten_stream_t *self, const char *msg,
                                  uint32_t size, void *user_data);

/**
 * @brief Close the stream.
 * @param self The stream to close.
 */
TEN_UTILS_API void ten_stream_close(ten_stream_t *self);

/**
 * @brief Set close callback for stream.
 * @param self The stream to set close callback for.
 * @param close_cb The callback to set.
 * @param close_cb_data The args of |close_cb| when it's been called
 */
TEN_UTILS_API void ten_stream_set_on_closed(ten_stream_t *self, void *on_closed,
                                            void *on_closed_data);

/**
 * @brief Migrate a stream from one runloop to another.
 *
 * @param self The stream to migrate
 * @param from The runloop to migrate from
 * @param to The runloop to migrate to
 * @param user_data The user data to be passed to the callback
 * @param cb The callback to be called when the migration is done
 *
 * @return 0 on success, -1 on failure
 *
 * @note 1. |cb| will be called in |to| loop thread if success
 *       2. |from| and |to| have to be the same implementation type
 *          (e.g.event2, uv, etc.)
 *       3. |self| will be removed from |from| loop and no more data
 *          will be read from it
 */
TEN_UTILS_API int ten_stream_migrate(ten_stream_t *self, ten_runloop_t *from,
                                     ten_runloop_t *to, void **user_data,
                                     void (*cb)(ten_stream_t *new_stream,
                                                void **user_data));

TEN_UTILS_API bool ten_stream_check_integrity(ten_stream_t *self);

TEN_UTILS_API void ten_stream_init(ten_stream_t *self);

TEN_UTILS_API void ten_stream_on_close(ten_stream_t *self);
