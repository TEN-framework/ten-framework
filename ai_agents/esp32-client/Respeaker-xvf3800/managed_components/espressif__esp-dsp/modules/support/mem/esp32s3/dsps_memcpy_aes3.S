/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsps_mem_platform.h"
#if dsps_mem_aes3_enbled

// This is memory access for ESP32S3 processor.
    .text
    .align  4
    .global dsps_memcpy_aes3
    .type   dsps_memcpy_aes3,@function
// The function implements the following C code:
// void *dsps_memcpy_aes3(void *arr_dest, const void *arr_src, size_t arr_len);

// Input params                 Variables
//
// arr_dest - a2                loop_len    - a5, a6
// arr_src  - a3                p_arr_des   - a7
// arr_len  - a4                div_48      - a8
//                              align_mask  - a9

/*
esp32s3 optimized memcpy function works with both, aligned and unaligned data.

arr_dest aligned -->     - _main_loop_aligned, 32 bytes in one run through the cycle, only aligned data
arr_src  aligned /       - Check modulos to finish copying the remaining data outside of the cycle
                         - Modulo 8 and 16 - S3 instructions for aligned data, the rest of the modulos are generic

arr_dest aligned --->    - _main_loop_unaligned, 48 bytes of source unaligned data in one run through the cycle,
arr_src unaligned /        (the destination must always be aligned)
                         - Check modulos to finish copying remaining data outside of the cycle
                         - Modulo 32 and 16 - S3 instructions for unaligned data, the rest of the modulos are generic

arr_dest unaligned ->    - First, use generic instructions to align the arr_dest data (keep increasing
arr_src aligned   /        the arr_dest pointer until the pointer is aligned)
                         - Once arr_dest is aligned treat the rest of the data as:
                             either both aligned (if arr_src happens to be aligned after the arr_dest aligning),
                             or as arr_dest aligned and arr_src unaligned
                         - Continue as mentioned above

arr_dest unaligned ->    - Very same approach as with arr_dest unaligned and arr_src aligned
arr_src unaligned /

if the arr_len is less than 16, jump to _less_than_16 label and copy data without any s3 instructions or cycles
*/
#define MEMCPY_OPTIMIZED    1           // Use optimized memcpy or ANSI memcpy
#define TIE_ENABLE          0           // Put a dummy TIE instruction to the ANSI memcpy to induce TIE context saving

dsps_memcpy_aes3:

