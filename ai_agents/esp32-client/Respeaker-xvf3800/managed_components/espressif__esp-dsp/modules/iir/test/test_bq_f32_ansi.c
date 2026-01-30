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

#include "dsps_tone_gen.h"
#include "dsps_d_gen.h"
#include "dsps_biquad_gen.h"
#include "dsps_biquad.h"

static const char *TAG = "dsps_biquad_f32_ansi";

float x[1024];
float y[1024];

TEST_CASE("dsps_biquad_f32_ansi functionality", "[dsps]")
{
    // In the test we generate filter with cutt off frequency 0.1
    // and then filtering 0.1 and 0.3 frequencis.
    // Result must be better then 24 dB
    int len = sizeof(x) / sizeof(float);

    dsps_tone_gen_f32(x, len, 1, 0.1, 0);

    float coeffs[5];
    float w1[2] = {0};
    float w2[2] = {0};
    dsps_biquad_gen_lpf_f32(coeffs, 0.1, 1);
    dsps_biquad_f32_ansi(x, y, len, coeffs, w1);
    float pow_band = 0;
    for (int i = len / 2 ; i < len ; i++) {
        pow_band += y[i] * y[i];
    }
    float pow_out_band = 0;
    dsps_tone_gen_f32(x, len, 1, 0.3, 0);
    dsps_biquad_f32_ansi(x, y, len, coeffs, w2);
    for (int i = len / 2 ; i < len ; i++) {
        pow_out_band += y[i] * y[i];
    }
    pow_band = 2 * pow_band / (float)len;
    pow_out_band = 2 * pow_out_band / (float)len;
    float diff_db = -10 * log10f(0.000000001 + pow_out_band / pow_band);
    ESP_LOGI(TAG, "Power: pass =%f, stop= %f, diff = %f dB", pow_band, pow_out_band, diff_db);

    if (diff_db < 24) {
        ESP_LOGE(TAG, "Attenuation for LPF must be not less then 24! Now it is: %f", diff_db);
        TEST_ASSERT_MESSAGE (false, "LPF attenuation is less then expected");
    }
}
