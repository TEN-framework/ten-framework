//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

// The brief closing flow of the 'integrated' protocol is as follows:
//
// 1. If the protocol is being closed from the ten world, ex: the remote is
//    closed.
//
// ten_remote_t   ten_connection_t    ten_protocol_t  ten_protocol_integrated_t
// -----------------------------------------------------------------------------
// close() {
//   is_closing = 1
// }
//
//                  close() {
//                    is_closing = 1
//                  }
//
//                                      close() {
//                                        is_closing = 1
//                                      }
//
//                                                         close() {
//                                                           close_stream()
//                                                         }
//
//                                                         on_stream_closed() {
//                                                           on_closed()
//                                                         }
//
//                                       // on_impl_closed
//                                       do_close() {
//                                         is_closed = true
//                                         on_closed()
//                                       }
//
//                  // on_protocol_closed
//                  do_close() {
//                    is_closed = true
//                    on_closed()
//                  }
//
// // on_connection_closed
// do_close() {
//    is_closed = true
//    on_closed()
// }
//
// 2. If the protocol is being closed from itself, ex: the physical connection
//    is broken.
//
// ten_remote_t   ten_connection_t    ten_protocol_t  ten_protocol_integrated_t
// -----------------------------------------------------------------------------
//                                                       on_stream_closed() {
//                                                         ten_protocol_close()
//                                                       }
//
//                                    // ten_protocol_close
//                                    close() {
//                                      is_closing = 1
//                                    }
//
//                                                       close() {
//                                                         // the stream is
//                                                         // already closed
//                                                         on_closed()
//                                                       }
//
//                                    do_close() {
//                                      is_closed = true
//                                      on_closed()
//                                    }
//
//                  // on_protocol_closed
//                  do_close() {
//                    close()
//                  }
//
//                  close() {
//                    is_closing = 1
//                    // the protocol is already closed
//                    on_closed()
//                  }
//
// // on_connection_closed
// do_close() {
//   close()
// }
//
// close() {
//   is_closing = 1
//   // the connection is already closed
//   on_closed()
// }
//

typedef struct ten_protocol_integrated_t ten_protocol_integrated_t;

TEN_RUNTIME_PRIVATE_API void ten_protocol_integrated_on_close(
    ten_protocol_integrated_t *self);

TEN_RUNTIME_PRIVATE_API void ten_protocol_integrated_on_stream_closed(
    ten_protocol_integrated_t *self);

TEN_RUNTIME_PRIVATE_API void ten_protocol_integrated_on_transport_closed(
    ten_protocol_integrated_t *self);

TEN_RUNTIME_PRIVATE_API void ten_protocol_integrated_close(
    ten_protocol_integrated_t *self);
