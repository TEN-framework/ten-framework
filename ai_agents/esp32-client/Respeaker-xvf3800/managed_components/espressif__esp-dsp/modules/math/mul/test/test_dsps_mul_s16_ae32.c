// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string.h>
#include "unity.h"
#include "dsp_platform.h"
#include "esp_log.h"

#include "dsp_tests.h"
#include "dsps_mul.h"
#include "esp_attr.h"

static const char *TAG = "dsps_mul";

TEST_CASE("dsps_mul_s16 functionality", "[dsps]")
{
    int n = 64;
    int16_t x[n];
    int16_t y[n];
    int32_t temp;
    int shift = 0;
    for (int i = 0 ; i < n ; i++) {
        x[i] = i << 4;
        temp = ((int32_t)x[i] * (int32_t)x[i]) >> shift;
        y[i] = temp;
    }

    dsps_mul_s16(x, x, x, n, 1, 1, 1, shift);
    for (int i = 0 ; i < n ; i++) {
        ESP_LOGD(TAG, "x[%i] = %i  %i", i, x[i], y[i]);
        if (x[i] != y[i]) {
            TEST_ASSERT_EQUAL(x[i], y[i]);
        }
    }
}

TEST_CASE("dsps_mul_s16 benchmark", "[dsps]")
{
    const int n = 256;
    int16_t x[n];
    for (int i = 0 ; i < n ; i++) {
        x[i] = i << 4;
    }

    unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_mul_s16(x, x, x, n, 1, 1, 1, 0);
    unsigned int end_b = dsp_get_cpu_cycle_count();

    float cycles = end_b - start_b;
    ESP_LOGI(TAG, "dsps_mul_s16 - %f cycles per sample \n", cycles);
}
