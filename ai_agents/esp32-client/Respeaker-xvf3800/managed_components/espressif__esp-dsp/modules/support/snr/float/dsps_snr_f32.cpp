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

#include "dsps_snr.h"
#include "dsps_fft2r.h"
#include "dsp_common.h"
#include <math.h>
#include <limits>
#include "esp_log.h"

static const char *TAG = "snr";

float dsps_snr_f32(const float *input, int32_t len, uint8_t use_dc)
{
    if (!dsp_is_power_of_two(len)) {
        return 0;
    }

    float *temp_array = new float[len * 2];
    for (int i = 0 ; i < len ; i++) {
        float wind = 0.5 * (1 - cosf(i * 2 * M_PI / (float)len));
        temp_array[i * 2 + 0] = input[i] * wind;
        temp_array[i * 2 + 1] = 0;
    }
    dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);

    dsps_fft2r_fc32_ansi(temp_array, len);
    dsps_bit_rev_fc32_ansi(temp_array, len);

    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::min();
    int max_pos = 0;
    for (int i = 0 ; i < len / 2 ; i++) {
        temp_array[i] = temp_array[i * 2 + 0] * temp_array[i * 2 + 0] + temp_array[i * 2 + 1] * temp_array[i * 2 + 1];
        if (temp_array[i] < min) {
            min = temp_array[i];
        }
        if (temp_array[i] > max) {
            max = temp_array[i];
            max_pos = i;
        }
        ESP_LOGD(TAG, "FFT Data[%i] =%8.4f dB", i, temp_array[i]);
    }
    int start_pos = 0;
    int wind_width = 7;

    if (use_dc == 0) {
        start_pos = wind_width;
    }
    float noise_power = 0;
    for (int i = start_pos ; i < len / 2 ; i++) {
        if ((i < (max_pos - wind_width)) || (i > (max_pos + wind_width))) {
            noise_power += temp_array[i];
            ESP_LOGD(TAG, "FFT Data[%i] =%8.4f dB, maX=%f, max_pos=%i, noise_power=%f", i, temp_array[i], max, max_pos, noise_power);
        }
    }

    delete[] temp_array;
    noise_power += std::numeric_limits<float>::min();
    if (noise_power < max * 0.00000000001) {
        return 192;
    }
    float snr = max / noise_power;
    float result = 10 * log10(max / noise_power) - 2; // 2 - window correction
    ESP_LOGI(TAG, "SNR = %f, result=%f dB", snr, result);
    return result;
}
