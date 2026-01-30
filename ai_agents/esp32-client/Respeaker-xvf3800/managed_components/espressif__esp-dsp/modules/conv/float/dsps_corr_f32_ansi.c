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

#include "dsps_corr.h"

esp_err_t dsps_corr_f32_ansi(const float *Signal, const int siglen, const float *Pattern, const int patlen, float *dest)
{
    if (NULL == Signal) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (NULL == Pattern) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (NULL == dest) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (siglen < patlen) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

    for (size_t n = 0; n <= (siglen - patlen); n++) {
        float k_corr = 0;
        for (size_t m = 0; m < patlen; m++) {
            k_corr += Signal[n + m] * Pattern[m];
        }
        dest[n] = k_corr;
    }
    return ESP_OK;
}
