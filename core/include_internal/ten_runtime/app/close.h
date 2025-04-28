//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "ten_runtime/app/app.h"

// The convention of the closing flow in the TEN runtime is as follows. There
// are 3 stages in the overall closing flow.
//
// - Stage 1 : Notification stage
//   Notify the containing resources that we are going to close. This stage is
//   just a notification stage, no actual closing operations are performed. In
//   the actual code, this stage is usually accomplished through setting a
//   'is_closing' flag to 'true'.
//
//   app -> (notify) ->
//            engine -> (notify) ->
//                        remote -> (notify) ->
//                                connection -> (notify) ->
//                                                   ... -> ...
//
// - Stage 2 : Closing flow execution stage
//   When a containing resource is closed, it will notify the container that 'I
//   cam closed'. When all the containing resources of a container are closed,
//   then the container can now start to actually perform the closing
//   operations. In the actual code, this stage is usually accomplished through
//   setting a 'is_closed' flag to 'true'.
//
//                                                      ... <- ...
//                                  remote <- (I am closed) <-
//                 engine <- (I am closed) <-
//   app <- (I am closed) <-
//
// - Stage 3 : Destroy stage
//   When all the relevant resources are closed, the top-level resource would
//   trigger the 'destroy' function, and it will destroy all the containing
//   resources and itself at once.
//
// The overall time sequence would be as follows.
//
//      Stage 1                            Stage 2
//       start                              done
//         |<-------------------------------->|--> perform 'destroy' safely
//         ^                                  ^
//   is_closing=true                     is_closed=true
//
// The convention of the functions relevant to the closing flow are as follows.
//
// - xxx_close_async()
//   Trigger the closing flow in different threads.
//
// - xxx_close()
//   Perform operations in the above 'stage 1'.
//
// - xxx_is_closing()
//   xxx is in its closing flow. In other words, xxx_close() has been called.
//
// - xxx_could_be_close()
//   All the containing resources of xxx are closed.
//
// - xxx_on_close()
//   When a containing resource is closed, call this function to check if the
//   container can go into the above 'stage 2'.
//
// - xxx_do_close()
//   Perform operations in the above 'stage 2'.

typedef struct ten_app_t ten_app_t;
typedef struct ten_engine_t ten_engine_t;
typedef struct ten_connection_t ten_connection_t;
typedef struct ten_protocol_t ten_protocol_t;

TEN_RUNTIME_PRIVATE_API bool ten_app_is_closing(ten_app_t *self);

TEN_RUNTIME_PRIVATE_API void ten_app_check_termination_when_engine_closed(
    ten_app_t *self, ten_engine_t *engine);

TEN_RUNTIME_PRIVATE_API void ten_app_on_protocol_closed(
    ten_protocol_t *protocol, void *on_closed_data);

TEN_RUNTIME_PRIVATE_API void ten_app_on_orphan_connection_closed(
    ten_connection_t *connection, void *on_closed_data);
