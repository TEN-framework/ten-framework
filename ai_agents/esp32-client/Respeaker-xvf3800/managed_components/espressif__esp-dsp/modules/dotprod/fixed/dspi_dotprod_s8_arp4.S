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

#include "dspi_dotprod_platform.h"
#if (dspi_dotprod_arp4_enabled == 1)
#include "dsp_err_codes.h"

    .text
    .align  4
    .global dspi_dotprod_s8_arp4
    .global dspi_dotprod_s8_ansi
    .type   dspi_dotprod_s8_arp4,@function

// esp_err_t dspi_dotprod_s8_arp4(image2d_t *in_image, image2d_t *filter, int16_t *out_value, int count_x, int count_y, int shift);
dspi_dotprod_s8_arp4: 
// in_image     - a0
// filter       - a1
// out_value    - a2
// count_x      - a3
// count_y      - a4
// shift        - a5

// i_data       - t0
// f_data       - t1
// i_step       - t2
// f_step       - t3
// t4           - current i_data
// t5           - current f_data

    lw t1, 4(a0) // load  in_image->step_x
    lw t2, 4(a1) // load  filter->step_x
    or t1, t1, t2
    addi t1, t1, -1 // should be 0 now
    andi t2, a3, 15
    or   t1, t1, t2
    
    beqz    t1, .dspi_dotprod_s8_arp4_body
    j   dspi_dotprod_s8_ansi

.dspi_dotprod_s8_arp4_body:
    add sp, sp, -16
    lw  t0, 0(a0)   // i_data
    lw  t1, 0(a1)   // f_data

    lw  t2, 8(a0)   // step_y
    lw  t4, 12(a0)  // stride_x
    mul t2, t4, t2

    lw  t3, 8(a1)       // step_y
    lw  t5, 12(a1)      // stride_x
    mul t3, t5, t3

    srli t6, a3, 4      // t5 = len/16
    
    addi    a6, a5, -1
    li      t4, 1
    sll     t4, t4, a6
    esp.zero.xacc
    esp.movx.w.xacc.l   t4

.loop_count_y:
        mv      t4, t0
        mv      t5, t1
        esp.vld.128.ip      q0, t4, 16          // q0 - i_data

        esp.lp.setup    0, t6, .loop_count_x
            esp.vld.128.ip  q1, t5, 16          // q1 - f_data
.loop_count_x:  esp.vmulas.s8.xacc.ld.ip     q0, t4, 16, q0, q1 // q0 - i_data

        add     t0, t0, t2
        add     t1, t1, t3
        add     a4,a4, -1
    bgtz a4, .loop_count_y

    esp.srs.s.xacc       t5, a5 // shift accx register by final_shift amount (a5), save the lower 32bits to t5
    sh  t5, 0(a2)               // store result to output buffer 

    li  a0,0
    add sp,sp,16
    ret

#endif // dspi_dotprod_arp4_enabled