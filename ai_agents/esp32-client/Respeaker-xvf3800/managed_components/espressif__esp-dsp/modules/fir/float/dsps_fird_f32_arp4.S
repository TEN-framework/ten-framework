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

#include "dsps_fir_platform.h"
#if (dsps_fird_f32_arp4_enabled == 1)

// This is FIR filter for esp32p4 processor.
    .text
    .align  4
    .global dsps_fird_f32_arp4
    .type   dsps_fird_f32_arp4,@function
// The function implements the following C code:
//esp_err_t dsps_fird_f32_arp4(fir_f32_t* fir, const float* input, float* output, int len);

dsps_fird_f32_arp4:
    add sp,sp,-16
    
    mv  a6, a3
    lw  t1, 4(a0)   // t1 - delay
    lw  a4, 4(a0)   // a4 - delay 
    lw  t2, 8(a0)   // t2 - N :FIR filter coefficients amount
    lw  t3, 12(a0)  // t3 - pos
    lw  t4, 16(a0)  // t4 - decim
    slli    t3, t3, 2   // t5 = pos*4 (bytes)
    add     t1, t1, t3  // delay[pos]
    slli    t6, t2, 2   // t6 = N*4 (bytes)
    add     t3, a4, t6  // last position for the daly[N]

    nop
.fird_loop_len:
//    p.lw      a1, 4(a1)
//fmv.w.x   fa5,zero
    flw         fa0, 0(a1)  // f0 = x[i],  first load
    esp.lp.setup    0, t4, .fird_load_data      // label to the last executed instruction
        add         a1, a1, 4                   // i++
        fsw         fa0, 0(t1)                  // delay[pos]
        add         t1, t1, 4
        blt         t1, t3, .do_not_reset_pos # if t0 < t1 then target
            lw  t1, 4(a0)   // t1 - delay
    .do_not_reset_pos:
    .fird_load_data:    flw         fa0, 0(a1)                  // f0 = x[i]

    lw      t0, 0(a0)   // t0 - coeffs
    sub     t5, t3, t1  // (last_pos - pos)*4
    srli    t5, t5, 2   // N-pos
    sub     t6, t1, a4
    srli    t6, t6, 2   // pos

    fmv.w.x fa2,zero
    
    lw  a5, 0(a0)   // a5 - coeffs 
    esp.lp.setup    0, t5, .first_fird_loop
        flw     fa1, 0(a5)
        flw     fa0, 0(t1)
        addi    a5, a5, 4
        fmadd.s   fa2, fa1, fa0, fa2
.first_fird_loop:  addi      t1, t1, 4


    lw  t1, 4(a0)   // t1 - delay

    beqz    t6, .skeep_loop
    esp.lp.setup    0, t6, .second_fird_loop
        flw     fa1, 0(a5)
        flw     fa0, 0(t1)
        addi    a5, a5, 4
        fmadd.s   fa2, fa1, fa0, fa2
.second_fird_loop:   addi      t1, t1, 4

.skeep_loop:
    // Store result

    fsw     fa2, 0(a2)
    addi    a2, a2, 4

    addi    a3, a3, -1
    BNEZ    a3, .fird_loop_len// Jump if > 0

    sub     t6,  t1, a4
    srli    t6, t6, 2   // pos

    sw  t6, 12(a0)  // t3 - pos

    mv  a0, a6
    add sp,sp,16
    ret

#endif // 