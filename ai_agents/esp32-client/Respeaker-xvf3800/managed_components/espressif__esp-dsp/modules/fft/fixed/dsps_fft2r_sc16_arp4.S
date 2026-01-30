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

#include "dsps_fft2r_platform.h"

#if (dsps_fft2r_sc16_arp4_enabled == 1)

// This is matrix multiplication function for esp32p4 processor.
    .text
    .align  4
    .global dsps_fft2r_sc16_arp4_
    .type   dsps_fft2r_sc16_arp4_,@function

dsps_fft2r_sc16_arp4_:
//esp_err_t dsps_fft2r_sc16_arp4_(int16_t *data, int N, int16_t *w);

    add sp,sp,-16
    sw  s8, 4(sp)
    sw  s9, 8(sp)
    sw  s10, 12(sp)

    mv  a5, a3
    li  a4, 16
    esp.movx.w.sar  a4

    li  a4, 0x7fff
    sw  a4, 0(sp)
    mv  x26, sp
    esp.vldbc.16.ip     q6, x26, 0
#
    srli t6, a1, 1 // t6 = N2 = N/2
    li   t0, 1     // t0 - ie
    li   t2, 2     // t2 = 2 : limit for the loop N2 > 2

.fft2r_l1: 
        li t1, 0    // t1 - j
        li t4, 0    // t4 = ia = 0;
        mv x26, a2  // x26 - pointer to w
.fft2r_l2:          // loop for j, a8 - j

            esp.vldbc.32.ip     q5, x26, 4
            add      t5, t4, t6   // t5 = m = ia + N2
            
            slli     a4, t5, 2    // a4 - pointer for m
            slli     a3, t4, 2    // a3 - pointer for ia
            add      a4, a4, a0   // a4 = &data[m*2]
            add      a3, a3, a0   // a3 = &data[ia*2]
            mv       x24, a3
            mv       x25, a4
            add      t4, t4, t6 // ia += N2 instead of ia++ for each cycle
            srli     a7, t6, 2  // a7 = t6>> 2
            beqz     a7, .fft2r_l3_skeep

            esp.lp.setup    0, a7, .fft2r_l3        // main butterfly loop
                esp.vld.128.ip      q0, x25, 0      // Load data[m  .. m + 3]
                esp.vld.128.ip      q2, x24, 0      // Load data[ia .. ia + 3] 
                esp.cmul.s16        q1, q5, q0, 2
                esp.vmul.s16        q2, q2, q6      // q0 = in_data_ia*0x7fff
                esp.cmul.s16        q1, q5, q0, 3
                esp.vsub.s16        q3, q2, q1     // input[2 * m] = input[2 * ia] - re_temp;

                esp.vadd.s16.st.incp   q3, x25, q4, q2, q1
.fft2r_l3:      esp.vst.128.ip         q4, x24, 16

.fft2r_l3_skeep:    
            add     t4, t4, t6          // ia += N2
            add     t1, t1, 1           // j++
        BNE  t1, t0, .fft2r_l2

        slli    t0, t0, 1   // ie = ie<<1
        srli    t6, t6, 1   // t6 = N2 = N2>>1
    bgt    t6, t2, .fft2r_l1// N2 > 2

    srli    t0, t0, 1       // ie = ie>>1
    mv      x26, a2         // x26 - pointer to w
    mv      x24, a0
    esp.lp.setup    0, t0, .fft2r_l2_1
        esp.vldbc.32.ip     q2, x26, 4
        esp.vldbc.32.ip     q7, x26, 4
        esp.vunzip.32       q2, q7

        esp.vld.l.64.ip     q0, x24, 8 
        esp.vld.l.64.ip     q1, x24, 8 
        esp.vld.h.64.ip     q0, x24, 8 
        esp.vld.h.64.ip     q1, x24, -24

        esp.vmul.s16        q0, q0, q6
        esp.cmul.s16        q3, q2, q1, 2 
        esp.cmul.s16        q3, q2, q1, 3 
        esp.vsub.s16        q4, q0, q3
        esp.vadd.s16        q5, q0, q3

        esp.vst.l.64.ip      q5, x24, 8
        esp.vst.l.64.ip      q4, x24, 8
        esp.vst.h.64.ip      q5, x24, 8
.fft2r_l2_1:        esp.vst.h.64.ip      q4, x24, 8

    mv      x26, a2  // x26 - pointer to w
    mv      x24, a0
    esp.lp.setup    0, t0, .fft2r_l2_0
        esp.vld.128.ip      q0, x24, 16  // q0 = ia
        esp.vld.128.ip      q1, x24,-16  // q1 = m
        esp.vld.128.ip      q2, x26, 16

        esp.vunzip.32       q0, q1

        esp.cmul.s16        q3, q2, q1, 2
        esp.vmul.s16        q0, q0, q6
        esp.cmul.s16        q3, q2, q1, 3

        esp.vsub.s16        q1, q0, q3
        esp.vadd.s16        q0, q0, q3

        esp.vzip.32         q0, q1
        
        esp.vst.128.ip      q0, x24, 16
.fft2r_l2_0: esp.vst.128.ip      q1, x24, 16

    lw  s8, 4(sp)
    lw  s9, 8(sp)
    lw  s10, 12(sp)
    add sp,sp,16
    li  a0,0
    ret

#endif // dsps_fft2r_sc16_arp4_enabled
