// Copyright 2018-2023 Espressif Systems (Shanghai) PTE LTD
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
#if (dsps_fird_f32_aes3_enabled == 1)

// This is FIR filter for Esp32s3 processor.
    .text
    .align  4
    .global dsps_fird_f32_aes3
    .type   dsps_fird_f32_aes3,@function
// The function implements the following C code:
//esp_err_t dsps_fird_f32_aes3(fir_f32_t* fir, const float* input, float* output, int len);

dsps_fird_f32_aes3:
// fir      - a2
// input    - a3
// output   - a4
// len      - a5

// a2 - fir structure
// a3 - input
// a4 - output
// a5 - length

// a6 - fir length
// a7 - position in delay line
// a8 - temp
// a10 - coeffs ptr
// a11 - delay line ptr
// a12 - const
// a13 - 
// a14 - temp for loops
// a15 - delay line rounded to 16

    entry	a1, 16
    // Array increment for floating point data should be 4
    l32i    a7,  a2, 12 // a7  - pos

    l32i    a6,  a2, 8  // a6  - N - amount of coefficients
    l32i    a10, a2, 0  // a10 - coeffs
    l32i    a11, a2, 4  // a11 - delay line
    addx4	a11, a7, a11 // a11 = a11 + a7*4	
    l32i    a6,  a2, 8   // a6  - N

    mov.n    a9, a5
    movi.n	a12, 3

    movi.n	a12, -16
    movi.n	a13, 15
// Main loop for input samples
.fird_loop_len:
        // Store K values from input to delay line:

        l32i    a14,  a2, 16   // a14  - decimation
        loopnez  a14, .fird_load_data // K loops
            // Store to delay line
            lsip	f15,  a3, 4		// a3  += 4, f15 = input[n]
            ssip	f15, a11, 4		// a11 += 4, *a11 = f15
            addi    a7,  a7,  1     // a7++ - position in delay line

            blt     a7, a6, .do_not_reset_a11
                l32i    a11, a2, 4	// Load delay line
                movi    a7,  0
            .do_not_reset_a11:
            and     a15, a11, a12
        .fird_load_data:
        //
        // Process data
        //
        // Load rounded delay line address

        l32i    a10, a2, 0  // a10 - coeffs

        // Clear f4, f5 for multiplications
        const.s f4, 0
        const.s f5, 0
        const.s f6, 0
        const.s f7, 0

        and		a8, a11, a13		// a8 = a11 & 15
        beqz   	a8, .offset_0
        addi   	a8, a8, -4
        beqz   	a8, .offset_1
        addi   	a8, a8, -4
        beqz   	a8, .offset_2
        addi   	a8, a8, -4
        beqz   	a8, .offset_3

// a10 - coeffs
// a11 - delay line
.offset_0:
        sub   a14, a6, a7   // a14 = N-pos
        srli  a14, a14, 2
        loopnez  a14, .first_fir_loop_0 // pos...N-1
            EE.LDF.128.IP f3, f2, f1, f0, a10, 16 // Load coeffs
            EE.LDF.128.IP f11, f10, f9, f8, a15, 16 // Load data from delay line
            madd.s  f4, f0, f8
            madd.s  f5, f1, f9
            madd.s  f6, f2, f10
            madd.s  f7, f3, f11
        .first_fir_loop_0:
        
        l32i    a15, a2, 4  // a11 - delay line [0]	
        srli  a14, a7, 2
        loopnez  a14, .second_fir_loop_0 // 0..pos
            EE.LDF.128.IP f3, f2, f1, f0, a10, 16 // Load coeffs
            EE.LDF.128.IP f11, f10, f9, f8, a15, 16 // Load data from delay line
            madd.s  f4, f0, f8
            madd.s  f5, f1, f9
            madd.s  f6, f2, f10
            madd.s  f7, f3, f11
        .second_fir_loop_0:
        j    .store_fir_result;

