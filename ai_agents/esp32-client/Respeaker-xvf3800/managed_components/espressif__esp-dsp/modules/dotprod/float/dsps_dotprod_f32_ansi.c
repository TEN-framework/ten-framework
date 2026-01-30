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

#include "dsps_dotprod.h"

esp_err_t dsps_dotprod_f32_ansi(const float *src1, const float *src2, float *dest, int len)
{
    float acc = 0;
    for (int i = 0 ; i < len ; i++) {
        acc += src1[i] * src2[i];
    }
    *dest = acc;
    return ESP_OK;
}
