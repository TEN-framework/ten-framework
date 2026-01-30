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

#include "dsps_tone_gen.h"
#include "dsps_d_gen.h"
#include "dsps_biquad_gen.h"
#include "dsps_biquad.h"
#include <malloc.h>

static const char *TAG = "dsps_biquad_sf32_ansi";

TEST_CASE("dsps_biquad_sf32_ansi functionality", "[dsps]")
{

    float *x = (float *)memalign(16, sizeof(float) * 1024 * 2);
    float *y = (float *)memalign(16, sizeof(float) * 1024 * 2);

    // In the test we generate filter with cutt off frequency 0.1
    // and then filtering 0.1 and 0.3 frequencis.
    // Result must be better then 24 dB
    int len = 1024;

    dsps_tone_gen_f32(x, len * 2, 1, 0.1, 0);
    for (int i = len / 2 ; i < len ; i++) {
        y[i * 2 + 0] *= 10;
        y[i * 2 + 1] *= 0.1;
    }

    float coeffs[5];
    float w1[4] = {0};
    float w2[4] = {0};
    dsps_biquad_gen_lpf_f32(coeffs, 0.1, 1);
    dsps_biquad_sf32_ansi(x, y, len, coeffs, w1);
    float pow_band0 = 0;
    float pow_band1 = 0;
    for (int i = len / 2 ; i < len ; i++) {
        pow_band0 += y[i * 2 + 0] * y[i * 2 + 0];
        pow_band1 += y[i * 2 + 1] * y[i * 2 + 1];
    }
    float pow_out_band0 = 0;
    float pow_out_band1 = 0;
    dsps_tone_gen_f32(x, len * 2, 1, 0.3, 0);
    for (int i = len / 2 ; i < len ; i++) {
        y[i * 2 + 0] *= 10;
        y[i * 2 + 1] *= 0.1;
    }
    dsps_biquad_sf32_ansi(x, y, len, coeffs, w2);
    for (int i = len / 2 ; i < len ; i++) {
        pow_out_band0 += y[i * 2 + 0] * y[i * 2 + 0];
        pow_out_band1 += y[i * 2 + 1] * y[i * 2 + 1];
    }
    pow_band0 = 2 * pow_band0 / (float)len;
    pow_band1 = 2 * pow_band1 / (float)len;
    pow_out_band0 = 2 * pow_out_band0 / (float)len;
    pow_out_band1 = 2 * pow_out_band1 / (float)len;
    float diff0_db = -10 * log10f(0.000000001 + pow_out_band0 / pow_band0);
    float diff1_db = -10 * log10f(0.000000001 + pow_out_band1 / pow_band1);
    ESP_LOGI(TAG, "Power0: pass =%f, stop= %f, diff = %f dB", pow_band0, pow_out_band0, diff0_db);
    ESP_LOGI(TAG, "Power1: pass =%f, stop= %f, diff = %f dB", pow_band1, pow_out_band1, diff1_db);

    if (diff0_db < 24) {
        ESP_LOGE(TAG, "Attenuation for LPF must be not less then 24! Now it is: %f", diff0_db);
        TEST_ASSERT_MESSAGE (false, "LPF attenuation is less then expected");
    }
    if (diff1_db < 24) {
        ESP_LOGE(TAG, "Attenuation for HPF must be not less then 24! Now it is: %f", diff1_db);
        TEST_ASSERT_MESSAGE (false, "HPF attenuation is less then expected");
    }


    free(x);
    free(y);
}