.offset_1:
        sub   a14, a6, a7   // a14 = N-pos
        addi  a14, a14, 3
        srli  a14, a14, 2
        EE.LDF.128.IP f11, f10, f9, f12, a15, 16 // Load data from delay line
        // f12 - delay[N-1], store for the last operation
        // f9..f11 - delay[0..2]
        loopnez  a14, .first_fir_loop_1 // pos...N-1
            EE.LDF.128.IP f3, f2, f1, f0, a10, 16 // Load coeffs
            madd.s  f4, f0, f9
            madd.s  f5, f1, f10
            madd.s  f6, f2, f11
            EE.LDF.128.IP f11, f10, f9, f8, a15, 16 // Load data from delay line
            madd.s  f7, f3, f8
        .first_fir_loop_1:
        
        l32i    a15, a2, 4  // a11 - delay line [0]
        EE.LDF.128.IP f11, f10, f9, f8, a15, 16 // Load data from delay line		
        srli  a14, a7, 2
        loopnez  a14, .second_fir_loop_1 // 0..pos
            madd.s  f4, f3, f8
            EE.LDF.128.IP f3, f2, f1, f0, a10, 16 // Load coeffs
            madd.s  f5, f0, f9
            madd.s  f6, f1, f10
            madd.s  f7, f2, f11
            EE.LDF.128.IP f11, f10, f9, f8, a15, 16 // Load data from delay line
        .second_fir_loop_1:

        madd.s  f4, f3, f12
        j    .store_fir_result;

.offset_2:
        sub   a14, a6, a7   // a14 = N-pos
        addi  a14, a14, 3
        srli  a14, a14, 2
        EE.LDF.128.IP f11, f10, f13, f12, a15, 16 // Load data from delay line
        // f12, f13 - delay[N-1], delay[N-2], store for the last operation
        // f10..f11 - delay[0..1]
        loopnez  a14, .first_fir_loop_2 // pos...N-1
            EE.LDF.128.IP f3, f2, f1, f0, a10, 16 // Load coeffs
            madd.s  f4, f0, f10
            madd.s  f5, f1, f11
            EE.LDF.128.IP f11, f10, f9, f8, a15, 16 // Load data from delay line
            madd.s  f6, f2, f8
            madd.s  f7, f3, f9
        .first_fir_loop_2:
        
        l32i    a15, a2, 4  // a11 - delay line [0]
        EE.LDF.128.IP f11, f10, f9, f8, a15, 16 // Load data from delay line		
        srli  a14, a7, 2
        loopnez  a14, .second_fir_loop_2 // 0..pos
            madd.s  f4, f2, f8
            madd.s  f5, f3, f9
            EE.LDF.128.IP f3, f2, f1, f0, a10, 16 // Load coeffs
            madd.s  f6, f0, f10
            madd.s  f7, f1, f11
            EE.LDF.128.IP f11, f10, f9, f8, a15, 16 // Load data from delay line
        .second_fir_loop_2:

        madd.s  f4, f2, f12
        madd.s  f5, f3, f13
        j    .store_fir_result;

.offset_3:
        sub   a14, a6, a7   // a14 = N-pos
        addi  a14, a14, 3
        srli  a14, a14, 2
        EE.LDF.128.IP f11, f14, f13, f12, a15, 16 // Load data from delay line
        // f12, f13, f14 - delay[N-1], delay[N-2], delay[N-3], store for the last operation
        // f11 - delay[0]
        loopnez  a14, .first_fir_loop_3 // pos...N-1
            EE.LDF.128.IP f3, f2, f1, f0, a10, 16 // Load coeffs
            madd.s  f4, f0, f11
            EE.LDF.128.IP f11, f10, f9, f8, a15, 16 // Load data from delay line
            madd.s  f5, f1, f8
            madd.s  f6, f2, f9
            madd.s  f7, f3, f10
        .first_fir_loop_3:
        
        l32i    a15, a2, 4  // a11 - delay line [0]
        EE.LDF.128.IP f11, f10, f9, f8, a15, 16 // Load data from delay line		
        srli  a14, a7, 2
        loopnez  a14, .second_fir_loop_3 // 0..pos
            madd.s  f4, f1, f8
            madd.s  f5, f2, f9
            madd.s  f6, f3, f10
            EE.LDF.128.IP f3, f2, f1, f0, a10, 16 // Load coeffs
            madd.s  f7, f0, f11
            EE.LDF.128.IP f11, f10, f9, f8, a15, 16 // Load data from delay line
        .second_fir_loop_3:

        madd.s  f4, f1, f12
        madd.s  f5, f2, f13
        madd.s  f4, f3, f14

.store_fir_result:

    add.s   f4, f4, f5
    add.s   f6, f6, f7
    add.s   f4, f4, f6

    // Store result
    ssip     f4, a4, 4  // y++ - save result and increment output pointer
    // Check loop length
    addi   a5, a5, -1
    bnez    a5, .fird_loop_len
    // store state

    s32i    a7,  a2, 12 // pos = a7
    mov.n    a2,  a9
    retw.n

#endif // dsps_fir_f32_aes3_enabled