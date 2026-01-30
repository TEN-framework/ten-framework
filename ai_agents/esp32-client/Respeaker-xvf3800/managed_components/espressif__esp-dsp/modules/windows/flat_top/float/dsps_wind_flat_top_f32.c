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

#define _USE_MATH_DEFINES
#include "dsps_wind_flat_top.h"
#include <math.h>

void dsps_wind_flat_top_f32(float *window, int len)
{
    const float a0 = 0.21557895;
    const float a1 = 0.41663158;
    const float a2 = 0.277263158;
    const float a3 = 0.083578947;
    const float a4 = 0.006947368;

    float len_mult = 1 / (float)(len - 1);
    for (int i = 0; i < len; i++) {
        window[i] = a0
                    - a1 * cosf(i * 2 * M_PI * len_mult)
                    + a2 * cosf(i * 4 * M_PI * len_mult)
                    - a3 * cosf(i * 6 * M_PI * len_mult)
                    + a4 * cosf(i * 8 * M_PI * len_mult);
    }
}
