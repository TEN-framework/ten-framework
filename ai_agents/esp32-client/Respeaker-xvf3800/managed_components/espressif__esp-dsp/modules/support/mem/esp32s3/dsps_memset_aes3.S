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
    .global dsps_memset_aes3
    .type   dsps_memset_aes3,@function
// The function implements the following C code:
// void *dsps_memset_aes3(void *arr_dest, uint8_t set_val, size_t set_size);

// Input params                 Variables
//
// arr_dest - a2                loop_len    - a5
// set_val  - a3                p_arr_dest  - a8
// set_size - a4                8_bit_set   - a7
//                              16_bit_set  - a9
//                              32_bit_set  - a10
//                              align_mask  - a11

/*
esp32s3 optimized memset function works with both, aligned and unaligned data.

arr_dest aligned         - _main_loop, 16 bytes in one loop, only aligned data
                         - Check modulos to finish copying remaining data outside of the cycle
                         - Modulo 8 - S3 instruction for aligned data, the rest of the modulos are generic

arr_dest unaligned       - First, use generic instructions to align the arr_dest data (keep increasing 
                           the arr_dest pointer until the pointer is aligned)
                         - Once arr_dest is aligned treat the rest of the data as aligned, same as above

if the set_size is less than 16, jump to _less_than_16 label and set data without any s3 instructions or cycles
*/

#define MEMSET_OPTIMIZED    1           // Use optimized memset or ansi memset
#define TIE_ENABLE          0           // Put a dummy TIE instruction to ANSI memset to induce TIE context saving

dsps_memset_aes3:

