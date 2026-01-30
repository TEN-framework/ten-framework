/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dspm_mult_platform.h"
#if (dspm_mult_f32_ae32_enabled == 1)

#include "dsps_dotprode_f32_m_ae32.S"

 // This is matrix multiplication function for ESP32 processor.
    .text
    .align  4
    .global  dspm_mult_ex_f32_ae32
    .global .dspm_mult_ex_f32_ae32_body
    .type    dspm_mult_ex_f32_ae32,@function
// The function implements the following C code:
//esp_err_t dspm_mult_ex_f32_ae32(const float *A, const float *B, float *C, int m, int n, int k, int A_padd, int B_padd, int C_padd);

dspm_mult_ex_f32_ae32: 

// A         - a2
// B         - a3
// C         - a4
// m         - a5
// n         - a6
// k         - a7
// A_padding - a14
// B_padding - a15
// C_padding - a8

// a10 = 4
// a9  - counter loop1: 0..m
// a11 - counter loop2: 0..k
// a12 - A
// a13 - B
// a4  - C

    entry   a1, 16
    // Array increment for floating point data should be 4
.dspm_mult_ex_f32_ae32_body:

    l32i.n  a14, a1, 16     // A_padding
    l32i.n  a15, a1, 20     // B_padding
    l32i.n  a8,  a1, 24     // C_padding

    add     a14, a14, a6    // A_step = A_padding + A_cols (n)
    add     a15, a15, a7    // B_step = B_padding + B_cols (k)
    slli    a15, a15, 2     // Pointer increment for B (B_step * 4)

    movi.n  a10, 4          // Increment = 4
    movi.n  a9, 0           // counter loop1
    const.s f3, 0           // Innitial state of accumulator, f3 = 0

.mult_ex_loop1:
    movi.n  a11, 0 // reset counter for loop2
    .mult_ex_loop2:
        // Clear initial state of the result register
        // a2 - A
        // a3 - B
        // a6 - n
        // a10 - step == 4 bytes
        
        mov     a12, a2             // load A
        addx4   a13, a11, a3        // loop count to pointer value
        mov.s   f1, f3              // reset f1

        // Calculating dotproduct...
        //dotprode_f32_ae32( x1   x2   count step1 step2)
        dotprode_f32_ae32    a12, a13, a6,   a10,  a15;

        addi.n  a11, a11, 1         // Increment loop2 counter
        ssip    f1,  a4,  4         // Store restul from f1 to memory at a4 and increment a4

        // check loop 2
        blt   a11, a7, .mult_ex_loop2

    // check loop 1
    addx4   a2, a14, a2      // A += (A_step << 2)
    addx4   a4, a8,  a4      // output += (C_padding << 2)
    addi.n  a9, a9, 1        // Increment loop1 counter
    blt     a9, a5, .mult_ex_loop1

    movi.n  a2, 0   // return status ESP_OK
    retw.n

#endif //dspm_mult_f32_ae32_enabled
