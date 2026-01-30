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

#include "dspm_mult_platform.h"
#if (dspm_mult_f32_arp4_enabled == 1)

// This is matrix multipliction function for ESP32 processor.
    .text
    .align  4
    .global dspm_mult_ex_f32_arp4
    .global .dspm_mult_ex_f32_arp4_body
    .type   dspm_mult_ex_f32_arp4,@function
// The function implements the following C code:
// esp_err_t dspm_mult_f32_ansi(const float *A, const float *B, float *C, int m, int n, int k, int A_padd, int B_padd, int C_padd)
// {
    // for (int i=0 ; i< m ; i++)
    // {
    //     for (int j=0 ; j< k ; j++)
    //     {
    //         C[i*k + j] = A[i*n]*B[j];
    //         for (int s=1; s< n ; s++)
    //         {
    //             C[i*k + j] += A[i*n + s]*B[s*k + j];
    //         }
    //     }
    // }
//     return ESP_OK;
// }

dspm_mult_ex_f32_arp4: 
// A - a2: a0
// B - a3: a1
// C - a4: a2
// m - a5: a3
// n - a6: a4
// k - a7: a5

// a8:a6  = n*4
// a10:t0 = 4
// a9:a7  - counter loop1: 0..m
// a11:t1 - counter loop2: 0..k
// a12:t2 - A
// a13:t3 - B
// a14:t4
// a15:t5

    add sp,sp,-16
    // Array increment for floating point data should be 4
.dspm_mult_ex_f32_arp4_body:

    mv      t5, a7

    add     t4, a6, a4    // A_step = A_padding + A_cols (n)
    add     t5, t5, a5    // B_step = B_padding + B_cols (k)
    slli    t5, t5, 2     // Pointer increment for B (B_step * 4)
    slli    t4, t4, 2     // A_step << 2
    lw      a6,  16(sp)   // C_padding from stack
    slli    a6, a6, 2     // C_step << 2
    
    li  a7, 0  // counter loop1

.dpf_loop1:    
    li  t1, 0 // reset counter for loop2
.dpf_loop2:

        // Clear initial state of the result register
        // a2 - A
        // a3 - B
        // a6 - n
        // a10 - step == 4 bytes
        // a8 -  step n*4
        mv      t2, a0 // load A

        slli     t3, t1, 2 // loop count to pointer value
        add      t3, a1, t3 // load A

        fmv.w.x fa2,zero // reset fa2
        // Calculating dotproduct...
        esp.lp.setup    0, a4, .matrix_mul_loop
            flw     fa0, 0(t2)
            add     t2, t2, 4
            flw     fa1, 0(t3)
            fmadd.s   fa2, fa1, fa0, fa2
        .matrix_mul_loop: add       t3, t3, t5

        fsw     fa2, 0(a2)
        addi    a2, a2, 4 // increment a2 for next time
        // check loop 2
        addi  t1, t1, 1 // Increment loop2 counter
        blt   t1, a5, .dpf_loop2

    // check loop 1
    add   a0, a0, t4      // A += (A_step << 2)
    add   a2, a2, a6      // output += (C_padding << 2)

    add   a7, a7, 1 // Increment loop1 counter
    blt   a7, a3, .dpf_loop1

    // Exit
    li  a0, 0       // return status ESP_OK
    add sp,sp,16
    ret

#endif //dspm_mult_ex_f32_arp4_enabled