#if MEMSET_OPTIMIZED

    entry   a1,    32
    mov     a8,    a2                               // a8 - save arr_dest pointer 
    blti    a4,    16, _less_than_16                // set_size shorter than 16

    movi.n  a7,    0xff                             // 0xff one-byte mask
    movi.n  a11,   0xf                              // 0xf  alignment mask
    and     a7,    a7,   a3                         // mask upper 24 bits of set_val a3

    bnez.n  a7, _non_zero_constant
        ee.zero.q  q0                               // initialize q0 to zero
        movi.n     a9,  0                           // initialize (16_bit_set) a9 to zero
        movi.n     a10, 0                           // initialize (32_bit_set) a10 to zero
        j _q_reg_prepared

    _non_zero_constant:
        // Fill q register
        slli    a6,    a7,   8                      // a6 - (masked)set_val << 8
        or      a9,    a6,   a7                     // a9 - (masked)set_val << 8 + (masked)set_val
                                                    // a9 - 16-bit set
        slli    a15,    a9,   16                    // a15 - a9 << 16
        or      a10,    a9,   a15                   // broadcast 8 bits from set_val a3 to 32 bits
                                                    // a10 - 32-bit set
        ee.movi.32.q   q0,   a10,  0                // fill q0 register from a10 by 32 bits
        ee.movi.32.q   q0,   a10,  1
        ee.movi.32.q   q0,   a10,  2
        ee.movi.32.q   q0,   a10,  3

    _q_reg_prepared:

    // alignment check
    and     a15,   a11,  a2                         // 0xf (alignment mask) AND arr_dest pointer
    beqz    a15,   _arr_dest_aligned                // branch if a15 equals to zero

        movi.n  a14,   16                           // a14 - 16
        sub     a15,   a14,   a15                   // a15 = 16 - unalignment
        sub     a4,    a4,    a15                   // len = len - (16 - unalignment)

        // keep setting until arr_dest is aligned
        // Check modulo 8 of the unalignment, if - then set 8 bytes
        bbci    a15,  3, _aligning_mod_8_check      // branch if 3-rd bit of unalignment a15 is clear
            s32i.n      a10,  a2,  0                // save 32 bits from a10 to arr_dest a2, offset 0 bytes
            s32i.n      a10,  a2,  4                // save 32 bits from a10 to arr_dest a2, offset 4 bytes
            addi.n      a2,   a2,  8                // increment arr_dest pointer by 8 bytes
        _aligning_mod_8_check:

        // Check modulo 4 of the unalignment, if - then set 4 bytes
        bbci a15, 2, _aligning_mod_4_check          // branch if 2-nd bit unalignment a15 is clear
            s32i.n      a10,  a2,  0                // save 32 bits from a10 to arr_dest a2, offset 0 bytes
            addi.n      a2,   a2,  4                // increment arr_dest pointer by 4 bytes
        _aligning_mod_4_check:

        // Check modulo 2 of the unalignment, if - then set 2 bytes
        bbci a15, 1, _aligning_mod_2_check          // branch if 1-st bit unalignment a15 is clear
            s16i        a9,   a2,  0                // save 16 bits from a9 to arr_dest a2, offset 0 bytes
            addi.n      a2,   a2,  2                // increment arr_dest pointer by 2 bytes
        _aligning_mod_2_check:

        // Check modulo 1 of the unalignment, if - then copy 1 byte
        bbci a15, 0, _arr_dest_aligned              // branch if 0-th bit unalignment a15 is clear
            s8i         a7,   a2,  0                // save 8 bits from a7 to arr_dest a2, offset 0 bytes
            addi.n      a2,   a2,  1                // increment arr_dest pointer by 1 byte


    _arr_dest_aligned:
    // Calculate main loop_len
    srli    a5,    a4,   4                          // a5 - loop_len = set_size / 16

    // Main loop
    loopnez  a5, ._main_loop                        // 16 bytes in one loop
        ee.vst.128.ip q0, a2, 16                    // store 16 bytes from q0 to arr_dest a2
    ._main_loop:

    // Check modulo 8 of the set_size, if - then set 8 bytes
    bbci a4, 3, _aligned_mod_8_check                // branch if 3-rd bit of set_size a4 is clear
        ee.vst.l.64.ip    q0,  a2,  8               // save lower 64 bits from q0 to arr_dest a2, increase arr_dest pointer by 8 bytes
    _aligned_mod_8_check:

    // Check modulo 4 of the set_size, if - then set 4 bytes
    bbci a4, 2, _aligned_mod_4_check                // branch if 2-nd bit of set_size a4 is clear
        s32i.n      a10,  a2,  0                    // save 32 bits from a10 to arr_dest a2, offset 0 bytes
        addi.n      a2,   a2,  4                    // increment arr_dest pointer by 4 bytes
    _aligned_mod_4_check:

    // Check modulo 2 of the set_size, if - then set 2 bytes
    bbci a4, 1, _aligned_mod_2_check                // branch if 1-st bit of set_size a4 is clear
        s16i        a9,   a2,  0                    // save 16 bits from a9 to arr_dest a2, offset 0 bytes
        addi.n      a2,   a2,  2                    // increment arr_dest pointer by 2 bytes
    _aligned_mod_2_check:

    // Check modulo 1 of the set_size, if - then set 1 byte
    bbci a4, 0, _aligned_mod_1_check                // branch if 0-th bit of set_size a4 is clear
        s8i         a7,   a2,  0                    // save 8 bits from a7 to arr_dest a2, offset 0 bytes
    _aligned_mod_1_check:

    mov     a2,   a8                                // copy the initial arr_dest pointer from a8 to arr_dest a2
    retw.n                                          // return

    _less_than_16:

        // make 16-byte set_val
        slli    a6,    a3,   8                      // a6 - a3 (set_val) << 8
        or      a7,    a6,   a3                     // a7 - a3 (set_val) << 8 + a3 (set_val)

        // Check modulo 8 of the set_size, if - then set 8 bytes
        bbci a4, 3, _less_than_16_mod_8_check       // branch if 3-rd bit of set_size a4 is clear
            s16i        a7,  a2,  0                 // save 16 bits from a7 to arr_dest a2, offset 0 bytes
            s16i        a7,  a2,  2                 // save 16 bits from a7 to arr_dest a2, offset 2 bytes
            s16i        a7,  a2,  4                 // save 16 bits from a7 to arr_dest a2, offset 4 bytes
            s16i        a7,  a2,  6                 // save 16 bits from a7 to arr_dest a2, offset 6 bytes
            addi.n      a2,  a2,  8                 // increment arr_dest pointer by 8 bytes
        _less_than_16_mod_8_check:

        // Check modulo 4 of the set_size, if - then set 4 bytes
        bbci a4, 2, _less_than_16_mod_4_check       // branch if 2-nd bit of set_size a4 is clear
            s16i        a7,  a2,  0                 // save 16 bits from a7 to arr_dest a2, offset 0 bytes
            s16i        a7,  a2,  2                 // save 16 bits from a7 to arr_dest a2, offset 2 bytes
            addi.n      a2,  a2,  4                 // increment arr_dest pointer by 4 bytes
        _less_than_16_mod_4_check:

        // Check modulo 2 of the set_size, if - then set 2 bytes
        bbci a4, 1, _less_than_16_mod_2_check       // branch if 1-st bit of set_size a4 is clear
            s16i        a7,  a2,  0                 // save 16 bits from a7 to arr_dest a2, offset 0 bytes
            addi.n      a2,  a2,  2                 // increment arr_dest pointer by 2 bytes
        _less_than_16_mod_2_check:

        // Check modulo 1 of the set_size, if - then set 1 byte
        bbci a4, 0, _less_than_16_mod_1_check       // branch if 0-th bit of set_size a4 is clear
            s8i         a3,  a2,   0                // save 8 bits from a3 to arr_dest a2, offset 0 bytes
        _less_than_16_mod_1_check:

    mov     a2,   a8                                // copy the initial arr_dest pointer from a8 to arr_dest a2
    retw.n                                          // return


