/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dspm_mult_platform.h"

#if (dspm_mult_f32_aes3_enabled == 1)

// This is matrix multiplication function for ESP32S3 processor.
    .text
    .align  4
    .global  dspm_mult_ex_f32_aes3
    .global .dspm_mult_ex_f32_ae32_body
    .type    dspm_mult_ex_f32_aes3,@function
// The function implements the following C code:
//esp_err_t dspm_mult_ex_f32_ansi(const float* A, const float* B, float* C, int A_rows, int A_cols, int B_cols, int A_padding, int B_padding, int C_padding)
//{
//    const int A_step = A_cols + A_padding;
//    const int B_step = B_cols + B_padding;
//    const int C_step = B_cols + C_padding;
//
//    for (int i = 0; i < A_rows; i++) {
//        for (int j = 0; j < B_cols; j++) {
//            C[i * C_step + j] = A[i * A_step] * B[j];
//            for (int s = 1; s < A_cols; s++) {
//                C[i * C_step + j] += A[i * A_step + s] * B[s * B_step + j];
//            }
//        }
//    }
//    return ESP_OK;
//}

// A - a2
// B - a3
// C - a4
// m - a5
// n - a6
// k - a7
// A_padd = a8
// B_padd = a9
// C_padd = a15

dspm_mult_ex_f32_aes3:

    entry   a1, 16
    l32i.n  a8, a1, 16     // A_padding
    l32i.n  a9, a1, 20     // B_padding
    l32i.n  a15,  a1, 24   // C_padding

    // Check if we can use S3 memory model
    // Check matrices dimensions and paddings all of them must be divisible by 4
    or      a12, a5, a6         // a12 = m OR n
    or      a14, a8, a9         // a14 = A_padd OR B_padd
    or      a12, a12, a7        // a12 = m OR n OR k
    or      a14, a14, a15       // a14 = A_padd OR B_padd OR C_padd
    or      a12, a12, a14       // a12 = m OR n OR k OR A_padd OR B_padd OR C_padd
    movi.n  a11, 3              // a11 = byte mask
    and     a12, a12, a11       // a12 = a12 AND 3 (byte mask)

    // Check alignment of A B C matrices data pointers
    movi.n  a11, 15             // a11 = byte mask
    or      a10, a3,  a2        // a10 = A pointer OR B pointer
    or      a10, a10, a4        // a10 = A pointer OR B pointer OR C pointer
    and     a10, a10, a11       // a10 = a10 AND 15 (byte mask)
    or      a12, a12, a10       // a12 = mat_dim OR alignment
    beqz    a12, .s3_mmult_ex   // if zero, jump to s3_mult
    // Call Esp32 function
    J      .dspm_mult_ex_f32_ae32_body

.s3_mmult_ex:
// f0, f1, f2, f3 - multiplication result
// f4, f5, f6, f7 - input for matrix B
// f8, f9, f10,f11- input far matrix A
    movi.n      a14, 0          // B pointer increment for y loop

    add         a15, a15, a7    // a15 = k + C_padding
    slli        a10, a15, 2     // a10 = (K + C_padding) * 4 - step for rows

    mov         a15, a9         // a15 = B_padd
    slli        a15, a15, 2     // a15 = B_padd * 4

    add         a7, a7, a9      // a7 = k + B_padding
    slli        a12, a7, 2      // a12 = (K + B_padding) * 4 - step for rows
    srli        a11, a6, 2      // a11 = n / 4
    addi.n      a11, a11, -1    // a11 = innter loop count (n)

    slli        a6, a8, 2       // a6 = A_padding *4 = A_pointer step
    mov         a13, a3         // backup B pointer
    mov         a7, a4          // backup C pointer