#if MEMCPY_OPTIMIZED

    // S3 optimized version of the memcpy (with TIE instrucstions)

    entry    a1,    32
    mov      a7,    a2                              // a7 - save arr_dest pointer

    blti     a4,    16,  _less_than_16

    // arr_dest alignment check
    movi.n  a9,    0xf                              // 0xf alignment mask
    and     a13,   a9,  a2                          // 0xf AND arr_dest pointer
    beqz    a13,   _arr_dest_aligned

        movi.n  a14,   16                           // a14 - 16
        sub     a13,   a14,   a13                   // a13 = 16 - unalignment
        sub     a4,    a4,    a13                   // len = len - (16 - unalignment)

        // Aligning the arr_dest
        // keep copying until arr_dest is aligned

        // Check modulo 8 of the unalignment, if - then copy 8 bytes
        bbci a13,  3, _aligning_mod_8_check         // branch if 3-rd bit of unalignment a13 is clear
            l32i.n      a15,  a3,  0                // load 32 bits from arr_src a3 to a15, offset 0
            l32i.n      a14,  a3,  4                // load 32 bits from arr_src a3 to a14, offset 4
            s32i.n      a15,  a2,  0                // save 32 bits from a15 to arr_dest a2, offset 0
            s32i.n      a14,  a2,  4                // save 32 bits from a14 to arr_dest a2, offset 4
            addi.n      a3,   a3,  8                // increment arr_src pointer by 8 bytes
            addi.n      a2,   a2,  8                // increment arr_dest pointer by 8 bytes
        _aligning_mod_8_check:

        // Check modulo 4 of the unalignment, if - then copy 4 bytes
        bbci a13, 2, _aligning_mod_4_check          // branch if 2-nd bit of unalignment a13 is clear
            l32i.n      a15,  a3,  0                // load 32 bits from arr_src a3 to a15
            addi.n      a3,   a3,  4                // increment arr_src pointer by 4 bytes
            s32i.n      a15,  a2,  0                // save 32 bits from a15 to arr_dest a2
            addi.n      a2,   a2,  4                // increment arr_dest pointer by 4 bytes
        _aligning_mod_4_check:

        // Check modulo 2 of the unalignment, if - then copy 2 bytes
        bbci a13, 1, _aligning_mod_2_check          // branch if 1-st bit of unalignment a13 is clear
            l16ui       a15,  a3,  0                // load 16 bits from arr_src a3 to a15
            addi.n      a3,   a3,  2                // increment arr_src pointer by 2 bytes
            s16i        a15,  a2,  0                // save 16 bits from a15 to arr_dest a2
            addi.n      a2,   a2,  2                // increment arr_dest pointer by 2 bytes
        _aligning_mod_2_check:

        // Check modulo 1 of the unalignment, if - then copy 1 byte
        bbci a13, 0, _arr_dest_aligned              // branch if 0-th bit of unalignment a13 is clear
            l8ui        a15,  a3,  0                // load 8 bits from arr_src a3 to a15
            addi.n      a3,   a3,  1                // increment arr_src pointer by 1 byte
            s8i         a15,  a2,  0                // save 8 bits from a15 to arr_dest a2
            addi.n      a2,   a2,  1                // increment arr_dest pointer by 1 byte

    _arr_dest_aligned:

    // arr_src alignment check
    and     a15,   a9,  a3                          // 0xf (alignment mask) AND arr_src pointer
    beqz    a15,   _arr_src_aligned

        // arr_src unaligned, arr_dest aligned (arr_des either aligned originally or modified to be aligned by the Aligning the arr_des routine)

        // Calculate modulo for non-aligned data
        movi     a8,  89478486                      // a8 - div_48 constant
        muluh    a5,  a8,  a4                       // a5 - loop_len = arr_len / 48
        movi     a9,  48                            // a9 - 48
        mul16s   a8,  a9,  a5                       // a8 - 48 * loop_len
        sub      a6,  a4,  a8                       // a6 - loop_len_MOD 48

        ee.ld.128.usar.ip   q2,  a3,  16            // Preload from arr_src
        ee.ld.128.usar.ip   q3,  a3,  16            // Preload from arr_src

        // Main loop arr_src unaligned
        loopnez a5, ._main_loop_unaligned           // 48 bytes in one loop
            ee.src.q.ld.ip    q4,  a3,  16, q2, q3  // preload and shift from arr_src
            ee.vst.128.ip     q2,  a2,  16          // store to aligned arr_dest
            ee.src.q.ld.ip    q2,  a3,  16, q3, q4  // preload and shift from arr_src
            ee.vst.128.ip     q3,  a2,  16          // store to aligned arr_dest
            ee.src.q.ld.ip    q3,  a3,  16, q4, q2  // preload and shift from arr_src
            ee.vst.128.ip     q4,  a2,  16          // store to aligned arr_dest
        ._main_loop_unaligned:

        // Finish the _main_loop_unaligned outside of the loop from Q registers preloads
        // Check modulo 32 of the loop_len_MOD, if - then copy 32 bytes
        bbci   a6,  5,   _unaligned_mod_32_check    // branch if 5-th bit of loop_len_MOD a6 is clear
            ee.src.q.ld.ip    q4,  a3,  0,  q2, q3  // preload and shift from arr_src
            ee.vst.128.ip     q2,  a2,  16          // store to aligned arr_dest
            ee.src.q          q3,  q3,  q4          // final shift
            ee.vst.128.ip     q3,  a2,  16          // store to aligned arr_dest
            j _follow_unaligned
        _unaligned_mod_32_check:

        // Check modulo 16 of the loop_len_MOD, if - then copy 16 bytes
        bbci   a6, 4,   _unaligned_mod_16_check     // branch if 4-th bit of loop_len_MOD a6 is clear
            ee.src.q          q2,  q2,  q3          // final shift
            ee.vst.128.ip     q2,  a2,  16          // store to aligned arr_dest
            addi              a3,  a3, -16          // put arr_src pointer back
            j _follow_unaligned
        _unaligned_mod_16_check:

        addi    a3, a3, -32                         // put arr_src pointer back

        // Finish the _main_loop_unaligned outside of the loop
        // Check modulo 8 of the loop_len_MOD, if - then copy 8 bytes
        _follow_unaligned:
        bbci a6, 3, _unaligned_mod_8_check          // branch if 3-rd bit of loop_len_MOD a6 is clear
            l32i.n      a15,  a3,  0                // load 32 bits from arr_src a3 to a15, offset 0
            l32i.n      a14,  a3,  4                // load 32 bits from arr_src a3 to a14, offset 4
            s32i.n      a15,  a2,  0                // save 32 bits from a15 to arr_dest a2, offset 0
            s32i.n      a14,  a2,  4                // save 32 bits from a14 to arr_dest a2, offset 4
            addi.n      a3,   a3,  8                // increment arr_src pointer by 8 bytes
            addi.n      a2,   a2,  8                // increment arr_dest pointer by 8 bytes
        _unaligned_mod_8_check:

        // Finish the rest of the data, as if the data were aligned, no S3 instructions will be used further after the jump
        j _aligned_mod_8_check

    // Both arrays (arr_src and arr_dest) aligned
    _arr_src_aligned:

    // Calculate modulo 32 for aligned data
    srli    a5,    a4,   5                          // a5 - loop_len = arr_len / 32
    slli    a6,    a5,   5
    sub     a6,    a4,  a6                          // a6 - loop_len_MOD 32

    // Main loop arr_src aligned
    loopnez  a5, ._main_loop_aligned                // 32 bytes in one loop
        ee.vld.128.ip    q0,  a3,  16               // load 16 bytes from arr_src to q0
        ee.vld.128.ip    q1,  a3,  16               // load 16 bytes from arr_src to q1

        ee.vst.128.ip    q0,  a2,  16               // save 16 bytes to arr_dest from q0
        ee.vst.128.ip    q1,  a2,  16               // save 16 bytes to arr_dest from q1
    ._main_loop_aligned:

    // Modulo 32 check
    beqz    a6,    _aligned_mod_32_check            // branch if mod_32 = 0

        // finish the end of the array outside of the main loop
        // Check modulo 16 of the loop_len_MOD, if - then copy 16 bytes
        bbci  a6, 4,  _aligned_mod_16_check         // branch if 4-th bit of loop_len_MOD a6 is clear
            ee.vld.128.ip    q0,  a3,  16           // load 128 bits from arr_src to q0, increase arr_src pointer by 16 bytes
            ee.vst.128.ip    q0,  a2,  16           // save 128 bits to arr_dest from q0, increase arr_dest pointer by 16 bytes
        _aligned_mod_16_check:

        // Check modulo 8 of the loop_len_MOD, if - then copy 8 bytes
        bbci a6, 3, _aligned_mod_8_check            // branch if 3-rd bit of loop_len_MOD a6 is clear
            ee.vld.l.64.ip    q0,  a3,  8           // load lower 64 bits from arr_src a3 to q0, increase arr_src pointer by 8 bytes
            ee.vst.l.64.ip    q0,  a2,  8           // save lower 64 bits from q0 to arr_dest a2, increase arr_dest pointer by 8 bytes
        _aligned_mod_8_check:

        // Check modulo 4 of the loop_len_MOD, if - then copy 4 bytes
        bbci a6, 2, _aligned_mod_4_check            // branch if 2-nd bit of loop_len_MOD a6 is clear
            l32i.n      a15,  a3,  0                // load 32 bits from arr_src a3 to a15
            addi.n      a3,   a3,  4                // increment arr_src pointer by 4 bytes
            s32i.n      a15,  a2,  0                // save 32 bits from a15 to arr_dest a2
            addi.n      a2,   a2,  4                // increment arr_dest pointer by 4 bytes
        _aligned_mod_4_check:

        // Check modulo 2 of the loop_len_MOD, if - then copy 2 bytes
        bbci a6, 1, _aligned_mod_2_check            // branch if 1-st bit of loop_len_MOD a6 is clear
            l16ui       a15,  a3,  0                // load 16 bits from arr_src a3 to a15
            addi.n      a3,   a3,  2                // increment arr_src pointer by 2 bytes
            s16i        a15,  a2,  0                // save 16 bits from a15 to arr_dest a2
            addi.n      a2,   a2,  2                // increment arr_dest pointer by 2 bytes
        _aligned_mod_2_check:

        // Check modulo 1 of the loop_len_MOD, if - then copy 1 byte
        bbci a6, 0, _aligned_mod_32_check           // branch if 0-th bit of loop_len_MOD a6 is clear
            l8ui        a15,  a3,  0                // load 8 bits from arr_src a3 to a15
            s8i         a15,  a2,  0                // save 8 bits from a15 to arr_dest a2

    _aligned_mod_32_check:

    mov      a2,    a7                              // copy the initial arr_dest pointer from a7 to arr_dest a2
    retw.n                                          // return

    _less_than_16:

        // If the length of the copied array is lower than 16, it is faster not to use esp32s3-optimized functions

        // Check modulo 8 of the arr_len, if - then copy 8 bytes
        bbci    a4,  3, _less_than_16_mod_8_check   // branch if 3-rd bit of arr_len a4 is clear
            l32i.n      a15,  a3,  0                // load 32 bits from arr_src a3 to a15, offset 0
            l32i.n      a14,  a3,  4                // load 32 bits from arr_src a3 to a14, offset 4
            s32i.n      a15,  a2,  0                // save 32 bits from a15 to arr_dest a2, offset 0
            s32i.n      a14,  a2,  4                // save 32 bits from a14 to arr_dest a2, offset 4
            addi.n      a3,   a3,  8                // increment arr_src pointer by 8 bytes
            addi.n      a2,   a2,  8                // increment arr_dest pointer by 8 bytes
        _less_than_16_mod_8_check:

        // Check modulo 4 of the arr_len, if - then copy 4 bytes
        bbci a4, 2, _less_than_16_mod_4_check       // branch if 2-nd bit of arr_len a4 is clear
            l32i.n      a15,  a3,  0                // load 32 bits from arr_src a3 to a15
            addi.n      a3,   a3,  4                // increment arr_src pointer by 4 bytes
            s32i.n      a15,  a2,  0                // save 32 bits from a15 to arr_dest a2
            addi.n      a2,   a2,  4                // increment arr_dest pointer by 4 bytes
        _less_than_16_mod_4_check:

        // Check modulo 2 of the arr_len, if - then copy 2 bytes
        bbci a4, 1, _less_than_16_mod_2_check       // branch if 1-st bit of arr_len a4 is clear
            l16ui       a15,  a3,  0                // load 16 bits from arr_src a3 to a15
            addi.n      a3,   a3,  2                // increment arr_src pointer by 2 bytes
            s16i        a15,  a2,  0                // save 16 bits from a15 to arr_dest a2
            addi.n      a2,   a2,  2                // increment arr_dest pointer by 2 bytes
        _less_than_16_mod_2_check:

        // Check modulo 1 of the arr_len, if - then copy 1 byte
        bbci a4, 0, _less_than_16_mod_1_check       // branch if 0-th bit of arr_len a4 is clear
            l8ui        a15,  a3,  0                // load 8 bits from arr_src a3 to a15
            s8i         a15,  a2,  0                // save 8 bits from a15 to arr_dest a2
        _less_than_16_mod_1_check:

    mov      a2,    a7                              // copy the initial arr_dest pointer from a7 to arr_dest a2
    retw.n                                          // return


