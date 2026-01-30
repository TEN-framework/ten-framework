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
#include <math.h>
#include "unity.h"
#include "dsp_platform.h"
#include "esp_log.h"

#include "dsps_ccorr.h"
#include "esp_attr.h"
#include "esp_dsp.h"

static const char *TAG = "dsps_ccorr";

#define lenA  20
#define lenB  20

static float inputA[lenA];
static float inputB[lenB];
static float output_fwd[lenA + lenB - 1 + 2];
static float output_back[lenA + lenB - 1 + 2];

TEST_CASE("dsps_ccorr_f32_ansi functionality", "[dsps]")
{
    for (size_t la = 1; la < lenA; la++) {
        for (size_t lb = 1; lb < lenB; lb++) {
            for (int i = 0 ; i < lenA ; i++) {
                inputA[i] = (float)rand() / (float)INT32_MAX;
            }
            for (int i = 0 ; i < lenB ; i++) {
                inputB[i] = (float)rand() / (float)INT32_MAX;
            }
            for (int i = 0 ; i < (lenA + lenB  - 1 + 2); i++) {
                output_fwd[i] = -1;
                output_back[i] = -1;
            }
            dsps_ccorr_f32_ansi(inputA, la, inputB, lb, &output_fwd[1]);
            dsps_ccorr_f32_ansi(inputB, lb, inputA, la, &output_back[1]);
            TEST_ASSERT_EQUAL(output_fwd[0], -1);
            TEST_ASSERT_EQUAL(output_fwd[la + lb], -1);
            TEST_ASSERT_EQUAL(output_back[0], -1);
            TEST_ASSERT_EQUAL(output_back[la + lb], -1);
        }
    }
}

TEST_CASE("dsps_ccorr_f32_ansi draw", "[dsps]")
{
    int max_N = 1024;
    float *x = (float *)malloc(max_N * sizeof(float));
    TEST_ASSERT_NOT_NULL(x);
    float *y = (float *)malloc(max_N * sizeof(float));
    TEST_ASSERT_NOT_NULL(y);
    float *z = (float *)malloc((max_N * 2 + 1) * sizeof(float));
    TEST_ASSERT_NOT_NULL(z);
    int l1 = 8;
    int l2 = 4;
    for (int i = 0 ; i < max_N ; i++) {
        x[i] = 0;
        y[i] = 0;
        z[i] = 0;
    }
    x[0] = 20;
    x[7] = 30;
    y[0] = 10;
    y[3] = 8;
    dsps_ccorr_f32_ansi(x, l1, y, l2, &z[0]);

    dsps_view(z, l1 + l2, l1 + l2, 10,  -1, 400, '+');
    for (int i = 0 ; i < (l1 + l2 - 1) ; i++) {
        ESP_LOGI(TAG, "Z[%i] = %2.2f", i, z[i]);
    }

    free(x);
    free(y);
    free(z);
}

TEST_CASE("dsps_ccorr_f32_ansi benchmark", "[dsps]")
{
    int max_N = 1024;
    int conv_size = 64;
    float *x = (float *)malloc(max_N * sizeof(float));
    TEST_ASSERT_NOT_NULL(x);
    float *y = (float *)malloc(max_N * sizeof(float));
    TEST_ASSERT_NOT_NULL(y);
    float *z = (float *)malloc((max_N * 2 + 1) * sizeof(float));
    TEST_ASSERT_NOT_NULL(z);

    for (int i = 0 ; i < max_N ; i++) {
        x[i] = 0;
        y[i] = 1000;
    }

    unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_ccorr_f32_ansi(x, max_N, y, conv_size, &z[0]);
    unsigned int end_b = dsp_get_cpu_cycle_count();

    float cycles = end_b - start_b;
    ESP_LOGI(TAG, "dsps_conv_f32_ansi - %f cycles for signal %i and pattern %i", cycles, max_N, conv_size);
    free(x);
    free(y);
    free(z);
}
