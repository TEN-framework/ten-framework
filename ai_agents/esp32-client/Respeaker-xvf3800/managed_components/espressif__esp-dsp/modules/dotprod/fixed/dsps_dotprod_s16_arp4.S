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
#if (dsps_dotprod_s16_arp4_enabled == 1)
#include "dsp_err_codes.h"

    .text
    .align  4
    .global dsps_dotprod_s16_arp4
    .global dsps_dotprod_s16_ansi
    .type   dsps_dotprod_s16_arp4,@function

//esp_err_t dsps_dotprod_s16_arp4(const int16_t* src1, const int16_t* src2, int16_t* dest, int len, int8_t shift);
dsps_dotprod_s16_arp4: 
// src1 - a0
// src2 - a1
// dest - a2
// len  - a3
// shift - a4
    andi a5, a3, 7
    beqz    a5, .dsps_dotprod_s16_arp4_body
    j   dsps_dotprod_s16_ansi

.dsps_dotprod_s16_arp4_body:
    add sp,sp,-16

    // Enable analigned data access
    esp.movx.r.cfg t6
    or t6, t6, 2
    esp.movx.w.cfg t6

    add t6, a4, -15
    neg t6, t6      // t6 - real_shift

    li      t3, 0x7fff
    srl     t3, t3, a4
    esp.zero.xacc
    esp.movx.w.xacc.l t3

    mv      t3, a0
    mv      t4, a1

    esp.vld.128.ip  q0, t3, 16  //q0 - src1
    srli t5, a3, 3              // t5 = len>>3
#     esp.lp.setup    0, t5, .main_loop
#         esp.vld.128.ip  q1, t4, 16        // q1 - src1
# .main_loop:       esp.vmulas.s16.xacc.ld.ip     q0, t3, 16, q0, q1 // q0 - src2

    .main_loop:        
        esp.vld.128.ip  q1, t4, 16      // q1 - src1
        esp.vmulas.s16.xacc.ld.ip     q0, t3, 16, q0, q1 // q0 - src2
        add  t5, t5, -1
    bgtz t5, .main_loop

    esp.srs.s.xacc       t5, t6 // shift accx register by final_shift amount (a6), save the lower 32bits to a15
    sh  t5, 0(a2)               // store result to output buffer 

    li  a0,0
    add sp,sp,16
    ret

#endif // dsps_dotprod_s16_ae32_enabled