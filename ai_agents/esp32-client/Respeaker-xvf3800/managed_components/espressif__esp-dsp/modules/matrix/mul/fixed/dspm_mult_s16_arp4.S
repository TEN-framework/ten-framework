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

#include "dspm_mult_platform.h"
#if (dspm_mult_s16_arp4_enabled == 1)

// This is matrix multipliction function for Risc-V processor core.
    .text
    .align  4
    .global dspm_mult_s16_arp4
    .global dspm_mult_s16_ansi  
    .global .dspm_mult_s16_arp4_body
    .type   dspm_mult_s16_arp4,@function
// The function implements the following C code:
// esp_err_t dspm_mult_f32_ansi(const int16_t *A, const int16_t *B, int16_t *C, int m, int n, int k, int shift)
// {
//    int final_shift = shift - 15;
//    for (int i = 0 ; i < m ; i++) {
//        for (int j = 0 ; j < k ; j++) {
//            // This code also could be used
//            //dsps_dotprode_f32_ae32(&A[i*n],&B[j],&C[i*k + j],n,1,n);
//            long long acc = 0x7fff >> shift;
//            for (int s = 0; s < n ; s++) {
//                acc += (int32_t)A[i * n + s] * (int32_t)B[s * k + j];
//            }
//            if (final_shift > 0) {
//                C[i * k + j] = (acc << final_shift);
//            } else {
//                C[i * k + j] = (acc >> (-final_shift));
//            }
//        }
//    }
//     return ESP_OK;
// }

dspm_mult_s16_arp4: 
// A - a0
// B - a1
// C - a2
// m - a3
// n - a4
// k - a5
// shift - a6

// a7 - counter loop1: 0..m
// t1 - counter loop2: 0..k
// t0 - counter loop3: 0..n
// x25(s9) - matrix step for input2
// x24(s8) - pointer to current B
// x29(t4) - pointer to initial B
// x30(t5) - pointer to A
// x31(t6) = 2 for increment....
// x26(s10)- final_shift

    or      t0, a3, a4
    or      t0, t0, a5
    andi    t0, t0, 0x7
    beqz    t0, .dspm_mult_s16_arp4_body
    j   dspm_mult_s16_ansi
    //ret

.dspm_mult_s16_arp4_body:
    add sp,sp,-16
    sw  s8, 4(sp)
    sw  s9, 8(sp)
    sw  s10, 12(sp)
    mv      t0, a4
    li      a7, 0  // counter loop1
    slli    x25, a5, 1 // step = step*2
    li      x31, 2
    // final_shift = shift - 15
    add     x26, a6, -15

.dpf_loop1: // loop for m
    li      t1, 0 // reset counter for loop2
    mv      x29, a1
.dpf_loop2: // loop for k
        mv  x30, a0
        mv  x24, x29        // load B
        // Calculating dotproduct...
        esp.zero.qacc                       // qacc = 0;
        esp.vldbc.16.xp     q0, x30, x31    // q0 = a[mx..mx]
        esp.vld.128.xp      q1, x24, x25    // q1 = b[x0..x7],
        esp.lp.setup    0, t0, .matrix_mul_loop
            esp.vmulas.s16.qacc.ldbc.incp   q0,x30,     q0,q1
        .matrix_mul_loop:   esp.vld.128.xp  q1,x24,x25
            
        esp.srcmb.s16.qacc  q2, x26, 0          //   q2 = qacc >> shift
        esp.vst.128.ip      q2, a2, 16          //  save k0..k7
        add     x29,x29, 16

        // check loop 2
        addi  t1, t1, 8 // Increment loop2 counter
        blt   t1, a5, .dpf_loop2
    add   x30, x30, -2
    mv    a0, x30   // 

    // check loop 1
    add   a7, a7, 1 // Increment loop1 counter
    blt   a7, a3, .dpf_loop1

    // Exit
    mv  a0, a6      // return status ESP_OK
    lw  s10, 12(sp)
    lw  s9, 8(sp)
    lw  s8, 4(sp)
    add sp,sp,16
    ret

#endif //dspm_mult_s16_arp4_enabled