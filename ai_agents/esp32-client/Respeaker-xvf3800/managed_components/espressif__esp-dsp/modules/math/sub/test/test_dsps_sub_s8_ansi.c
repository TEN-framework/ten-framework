/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "unity.h"
#include "dsp_platform.h"
#include "esp_log.h"

#include "dsp_tests.h"
#include "dsps_sub.h"
#include "esp_attr.h"

static const char *TAG = "dsps_sub";

TEST_CASE("dsps_sub_s8_ansi functionality", "[dsps]")
{
    int n = 64;
    int8_t x[n];
    int8_t y[n];
    int32_t temp;
    int shift = 0;
    for (int i = 0 ; i < n ; i++) {
        x[i] = i - n / 2;
        temp = ((int32_t)x[i] - (int32_t)x[i]) >> shift;
        y[i] = temp;
    }

    dsps_sub_s8_ansi(x, x, x, n, 1, 1, 1, 0);
    for (int i = 0 ; i < n ; i++) {
        if (x[i] != y[i]) {
            TEST_ASSERT_EQUAL(x[i], y[i]);
        }
    }
}

TEST_CASE("dsps_sub_s8_ansi benchmark", "[dsps]")
{
    const int n = 256;
    int8_t x[n];
    for (int i = 0 ; i < n ; i++) {
        x[i] = i << 4;
    }

    unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_sub_s8_ansi(x, x, x, n, 1, 1, 1, 0);
    unsigned int end_b = dsp_get_cpu_cycle_count();

    float cycles = end_b - start_b;
    ESP_LOGI(TAG, "dsps_sub_s8_ansi - %f cycles per sample \n", cycles);
}
