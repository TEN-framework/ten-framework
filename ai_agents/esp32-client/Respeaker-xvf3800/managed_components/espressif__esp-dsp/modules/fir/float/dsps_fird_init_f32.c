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

#include "dsps_fir.h"


esp_err_t dsps_fird_init_f32(fir_f32_t *fir, float *coeffs, float *delay, int N, int decim)
{
    fir->coeffs = coeffs;
    fir->delay = delay;
    fir->N = N;
    fir->pos = 0;
    fir->decim = decim;

#ifdef CONFIG_IDF_TARGET_ESP32S3
    // The amount of coefficients should be divided to 4,
    // if not, add zero coefficients to round length to 0
    if (fir->N % 4 != 0) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }
    // The coeffs array should be aligned to 16
    if (((uint32_t)coeffs) & 0x0f) {
        return ESP_ERR_DSP_ARRAY_NOT_ALIGNED;
    }
    // The delay array should be aligned to 16
    if (((uint32_t)delay) & 0x0f) {
        return ESP_ERR_DSP_ARRAY_NOT_ALIGNED;
    }
#endif // CONFIG_IDF_TARGET_ESP32S3

    for (int i = 0 ; i < N; i++) {
        fir->delay[i] = 0;
    }
    return ESP_OK;
}
