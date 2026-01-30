/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dspm_add_platform.h"
#if (dspm_add_f32_ae32_enabled == 1)

// This is an add function for sub-matrices for ESP32 processor
    .text
    .align  4
    .global dspm_add_f32_ae32
    .type   dspm_add_f32_ae32,@function
// The function implements the following C code:
// esp_err_t dspm_add_f32_ansi(const float *input1, const float *input2, float *output, int rows, int cols, int padd1, int padd2, int padd_out, int step1, int step2, int step_out);

dspm_add_f32_ae32: 
// input1           - a2
// input2           - a3
// output           - a4
// rows             - a5
// cols             - a6
// padd1            - a7
// padd2            - a8
// padd_out         - a9
// step1            - a10
// step2            - a11
// step_out         - a12

    entry   a1, 16

    l32i.n  a8,  a1, 16         // padd2
    l32i.n  a9,  a1, 20         // padd_out
    l32i.n  a10, a1, 24         // step1
    l32i.n  a11, a1, 24         // step2
    l32i.n  a12, a1, 24         // step_out

    slli    a10, a10, 2         // a10  - step1 << 2
    slli    a11, a11, 2         // a11  - step2 << 2
    slli    a12, a12, 2         // a12  - step_out << 2

    .outer_loop_add_f32_ae32:

        loopnez a6, .loop_add_f32_ae32
            lsxp     f0,  a2,  a10      // load input1 to f0, increment input1 (input1_ptr+=step1)
            lsxp     f1,  a3,  a11      // load input2 to f1, increment input2 (input2_ptr+=step2)

            add.s    f2,  f0,  f1       // f2 = f0 + f1
            ssxp     f2,  a4,  a12      // save result f2 to output a4, increment output (output_ptr+=step_out)
        .loop_add_f32_ae32:

        addx4    a3,  a8,  a3           // input2_ptr += (padd2 << 2);
        addx4    a2,  a7,  a2           // input1_ptr += (padd1 << 2);
        addx4    a4,  a9,  a4           // output_ptr += (padd_out << 2);
        addi.n   a5,  a5,  -1           // rows - 1

    bnez a5, .outer_loop_add_f32_ae32

    movi.n  a2, 0 // return status ESP_OK
    retw.n

#endif // dspm_add_f32_ae32_enabled