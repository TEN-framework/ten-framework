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


#include "dsps_biquad.h"


esp_err_t dsps_biquad_sf32_ansi(const float *input, float *output, int len, float *coef, float *w)
{
    for (int i = 0 ; i < len ; i++) {
        float d0 = input[i * 2 + 0] - coef[3] * w[0] - coef[4] * w[1];
        output[i * 2 + 0] = coef[0] * d0 +  coef[1] * w[0] + coef[2] * w[1];
        w[1] = w[0];
        w[0] = d0;

        d0 = input[i * 2 + 1] - coef[3] * w[2] - coef[4] * w[3];
        output[i * 2 + 1] = coef[0] * d0 +  coef[1] * w[2] + coef[2] * w[3];
        w[3] = w[2];
        w[2] = d0;
    }
    return ESP_OK;
}
