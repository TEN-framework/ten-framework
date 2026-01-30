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
#include "dsps_corr.h"
#include "esp_attr.h"

static const char *TAG = "dsps_corr";

#define lenA  15
#define lenB  10

static float inputA[lenA];
static float inputB[lenB];
static float output[lenA + lenB - 1 + 2];
static float output_ref[lenA + lenB - 1 + 2];

TEST_CASE("dsps_corr_f32_aexx functionality", "[dsps]")
{
    for (int i = 0 ; i < lenA ; i++) {
        inputA[i] = i;
    }
    for (int i = 0 ; i < lenB ; i++) {
        inputB[i] = 10 + i;
    }
    for (int i = 0 ; i < (lenA - lenB  + 2); i++) {
        output[i] = -1;
        output_ref[i] = -1;
    }
    inputB[0] = 1;
    dsps_corr_f32(inputA, lenA, inputB, lenB, &output[1]);
    dsps_corr_f32_ansi(inputA, lenA, inputB, lenB, &output_ref[1]);
    for (int i = 0; i < (lenA - lenB) + 2; i++) {
        ESP_LOGD(TAG, "Data[%i] = %2.2f, expected = %2.2f", i, output[i], output_ref[i]);
    }
    for (size_t i = 0; i < (lenA - lenB) + 2; i++) {
        TEST_ASSERT_EQUAL(output_ref[i], output[i]);
    }
}

TEST_CASE("dsps_corr_f32_aexx benchmark", "[dsps]")
{
    int max_N = 1024;
    int corr_size = 64;
    float *x = (float *)malloc(max_N * sizeof(float));
    TEST_ASSERT_NOT_NULL(x);
    float *y = (float *)malloc(max_N * sizeof(float));
    TEST_ASSERT_NOT_NULL(y);
    float *z = (float *)malloc(max_N * sizeof(float));
    TEST_ASSERT_NOT_NULL(z);

    for (int i = 0 ; i < max_N ; i++) {
        x[i] = 0;
        y[i] = 1000;
    }

    unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_corr_f32(x, max_N, y, corr_size, &z[0]);
    unsigned int end_b = dsp_get_cpu_cycle_count();

    float cycles = end_b - start_b;
    ESP_LOGI(TAG, "dsps_corr_f32_ae32 - %f cycles for signal %i and pattern %i", cycles, max_N, corr_size);
    free(x);
    free(y);
    free(z);

}