.loop_x_mult_ex:
    movi.n      a9,  0          // reset loop1 counter
    mov         a8,  a2         // move A matrix back to the beginning
    .loop_y_mult_ex:

        add  a13, a3, a14       // Reload Y pointer to Y11 + A14
        EE.LDF.128.IP f11, f10, f9, f8, a8, 16  // Load A values: X11, X12, X13, X14
        EE.LDF.128.XP f7, f6, f5, f4, a13, a12 // Load B value: Y11, Y12, Y13, Y14
        mul.s   f0, f4, f8      // f0 = X11*Y11
        mul.s   f1, f5, f8      // f1 = X12*Y11
        mul.s   f2, f6, f8      // f2 = X13*Y11
        mul.s   f3, f7, f8      // f3 = X14*Y11

        EE.LDF.128.XP f7, f6, f5, f4, a13, a12 // Load B value: Y21, Y22, Y23, Y24
        madd.s  f0, f4, f9      // f0 = X11*Y11 + X12*Y21
        madd.s  f1, f5, f9      // f1 = X11*Y12 + X12*Y22
        madd.s  f2, f6, f9      // f2 = X11*Y13 + X12*Y23
        madd.s  f3, f7, f9      // f3 = X11*Y14 + X12*Y24

        EE.LDF.128.XP f7, f6, f5, f4, a13, a12 // Load B value: Y31, Y32, Y33, Y34
        madd.s  f0, f4, f10     // f0 = X11*Y11 + X12*Y21 + X13*Y31
        madd.s  f1, f5, f10     // f1 = X11*Y12 + X12*Y22 + X13*Y32
        madd.s  f2, f6, f10     // f2 = X11*Y13 + X12*Y23 + X13*Y33
        madd.s  f3, f7, f10     // f3 = X11*Y14 + X12*Y24 + X13*Y34

        EE.LDF.128.XP f7, f6, f5, f4, a13, a12 // Load B value: Y41, Y42, Y43, Y44
        madd.s  f0, f4, f11     // f0 = X11*Y11 + X12*Y21 + X13*Y31 + X14*Y41
        madd.s  f1, f5, f11     // f1 = X11*Y12 + X12*Y22 + X13*Y32 + X14*Y42
        madd.s  f2, f6, f11     // f2 = X11*Y13 + X12*Y23 + X13*Y33 + X14*Y43
        madd.s  f3, f7, f11     // f3 = X11*Y14 + X12*Y24 + X13*Y34 + X14*Y44

        loopnez a11, .iner_loop_mult_ex
            EE.LDF.128.IP f11, f10, f9, f8, a8, 16  // Load A values: X15, X16, X17, X18

            EE.LDF.128.XP f7, f6, f5, f4, a13, a12 // Load B value: Y51, Y52, Y53, Y54
            madd.s  f0, f4, f8      // f0 += X15*Y51
            madd.s  f1, f5, f8      // f1 += X15*Y52
            madd.s  f2, f6, f8      // f2 += X15*Y53
            madd.s  f3, f7, f8      // f3 += X15*Y54

            EE.LDF.128.XP f7, f6, f5, f4, a13, a12 // Load B value: Y61, Y62, Y63, Y64
            madd.s  f0, f4, f9      // f0 += X16*Y61
            madd.s  f1, f5, f9      // f1 += X16*Y62
            madd.s  f2, f6, f9      // f2 += X16*Y63
            madd.s  f3, f7, f9      // f3 += X16*Y64

            EE.LDF.128.XP f7, f6, f5, f4, a13, a12 // Load B value: Y71, Y72, Y73, Y74
            madd.s  f0, f4, f10     // f0 =
            madd.s  f1, f5, f10     // f1 =
            madd.s  f2, f6, f10     // f2 =
            madd.s  f3, f7, f10     // f3 =

            EE.LDF.128.XP f7, f6, f5, f4, a13, a12 // Load B value: Y81, Y82, Y83, Y84
            madd.s  f0, f4, f11     // f0 =
            madd.s  f1, f5, f11     // f1 =
            madd.s  f2, f6, f11     // f2 =
            madd.s  f3, f7, f11     // f3 =
        .iner_loop_mult_ex:
        EE.STF.128.XP f3, f2, f1, f0, a4, a10 // Store result 

        addi.n  a9,  a9, 1          // Increment loop1 counter
        add     a8,  a8, a6         // (increase A pointer by A_padding * 4 times)
    blt   a9, a5, .loop_y_mult_ex

    addi.n  a7,  a7,  16            // Increase C pinter by 16
    mov     a4,  a7
    addi.n  a14, a14, 16            // Increase B pointer by 16
    addi.n  a15, a15, 16            // Increment loop2 counter by 16

blt   a15, a12, .loop_x_mult_ex
    movi.n  a2, 0 // return status ESP_OK
    retw.n

#endif //dspm_mult_f32_aes3_enabled