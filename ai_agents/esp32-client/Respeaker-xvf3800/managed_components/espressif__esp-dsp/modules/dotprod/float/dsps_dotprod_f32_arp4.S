// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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

#include "dsps_dotprod_platform.h"
#if (dsps_dotprod_f32_arp4_enabled == 1)

    .text
    .align  4
    .global dsps_dotprod_f32_arp4
    .type   dsps_dotprod_f32_arp4,@function
// The function implements the following C code:
//esp_err_t dsps_dotprod_f32(const float* src1, const float* src2, float* dest, int len)
//{
//    float acc = 0;
//    for (int i=0 ; i< len ; i++)
//    {
//        acc += src1[i]*src2[i];
//    }
//    *dest = acc;
//    return ESP_OK;
//}

dsps_dotprod_f32_arp4:
// src1 - a0
// src2 - a1
// dest - a2
// len  - a3
    add sp,sp,-16

    fmv.w.x fa2,zero


    flw     fa0, 0(a0)
    flw     fa1, 0(a1)
    add     a0, a0, 4
    add     a1, a1, 4
    li      a4, 2
    ble     a3, a4, .loop_less_2 

// Loop when len > 2
    esp.lp.setup    0, a3, .dotprod_loop
        fmadd.s fa2, fa0, fa1, fa2
        flw     fa0, 0(a0)
        flw     fa1, 0(a1)
        add     a0, a0, 4
.dotprod_loop:  add     a1, a1, 4
    fsw     fa2, 0(a2)

    add sp,sp,16
    li  a0,0
    ret
// Loop when len <=2
.loop_less_2:
    fmadd.s fa2, fa0, fa1, fa2
    flw     fa0, 0(a0)
    flw     fa1, 0(a1)
    add     a0, a0, 4
    add     a1, a1, 4
    add     a3, a3, -1
    bnez    a3, .loop_less_2
    fsw     fa2, 0(a2)
    add sp,sp,16
    li  a0,0
    ret

#endif // dotprode_f32_arp4_enabled
