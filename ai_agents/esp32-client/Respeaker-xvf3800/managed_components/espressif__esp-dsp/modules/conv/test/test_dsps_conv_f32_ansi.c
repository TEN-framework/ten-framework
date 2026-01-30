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
#include <malloc.h>
#include "unity.h"
#include "dsp_platform.h"
#include "esp_log.h"

#include "dsp_tests.h"
#include "dsps_conv.h"
#include "esp_attr.h"
#include "esp_dsp.h"

static const char *TAG = "dsps_conv";

#define lenA  20
#define lenB  20

esp_err_t dsps_conv_f32_ref(const float *Signal, const int siglen, const float *Kernel, const int kernlen, float *convout)
{
    if (NULL == Signal) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (NULL == Kernel) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (NULL == convout) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

    for (int n = 0; n < siglen + kernlen - 1; n++) {
        size_t kmin, kmax, k;

        convout[n] = 0;

        kmin = (n >= kernlen - 1) ? n - (kernlen - 1) : 0;
        kmax = (n < siglen - 1) ? n : siglen - 1;

        for (k = kmin; k <= kmax; k++) {
            convout[n] += Signal[k] * Kernel[n - k];
        }
    }
    return ESP_OK;
}

TEST_CASE("dsps_conv_f32_ansi functionality", "[dsps]")
{
    float *inputA = (float *)memalign(16, lenA * sizeof(float));
    float *inputB = (float *)memalign(16, lenB * sizeof(float));

    float *output_ref = (float *)memalign(16, (lenA + lenB - 1 + 2) * sizeof(float));
    float *output_fwd = (float *)memalign(16, (lenA + lenB - 1 + 2) * sizeof(float));
    float *output_back = (float *)memalign(16, (lenA + lenB - 1 + 2) * sizeof(float));

    for (int la = 1; la < lenA; la++) {
        for (int lb = 1; lb < lenB; lb++) {
            for (int i = 0 ; i < lenA ; i++) {
                inputA[i] = (float)rand() / (float)INT32_MAX;
            }
            for (int i = 0 ; i < lenB ; i++) {
                inputB[i] = (float)rand() / (float)INT32_MAX;
            }
            for (int i = 0 ; i < (lenA + lenB  - 1 + 2); i++) {
                output_ref[i] = -1;
                output_fwd[i] = -1;
                output_back[i] = -1;
            }
            dsps_conv_f32_ref(inputA, la, inputB, lb, &output_ref[1]);
            dsps_conv_f32_ansi(inputA, la, inputB, lb, &output_fwd[1]);
            dsps_conv_f32_ansi(inputB, lb, inputA, la, &output_back[1]);
            float max_eps = 0.000001;
            for (int i = 0; i < (la + lb + 1); i++) {
                if ((fabs(output_ref[i] - output_fwd[i]) > max_eps) || (fabs(output_ref[i] - output_back[i]) > max_eps) || (fabs(output_back[i] - output_fwd[i]) > max_eps)) {
                    ESP_LOGI(TAG, "la=%i, lb=%i, i=%i, ref=%2.3f, fwd=%2.3f, back=%2.3f", la, lb, i, output_ref[i], output_fwd[i], output_back[i]);
                }
                TEST_ASSERT_EQUAL(output_ref[i], output_fwd[i]);
                TEST_ASSERT_EQUAL(output_ref[i], output_back[i]);
                TEST_ASSERT_EQUAL(output_back[i], output_fwd[i]);
            }
        }
    }
    free(inputA);
    free(inputB);
    free(output_ref);
    free(output_fwd);
    free(output_back);
}

TEST_CASE("dsps_conv_f32_ansi draw", "[dsps]")
{
    int max_N = 1024;
    float *x = (float *)malloc(max_N * sizeof(float));
    TEST_ASSERT_NOT_NULL(x);
    float *y = (float *)malloc(max_N * sizeof(float));
    TEST_ASSERT_NOT_NULL(y);
    float *z = (float *)malloc((max_N * 2 + 1) * sizeof(float));
    TEST_ASSERT_NOT_NULL(z);

    for (int i = 0 ; i < max_N ; i++) {
        x[i] = 10;
        y[i] = 20;
        z[i] = 0;
    }

    dsps_conv_f32_ansi(x, 32, y, 16, &z[0]);

    dsps_view(z, 32 + 16, 32 + 16, 10,  -1, 4000, '+');

    free(x);
    free(y);
    free(z);
}

TEST_CASE("dsps_conv_f32_ansi benchmark", "[dsps]")
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
    dsps_conv_f32_ansi(x, max_N, y, conv_size, &z[0]);
    unsigned int end_b = dsp_get_cpu_cycle_count();

    float cycles = end_b - start_b;
    ESP_LOGI(TAG, "dsps_conv_f32_ansi - %f cycles for signal %i and pattern %i", cycles, max_N, conv_size);
    free(x);
    free(y);
    free(z);
}
