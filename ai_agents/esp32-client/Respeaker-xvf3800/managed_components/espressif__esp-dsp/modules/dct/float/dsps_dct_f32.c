// Copyright 2018-2020 Espressif Systems (Shanghai) PTE LTD
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

#include "dsp_common.h"
#include <math.h>

#include "dsps_dct.h"
#include "dsps_fft2r.h"

esp_err_t dsps_dct_f32_ref(float *data, int N, float *result)
{
    float factor = M_PI / N;
    for (size_t i = 0; i < N; i++) {
        float sum = 0;
        for (size_t j = 0; j < N; j++) {
            sum += data[j] * cosf((j + 0.5) * i * factor);
        }
        result[i] = sum;
    }
    return ESP_OK;
}

esp_err_t dsps_dct_inverce_f32_ref(float *data, int N, float *result)
{
    float factor = M_PI / N;
    for (size_t i = 0; i < N; i++) {
        float sum = data[0] / 2;
        for (size_t j = 0; j < N; j++) {
            sum += data[j] * cosf(j * (i + 0.5) * factor);
        }
        result[i] = sum;
    }
    return ESP_OK;
}

esp_err_t dsps_dct_f32(float *data, int N)
{
    esp_err_t ret = ESP_OK;
    if (dsps_fft2r_initialized == 0) {
        return ESP_ERR_DSP_REINITIALIZED;
    }

    for (int i = 0; i < N / 2; i++) {
        data[(N - 1 - i) * 2] = data[i * 2 + 1];
        data[i * 2 + 1] = 0;
        data[N + i * 2 + 1] = 0;
    }

    ret = dsps_fft2r_fc32(data, N);
    if (ret != ESP_OK) {
        return ret;
    }

    // // The follows code do the same as this one:
    // //
    // float factor = M_PI / (N * 2);
    // ret = dsps_bit_rev_fc32(data, N);
    // for (int i = 0; i < N; i++) {
    //  float temp = i * factor;
    //  data[i] = data[i*2] * cosf(temp) + data[i*2 + 1] * sinf(temp);
    // }
    int table_step = 2;
    for (int i = 0; i < N; i++) {
        float c =  dsps_fft_w_table_fc32[i * 2 * table_step];
        float s =  dsps_fft_w_table_fc32[i * 2 * table_step + 1];
        data[i * 2] = data[i * 2] * c;
        data[i * 2 + 1] = data[i * 2 + 1] * s;
    }
    ret = dsps_bit_rev_fc32(data, N);
    if (ret != ESP_OK) {
        return ret;
    }

    for (int i = 0; i < N; i++) {
        data[i] = data[i * 2] + data[i * 2 + 1];
    }
    return ESP_OK;
}

esp_err_t dsps_dct_inv_f32(float *data, int N)
{
    esp_err_t ret = ESP_OK;
    if (dsps_fft2r_initialized == 0) {
        return ESP_ERR_DSP_REINITIALIZED;
    }

    float factor = M_PI / (N * 2);
    data[0] *= 0.5;
    for (int i = N - 1; i >= 0; i--) {
        float temp = i * factor;
        data[i * 2] = data[i] * cosf(temp);
        data[i * 2 + 1] = data[i] * -sinf(temp);
    }
    ret = dsps_fft2r_fc32(data, N);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = dsps_bit_rev_fc32(data, N);
    if (ret != ESP_OK) {
        return ret;
    }
    for (size_t i = 0; i < N / 2; i++) {
        data[i * 2 + 1] = data[(N - 1 - i) * 2];
    }

    return ESP_OK;
}
