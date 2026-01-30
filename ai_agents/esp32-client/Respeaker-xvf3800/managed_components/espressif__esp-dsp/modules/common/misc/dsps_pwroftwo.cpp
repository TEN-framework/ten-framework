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

#include "dsp_common.h"

bool dsp_is_power_of_two(int x)
{
    return (x != 0) && ((x & (x - 1)) == 0);
}

int dsp_power_of_two(int x)
{
    for (size_t i = 0; i < 32; i++) {
        x = x >> 1;
        if (0 == x) {
            return i;
        }
    }
    return 0;
}