#else   // MEMCPY_OPTIMIZED

    // ansi version of the memcpy (without TIE instructions) for testing purposes

    entry    a1,    32
    mov      a7,    a2                              // a7 - save arr_dest pointer

    srli     a5,    a4,   4                         // a5 - loop_len = arr_len / 16

    // Run main loop which copies 16 bytes in one loop run
    loopnez a5, ._ansi_loop
        l32i.n      a15,  a3,  0                    // load 32 bits from arr_src a3 to a15
        l32i.n      a14,  a3,  4                    // load 32 bits from arr_src a3 to a14
        l32i.n      a13,  a3,  8                    // load 32 bits from arr_src a3 to a13
        l32i.n      a12,  a3,  12                   // load 32 bits from arr_src a3 to a13
        s32i.n      a15,  a2,  0                    // save 32 bits from a15 to arr_dest a2
        s32i.n      a14,  a2,  4                    // save 32 bits from a14 to arr_dest a2
        s32i.n      a13,  a2,  8                    // save 32 bits from a13 to arr_dest a2
        s32i.n      a12,  a2,  12                   // save 32 bits from a13 to arr_dest a2
        addi.n      a3,   a3,  16                   // increment arr_src pointer by 12 bytes
        addi.n      a2,   a2,  16                   // increment arr_dest pointer by 12 bytes
    ._ansi_loop:

    // Finish the remaining bytes out of the loop
    // Check modulo 8 of the arr_len, if - then copy 8 bytes
    bbci a4, 3, _mod_8_check                        // branch if 2-nd bit of arr_len a4 is clear
        l32i.n      a15,  a3,  0                    // load 32 bits from arr_src a3 to a15
        l32i.n      a14,  a3,  4                    // load 32 bits from arr_src a3 to a15
        s32i.n      a15,  a2,  0                    // save 32 bits from a15 to arr_dest a2
        s32i.n      a14,  a2,  4                    // save 32 bits from a15 to arr_dest a2
        addi.n      a3,   a3,  8                    // increment arr_src pointer by 4 bytes
        addi.n      a2,   a2,  8                    // increment arr_dest pointer by 4 bytes
    _mod_8_check:

    // Check modulo 4 of the arr_len, if - then copy 4 bytes
    bbci a4, 2, _mod_4_check                        // branch if 2-nd bit of arr_len a4 is clear
        l32i.n      a15,  a3,  0                    // load 32 bits from arr_src a3 to a15
        addi.n      a3,   a3,  4                    // increment arr_src pointer by 4 bytes
        s32i.n      a15,  a2,  0                    // save 32 bits from a15 to arr_dest a2
        addi.n      a2,   a2,  4                    // increment arr_dest pointer by 4 bytes
    _mod_4_check:

    // Check modulo 2 of the arr_len, if - then copy 2 bytes
    bbci a4, 1, _mod_2_check                        // branch if 1-st bit of arr_len a4 is clear
        l16ui       a15,  a3,  0                    // load 16 bits from arr_src a3 to a15
        addi.n      a3,   a3,  2                    // increment arr_src pointer by 2 bytes
        s16i        a15,  a2,  0                    // save 16 bits from a15 to arr_dest a2
        addi.n      a2,   a2,  2                    // increment arr_dest pointer by 2 bytes
    _mod_2_check:

    // Check modulo 1 of the arr_len, if - then copy 1 byte
    bbci a4, 0, _mod_1_check                        // branch if 0-th bit of arr_len a4 is clear
        l8ui        a15,  a3,  0                    // load 8 bits from arr_src a3 to a15
        s8i         a15,  a2,  0                    // save 8 bits from a15 to arr_dest a2
    _mod_1_check:

    // if arr_len is shorter than 16, skip adding TIE instruction, to fix the panic handler before the main_app() loads
    blti    a4,    16, _less_than_16_1              // branch, if arr_len a4 is shorter than 16 bytes
    #if TIE_ENABLE                                  // put dummy TIE instruction to induce TIE context saving
        ee.zero.qacc                                // initialize q0 to zero (dummy instruction)
    #else                   // TIE_ENABLE                                  
        nop                                         // compensate one cycle, when TIE is disabled to get the same benchmark value
    #endif                  // TIE_ENABLE
    _less_than_16_1:

    mov      a2,    a7                              // copy the initial arr_dest pointer from a7 to arr_dest a2
    retw.n                                          // return

#endif  // MEMCPY_OPTIMIZED

#endif  // dsps_mem_aes3_enbled
