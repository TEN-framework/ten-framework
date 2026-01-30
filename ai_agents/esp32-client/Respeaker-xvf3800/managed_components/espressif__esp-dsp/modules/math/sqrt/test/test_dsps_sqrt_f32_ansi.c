// Copyright 2018-2023 Espressif Systems (Shanghai) PTE LTD
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
#include "dsps_sqrt.h"
#include "esp_attr.h"

static const char *TAG = "dsps_sqrt";

TEST_CASE("dsps_sqrtf_f32_ansi functionality", "[dsps]")
{
    float max_err = -1000;
    float max_value = 0;
    float min_err = 0;
    float min_value = (float)INT32_MAX;
    int test_points = 100000;
    for (size_t i = 0; i < test_points; i++) {
        float test_value = rand();
        // if (test_value > max_value) max_value = test_value;
        // if (test_value < min_value) min_value = test_value;
        float x = sqrtf(test_value);
        float y = dsps_sqrtf_f32(test_value);
        float error = 20 * log10f(fabs((x - y) / x) + 0.000001);
        if (error > max_err) {
            max_err = error;
            max_value = test_value;
        }
        if (error < min_err) {
            min_err = error;
            min_value = test_value;
        }
        if (error > -25) {
            ESP_LOGE(TAG, "dsps_sqrtf_f32_ansi: error = %f dB,  value = %f (0x%8.8x)\n", error, test_value, (int)test_value);
            TEST_ASSERT_EQUAL(x, y);
        }
    }
    ESP_LOGI(TAG, "dsps_sqrtf_f32_ansi: max error = %f dB,   min error = %f dB, max_value = %f (0x%8.8x), min_value = %f (0x%8.8x)\n", max_err, min_err, max_value, (int)max_value, min_value, (int)min_value);
}

TEST_CASE("dsps_sqrt_f32_ansi functionality", "[dsps]")
{
    int n = 256;
    float *x = (float *)malloc(sizeof(float) * n);
    float *result = (float *)malloc(sizeof(float) * n);
    float *y = (float *)malloc(sizeof(float) * n);
    for (int i = 0 ; i < n ; i++) {
        y[i] = i * 10;
        x[i] = y[i] * y[i];
    }
    unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_sqrt_f32_ansi(x, result, n);
    float cycles = dsp_get_cpu_cycle_count() - start_b;

    for (int i = 0 ; i < n ; i++) {
        //printf("Result[%i] = %f, expected = %f,  diff = %f\n", i, result[i], y[i], 20*logf(fabs((result[i] - y[i])/y[i]) + 0.000001));
        float error = 20 * log10f(fabs((result[i] - y[i]) / y[i]) + 0.000001);
        if (error > -25) {
            TEST_ASSERT_EQUAL(result[i], y[i]);
        }
    }
    ESP_LOGI(TAG, "dsps_sqrt_f32_ansi - %f cycles for %i samples \n", cycles, n);
    free(x);
    free(y);
    free(result);
}
