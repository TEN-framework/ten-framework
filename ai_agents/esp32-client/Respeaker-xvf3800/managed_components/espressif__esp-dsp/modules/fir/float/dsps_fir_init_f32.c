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
#include "malloc.h"


esp_err_t dsps_fir_init_f32(fir_f32_t *fir, float *coeffs, float *delay, int coeffs_len)
{
    // Allocate delay line in case if it's NULL
    if (delay == NULL) {
#ifdef CONFIG_IDF_TARGET_ESP32S3
        delay = (float *)memalign(16, (coeffs_len + 4) * sizeof(float));
#else
        delay = (float *)malloc((coeffs_len + 4) * sizeof(float));
#endif // CONFIG_IDF_TARGET_ESP32S3
        fir->use_delay = 1;
    } else {
        fir->use_delay = 0;
    }
    for (int i = 0; i < (coeffs_len + 4); i++) {
        delay[i] = 0;
    }
    fir->coeffs = coeffs;
    fir->delay = delay;
    fir->N = coeffs_len;
    fir->pos = 0;

#ifdef CONFIG_IDF_TARGET_ESP32S3
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

    for (int i = 0 ; i < coeffs_len; i++) {
        fir->delay[i] = 0;
    }
    return ESP_OK;
}

esp_err_t dsps_fir_f32_free(fir_f32_t *fir)
{
    if (fir->use_delay != 0) {
        fir->use_delay = 0;
        free(fir->delay);
    }
    return ESP_OK;
}
