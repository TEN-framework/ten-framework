/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsps_cplx_gen_platform.h"
#if (dsps_cplx_gen_aes3_enbled || dsps_cplx_gen_ae32_enbled)

// This is a Complex signal generator for ESP32 processor.
    .text
    .align  4
    .global dsps_cplx_gen_ae32
    .type   dsps_cplx_gen_ae32,@function
// The function implements the following C code:
// esp_err_t dsps_cplx_gen_ae32(cplx_sig_t *cplx_gen, void *output, int32_t len);

dsps_cplx_gen_ae32:

// Input params                 Variables float             Variables fixed
//
// cplx_gen - a2                fr              - f0        lut             - a5
// output   - a3                one_const       - f1        lut_len         - a6
// len      - a4                lut_len_f       - f2        sin_pos         - a7
//                              ph_f            - f3        cos_pos         - a8
//                              sin_pos_f       - f4        sin_to_cos      - a9
//                                                          ph_floor        - a10
//                                                          modulo          - a11

    entry    a1,    32
    l32i     a5,    a2,    0                    // a5 - lut
    l32i     a6,    a2,    4                    // a6 - lut_len
    lsi      f0,    a2,    8                    // f0 - fr
    lsi      f3,    a2,    12                   // f3 - ph_f (phase increment)
    const.s  f1,     1                          // f1 - constant 1
    float.s  f2,    a6,    0                    // f2 - lut_len_f
    srli     a9,    a6,    2                    // a9 - sin_to_cos = lut_len / 4
    addi     a11,   a6,   -1                    // a11 - modulo = lut_len - 1

    l32i     a15,   a2,    16                   // a15 - d_type
    beqz     a15, _s16_fixed

    // F32 floating point
    loopnez a4, ._main_loop_float

        floor.s     a10,   f3,   0              // turncate wiht rounding towards -infinity

        // branch if ph_floor is greater than 0
        bgez    a10, _ph_check_low_float
            add.s       f3,    f3,   f1         // f3 = f3 - f1 (ph_f + 1)
            floor.s     a10,   f3,    0         // turncate wiht rounding towards -infinity
        _ph_check_low_float:

        // branch if ph_ceil is lower than 2 (floored to 1)
        blti    a10, 1, _ph_check_great_float
            sub.s   f3,    f3,   f1             // f3 = f3 - f1 (ph_f - 1)
        _ph_check_great_float:

        mul.s   f4,   f3,  f2                   // sin_pos_f = ph_f * lut_len
        trunc.s a7,   f4,  0                    // truncate sin_pos_f to sin_pos

        add     a8,   a7,  a9                   // cos_pos (a8) = sin_pos(a7) + sin_to_cos(a9)
        and     a8,   a8,  a11                  // cos_pos = cos_pos & modulo (lut_len - 1)

        slli    a8,   a8,  2                    // set index of the LUT (4 x cos_pos)
        slli    a7,   a7,  2                    // set index of the LUT (4 x sin_pos)

        lsx     f14,  a5,  a7                   // load sin LUT value form *lut
        lsx     f15,  a5,  a8                   // load cos LUT value form *lut

        ssi     f15,  a3,  0                    // save cos LUT value to the output, offset 0
        ssi     f14,  a3,  4                    // save sin LUT value to the output, offset 4
        add.s   f3,   f3,  f0                   // ph_f += fr

        addi.n  a3,   a3,  8                    // increase the output pointer (2 x f32)
    ._main_loop_float:

    movi.n a2, 0
    retw.n

    // Q15 fixed point
    _s16_fixed:
    loopnez a4, ._main_loop_fixed

        floor.s     a10,   f3,   0              // turncate wiht rounding towards -infinity

        // branch if ph_floor is greater than 0
        bgez    a10, _ph_check_low_fixed
            add.s       f3,    f3,   f1         // f3 = f3 - f1 (ph_f + 1)
            floor.s     a10,   f3,    0         // turncate wiht rounding towards -infinity
        _ph_check_low_fixed:

        // branch if ph_ceil is lower than 2 (floored to 1)
        blti    a10, 1, _ph_check_great_fixed
            sub.s   f3,    f3,   f1             // f3 = f3 - f1 (ph_f - 1)
        _ph_check_great_fixed:

        mul.s   f4,   f3,  f2                   // sin_pos_f = ph_f * lut_len
        trunc.s a7,   f4,  0                    // truncate sin_pos_f to sin_pos

        add     a8,   a7,  a9                   // cos_pos (a8) = sin_pos(a7) + sin_to_cos(a9)
        and     a8,   a8,  a11                  // cos_pos = cos_pos & modulo (lut_len - 1)

        addx2   a15,  a8,  a5                   // get cos index of the LUT (*lut + 2 x cos_pos)
        addx2   a13,  a7,  a5                   // get sin index of the LUT (*lut + 2 x sin_pos)

        l16si   a14,  a15, 0                    // load cos LUT value from *lut
        l16si   a12,  a13, 0                    // load sin LUT value from *lut

        s16i    a14,  a3,  0                    // save cos LUT value to the output (a3), offset 0
        s16i    a12,  a3,  2                    // save sin LUT value to the output (a3), offset 2
        add.s   f3,   f3,  f0                   // ph_f += fr

        addi.n  a3,   a3,  4                    // increase the output pointer (2 x s16)
    ._main_loop_fixed:

    movi.n a2, 0
    retw.n

#endif // (dsps_cplx_gen_aes3_enbled || dsps_cplx_gen_ae32_enbled)