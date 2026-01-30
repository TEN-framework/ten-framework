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
#include "esp_dsp.h"
#include "dsp_platform.h"
#include "esp_log.h"

#include "dsps_tone_gen.h"
#include "dsps_d_gen.h"
#include "dsps_fir.h"
#include "dsp_tests.h"

static const char *TAG = "dsps_fir_f32_ansi";

static float x[1024];
static float y[1024];

static float coeffs[32];
static float delay[32 + 4];

TEST_CASE("dsps_fir_f32_ansi functionality", "[dsps]")
{
    // In the test we generate filter with cutt off frequency 0.1
    // and then filtering 0.1 and 0.3 frequencis.
    // Result must be better then 24 dB
    int len = sizeof(x) / sizeof(float);
    int fir_len = sizeof(coeffs) / sizeof(float);

    fir_f32_t fir1;
    for (int i = 0 ; i < fir_len ; i++) {
        coeffs[i] = (fir_len - i - 1);
    }

    for (int i = 0 ; i < len ; i++) {
        x[i] = 0;
    }
    x[0] = 1;

    dsps_fir_init_f32(&fir1, coeffs, delay, fir_len);
    dsps_fir_f32_ansi(&fir1, x, y, len);

    for (int i = 0 ; i < fir_len * 3 ; i++) {
        ESP_LOGD(TAG, "fir[%i] = %f", i, y[i]);
    }
    for (int i = 0 ; i < fir_len ; i++) {
        if (y[i] != i) {
            TEST_ASSERT_EQUAL(y[i], i);
        }
    }

    // Check even length
#ifndef CONFIG_IDF_TARGET_ESP32S3
    fir_len--;
#endif
    for (int i = 0 ; i < fir_len ; i++) {
        coeffs[i] = (fir_len - i - 1);
    }

    for (int i = 0 ; i < len ; i++) {
        x[i] = 0;
    }
    x[0] = 1;
    dsps_fir_init_f32(&fir1, coeffs, delay, fir_len);
    dsps_fir_f32_ansi(&fir1, x, y, len);

    for (int i = 0 ; i < fir_len ; i++) {
        if (y[i] != i) {
            TEST_ASSERT_EQUAL(y[i], i);
        }
    }
    for (int i = fir_len ; i < len ; i++) {
        if (y[i] != 0) {
            TEST_ASSERT_EQUAL(y[i], 0);
        }
    }
}

TEST_CASE("dsps_fir_f32_ansi benchmark", "[dsps]")
{

    int len = sizeof(x) / sizeof(float);
    int fir_len = sizeof(coeffs) / sizeof(float);
    int repeat_count = 1;

    fir_f32_t fir1;
    for (int i = 0 ; i < fir_len ; i++) {
        coeffs[i] = i;
    }

    for (int i = 0 ; i < len ; i++) {
        x[i] = 0;
    }
    x[0] = 1;

    dsps_fir_init_f32(&fir1, coeffs, delay, fir_len);

    unsigned int start_b = dsp_get_cpu_cycle_count();
    for (int i = 0 ; i < repeat_count ; i++) {
        dsps_fir_f32_ansi(&fir1, x, y, len);
    }
    unsigned int end_b = dsp_get_cpu_cycle_count();

    float total_b = end_b - start_b;
    float cycles = total_b / (len * repeat_count);

    ESP_LOGI(TAG, "dsps_fir_f32_ansi - %f per sample for for %i coefficients, %f per tap \n", cycles, fir_len, cycles / (float)fir_len);

    float min_exec = 10;
    float max_exec = 800;
    TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);
}
