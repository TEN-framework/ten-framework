/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "nghttp2_session.h"
#include "sdkconfig.h"

// Redefine NGHTTP2_INBOUND_BUFFER_LENGTH to the value from sdkconfig
#undef NGHTTP2_INBOUND_BUFFER_LENGTH
#define NGHTTP2_INBOUND_BUFFER_LENGTH CONFIG_ESP_NGHTTP2_INBOUND_BUFFER_LENGTH
