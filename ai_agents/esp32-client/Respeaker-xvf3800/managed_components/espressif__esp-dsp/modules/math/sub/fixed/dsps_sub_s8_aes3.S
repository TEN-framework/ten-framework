/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsps_sub_platform.h"
#if (dsps_sub_s16_aes3_enabled == 1)

    .text
    .align  4
    .global dsps_sub_s8_aes3
    .type   dsps_sub_s8_aes3,@function
// The function implements the following C code:
// esp_err_t dsps_sub_s8_ansi(const int16_t *input1, const int16_t *input2, int16_t *output, int len, int step1, int step2, int step_out, int shift)
// {
//     for (int i = 0 ; i < len ; i++) {
//         int32_t acc = (int32_t)input1[i * step1] + (int32_t)input2[i * step2];
//         output[i * step_out] = acc >> shift;
//     }
//     return ESP_OK;
// }
dsps_sub_s8_aes3: 
// input1   - a2
// input2   - a3
// output   - a4
// len      - a5
// step_in1 - a6
// step_in2 - a7
// step_out - stack (a10)
// shift    - stack (a9)

    entry	a1, 16

    l32i.n	a10, a1, 16     // Load step_out to the a10 register
    l32i.n	a9, a1,  20     // Load shift to the a9 register
    ssr     a9              // sar = a9

    // Check if any of steps is not 0
    addi    a15, a6, -1
    bnez    a15, .sub_s8_ae32_mode // Branch if step !=0
    addi    a15, a7, -1
    bnez    a15, .sub_s8_ae32_mode // Branch if step !=0
    addi    a15, a10, -1
    bnez    a15,.sub_s8_ae32_mode  // Branch if step !=0

    // Check addresses
    movi        a15, 0xF                                              // modulo 16 mask
    bany        a2, a15, .sub_s8_ae32_mode                          // jump if != 0
    bany        a3, a15, .sub_s8_ae32_mode                          // jump if != 0

    // Check length (should be divided to 8)
    movi        a15, 0xf                                              // modulo 8 mask
    bany        a5, a15, .sub_s8_ae32_mode                          // jump if != 0

    // Process main function for S3
    
	wsr.sar	a9          // load sar register

    // Preload q1 from a3
    //ee.vld.128.ip     q1, a3, 16
    srli    a5, a5, 4
    ee.vld.128.ip     q0, a2, 16
    loopnez a5, .loop_end_sub_s8_aes3_main
        ee.vld.128.ip     q1, a3, 16
        ee.vsubs.s8.ld.incp q0, a2, q4, q0, q1
        ee.vst.128.ip     q4, a4, 16
.loop_end_sub_s8_aes3_main:

    // Exit for Esp32s3 mode
    movi.n	a2, 0 // return status ESP_OK
    retw.n


.sub_s8_ae32_mode:
    l8ui    a11, a2, 0
    l8ui    a8,  a3, 0
    sub     a8, a11, a8
    srl     a9, a8          // a8 = a8>>sar    

    loopnez a5, .loop_end_sub_s8_aes3
        add.n   a2, a2, a6      // input1+=step_in1;
        add.n   a3, a3, a7      // input2+=step_in2;

        l8ui   a11, a2, 0
        l8ui   a8,  a3, 0
        s8i	a9,  a4, 0      // store result to the putput
        sub     a8, a11, a8
    	srl     a9, a8          // a8 = a8>>sar    

        add.n   a4, a4, a10     // output+=step_out;
.loop_end_sub_s8_aes3:
    // Exit for Esp32 mode
    movi.n	a2, 0 // return status ESP_OK
    retw.n

#endif // dsps_sub_s8_aes3_enabled