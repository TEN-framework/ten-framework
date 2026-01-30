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

#include "dsps_fir_platform.h"
#if (dsps_fird_s16_arp4_enabled == 1)

// This is FIR filter for esp32p4 processor.
    .text
    .align  4
    .global dsps_fird_s16_arp4
    .global dsps_fird_s16_ansi
    .type   dsps_fird_s16_arp4,@function
// The function implements the following C code:
// int32_t dsps_fird_s16_arp4(fir_s16_t *fir, const int16_t *input, int16_t *output, int32_t len);

dsps_fird_s16_arp4:
    // In case of filter length different then 8*K
    lh  t2, 8(a0)   // t2 - coeffs_len
    andi t2, t2, 7
    beqz    t2, .dsps_fird_s16_arp4_body
    j   dsps_fird_s16_ansi

.dsps_fird_s16_arp4_body:
    add sp,sp,-48
    sw  s0, 0(sp)
    sw  s1, 4(sp)
    sw  s2, 8(sp)
    sw  s3, 12(sp)
    sw  s4, 16(sp)
    sw  s5, 20(sp)
    sw  s6, 24(sp)
    sw  s7, 28(sp)
    sw  s8, 32(sp)
    sw  s9, 36(sp)
    sw  s10, 40(sp)
    sw  s11, 44(sp)

    // Enable analigned data access
    esp.movx.r.cfg t6
    or t6, t6, 2
    esp.movx.w.cfg t6

    lw  t1, 4(a0)       // t1 - delay_line
    lh  t2, 8(a0)       // t2 - coeffs_len
    lh  t3, 10(a0)      // t3 - pos
    lh  t6, 16(a0)      // t6 - shift
    add t6, t6, -15
    neg t6, t6
    lw  t5, 20(a0)      // t5 - rounding_buff
    lw  s2, 4(a0)       // s2 - delay_line* current position
    add s2, s2, t3      // s2 = delay_line + pos*2
    add s2, s2, t3      //
    add s4, t2, t2      // s4 = coeff_len*2
    add s0, t1, s4      // s0 - &delay[coeffs_len]

    lh  a4,  0(t1)
.loop_len:
         lh  t4, 12(a0)          // t4 - decim
        .loop_decim_copy:
            lh   s1, 0(a1)      // load input data
            add  a1, a1, 2

            sh   s1, 0(s2)            
            add  s2, s2, 2     // preincrement of delay line
            bgt  s0, s2, .skeep_reset
                lw  s2, 4(a0)       // s2 - delay_line
            .skeep_reset:
            add  t4, t4, -1
        bgtz t4, .loop_decim_copy

        // s5 - count1 = length - pos
        // s6 = count1 >> 3 :  
        sub  t3, s2, t1
        srli t3, t3, 1          // t3 = (pos*2)>>1
        sub  s5, t2, t3
        srli s6, s5, 3          // s6 = (coeff_len - pos)>>3

        srli s7, t3, 3          // s7 = pos>>3
        and  s8, t3, 0x07       // s8 = pos&0x07

        esp.ld.xacc.ip           t5, 0                                          // load rounding value to accx

        lw  s10, 0(a0)          // s10 - coeffs
        esp.vld.128.ip          q0, s10, 16 //q0 - coeffs
        mv      s9, s2          // s9 - pointer to delay line
        esp.vld.128.ip          q1, s9, 16  // q1 - delay line data

        beqz s6, .skip_main_loop1
        esp.lp.setup    0, s6, .main_loop1
            esp.vmulas.s16.xacc.ld.ip     q0, s10, 16, q0, q1   // q0 - coeffs, q1 - data
        .main_loop1:     esp.vld.128.ip  q1, s9, 16              // Load delay line    
.skip_main_loop1: nop


        add     s9, s9, -16
        sub     s9, s9, s4
        beqz s8, .skip_rest_add
            esp.vld.128.ip          q2, s9, 16
            esp.vadd.s16            q1, q2, q1
            esp.vmulas.s16.xacc.ld.ip     q0, s10, 16, q0, q1   // q0 - coeffs, q1 - data
        .skip_rest_add: 
        esp.vld.128.ip          q1, s9, 16
        
        beqz s7, .skip_main_loop3
        esp.lp.setup    1, s7, .main_loop3
            esp.vmulas.s16.xacc.ld.ip     q0, s10, 16, q0, q1 // q0 - coeffs, q1 - data
            esp.vld.128.ip  q1, s9, 16
        .main_loop3: nop
.skip_main_loop3: nop

        // Shift and Store result
        esp.srs.s.xacc       s11, t6   // shift accx register by final_shift amount (a6), save the lower 32bits to a15
        sh  s11, 0(a2)      // store result to output buffer 
        add a2, a2, 2

        add  a3, a3, -1
    bgtz a3, .loop_len
    sh   t3, 10(a0)

.fast_exit:
    mv  a0, a6

    lw  s0, 0(sp)
    lw  s1, 4(sp)
    lw  s2, 8(sp)
    lw  s3, 12(sp)
    lw  s4, 16(sp)
    lw  s5, 20(sp)
    lw  s6, 24(sp)
    lw  s7, 28(sp)
    lw  s8, 32(sp)
    lw  s9, 36(sp)
    lw  s10, 40(sp)
    lw  s11, 44(sp)

    add sp,sp,48
    ret

#endif // 