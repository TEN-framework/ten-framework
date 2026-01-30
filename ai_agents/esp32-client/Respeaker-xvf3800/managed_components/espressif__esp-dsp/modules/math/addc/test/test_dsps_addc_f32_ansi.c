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
#include "dsps_addc.h"
#include "esp_attr.h"

static const char *TAG = "dsps_addc";


TEST_CASE("dsps_addc_f32_ansi functionality", "[dsps]")
{
    int n = 64;
    float x[n];
    float y[n];
    for (int i = 0 ; i < n ; i++) {
        x[i] = i;
        y[i] = i + 10;
    }
    dsps_addc_f32_ansi(x, x, n, 10, 1, 1);
    for (int i = 0 ; i < n ; i++) {
        if (x[i] != y[i]) {
            TEST_ASSERT_EQUAL(x[i], y[i]);
        }
    }
}

TEST_CASE("dsps_addc_f32 functionality", "[dsps]")
{
    int n = 64;
    float x[n];
    float y[n];
    for (int i = 0 ; i < n ; i++) {
        x[i] = i;
        y[i] = i + 10;
    }
    dsps_addc_f32(x, x, n, 10, 1, 1);
    for (int i = 0 ; i < n ; i++) {
        if (x[i] != y[i]) {
            TEST_ASSERT_EQUAL(x[i], y[i]);
        }
    }

    int repeat_count = 1;

    dsps_addc_f32(x, x, n, 10, 1, 1);

    unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_addc_f32(x, x, n, 10, 1, 1);
    unsigned int end_b = dsp_get_cpu_cycle_count();

    float total_b = end_b - start_b;
    float cycles = total_b / (n * repeat_count);
    ESP_LOGI(TAG, "dsps_addc_f32 - %f cycles per sample \n", cycles);
}
