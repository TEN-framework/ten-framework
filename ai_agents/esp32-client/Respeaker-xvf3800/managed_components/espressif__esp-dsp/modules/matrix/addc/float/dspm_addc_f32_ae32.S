/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dspm_addc_platform.h"
#if (dspm_addc_f32_ae32_enabled == 1)

// This is an add function for sub-matrices for ESP32 processor
    .text
    .align  4
    .global dspm_addc_f32_ae32
    .type   dspm_addc_f32_ae32,@function
// The function implements the following C code:
// esp_err_t dspm_addc_f32_ansi(const float *input, float *output, float C, int rows, int cols, int padd_in, int padd_out, int step_in, int step_out);

dspm_addc_f32_ae32: 
// input            - a2
// output           - a3
// C                - a4
// rows             - a5
// cols             - a6
// padd_in          - a7
// padd_out         - a8
// step_in          - a9
// step_out         - a10

    entry   a1, 16

    l32i.n  a8,  a1, 16         // padd_out
    l32i.n  a9,  a1, 20         // step_in
    l32i.n  a10, a1, 24         // step_out

    slli    a9,  a9,  2         // a9   - step_in << 2
    slli    a10, a10, 2         // a10  - step_out << 2

    wfr     f0,  a4             // a4   - load to the f0

    .outer_loop_addc_f32_ae32:

        loopnez a6, .loop_addc_f32_ae32
            lsxp     f1,  a2,  a9       // load input to f1, increment input (input_ptr+=step_in)

            add.s    f2,  f0,  f1       // f2 = f0 + f1
            ssxp     f2,  a3,  a10      // save result f2 to output a3, increment output (output_ptr+=step_out)
        .loop_addc_f32_ae32:

        addx4    a2,  a7,  a2           // input1_ptr += (padd_in << 2);
        addx4    a3,  a8,  a3           // output_ptr += (padd_out << 2);
        addi.n   a5,  a5,  -1           // rows - 1

    bnez a5, .outer_loop_addc_f32_ae32

    movi.n  a2, 0 // return status ESP_OK
    retw.n

#endif // dspm_add_f32_ae32_enabled