#else   // MEMSET_OPTIMIZED

    // ansi version of the memset (without TIE instructions) for testing purposes

    entry    a1,    32
    mov      a8,    a2                              // a8 - save arr_dest pointer

    movi.n  a7,    0xff                             // 0xff one-byte mask
    and     a7,    a7,   a3                         // mask upper 24 bits of a3

    slli    a6,    a7,   8                          // a6 - (masked)set_val << 8
    or      a9,    a6,   a7                         // a9 - (masked)set_val << 8 + (masked)set_val
                                                    // a9 - 16-bit set
    slli    a15,    a9,   16                        // a15 - a9 << 16
    or      a10,    a9,   a15                       // broadcast 8 bits from a3 to 32 bits 

    srli    a5,    a4,   4                          // a5 - loop_len = arr_len / 16

    // Run main loop which sets 16 bytes in one loop run
    loopnez a5, ._ansi_loop
        s32i.n      a10,  a2,  0                    // save 32 bits from a15 to arr_dest a2
        s32i.n      a10,  a2,  4                    // save 32 bits from a14 to arr_dest a2
        s32i.n      a10,  a2,  8                    // save 32 bits from a14 to arr_dest a2
        s32i.n      a10,  a2,  12                   // save 32 bits from a14 to arr_dest a2
        addi.n      a2,   a2,  16                   // increment arr_dest pointer by 8 bytes
    ._ansi_loop:

    // Finish the remaining bytes out of the loop
    // Check modulo 8 of the arr_len, if - then set 8 bytes
    bbci a4, 3, _mod_8_check                        // branch if 2-nd bit of arr_len is clear
        s32i.n      a10,  a2,  0                    // save 32 bits from a10 to arr_dest a2, offset 0 bytes
        s32i.n      a10,  a2,  4                    // save 32 bits from a10 to arr_dest a2, offset 0 bytes
        addi.n      a2,   a2,  8                    // increment arr_dest pointer by 4 bytes
    _mod_8_check:

    // Check modulo 4 of the arr_len, if - then set 4 bytes
    bbci a4, 2, _mod_4_check                        // branch if 2-nd bit of arr_len is clear
        s32i.n      a10,  a2,  0                    // save 32 bits from a10 to arr_dest a2, offset 0 bytes
        addi.n      a2,   a2,  4                    // increment arr_dest pointer by 4 bytes
    _mod_4_check:

    // Check modulo 2 of the arr_len, if - then set 2 bytes
    bbci a4, 1, _mod_2_check                        // branch if 1-st bit of arr_len is clear
        s16i        a9,  a2,  0                     // save 16 bits from a7 to arr_dest a2, offset 0 bytes
        addi.n      a2,  a2,  2                     // increment arr_dest pointer by 2 bytes
    _mod_2_check:

    // Check modulo 1 of the arr_len, if - then set 1 byte
    bbci a4, 0, _mod_1_check                        // branch if 0-th bit of arr_len is clear
        s8i         a7,  a2,   0                    // save 8 bits from a3 to arr_dest a2, offset 0 bytes
    _mod_1_check:

    // if arr_len is shorter than 16, skip adding TIE instruction, to fix the panic handler before the main_app() loads
    blti    a4,    16, _less_than_16_1              // set_size shorter than 16, to fix panic handler before main_app() load
    #if TIE_ENABLE                                  // put dummy TIE instruction to induce TIE context saving
        ee.zero.qacc                                // initialize q0 to zero
    #else                   // TIE_ENABLE                                  
        nop                                         // compensate one cycle, when TIE is disabled to get the same benchmark value
    #endif                  // TIE_ENABLE
    _less_than_16_1:

    mov      a2,    a8                              // copy the initial arr_dest pointer from a8 to arr_dest a2
    retw.n                                          // return

#endif  // MEMSET_OPTIMIZED

#endif  // dsps_mem_aes3_enbled