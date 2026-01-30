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

#include "dsps_add_platform.h"
#if (dsps_add_s16_ae32_enabled == 1)

    .text
    .align  4
    .global dsps_add_s16_ae32
    .type   dsps_add_s16_ae32,@function
// The function implements the following C code:
// esp_err_t dsps_add_s16_ansi(const int16_t *input1, const int16_t *input2, int16_t *output, int len, int step1, int step2, int step_out, int shift)
// {
//     for (int i = 0 ; i < len ; i++) {
//         int32_t acc = (int32_t)input1[i * step1] + (int32_t)input2[i * step2];
//         output[i * step_out] = acc >> shift;
//     }
//     return ESP_OK;
// }
dsps_add_s16_ae32: 
// input1   - a2
// input2   - a3
// output   - a4
// len      - a5
// step_in1 - a6
// step_in2 - a7
// step_out - stack (a10)
// shift    - stack (a9)

    entry	a1, 16


    // l32i.n	a10, a1, 16
    // s16i	a10, a4, 0
    // l32i.n	a10, a1, 20
    // s16i	a10, a4, 2       

    l32i.n	a10, a1, 16     // Load step_out to the a10 register
    l32i.n	a9, a1,  20     // Load shift to the a9 register
    ssr     a9              // sar = a9

    slli 	a6, a6, 1  	// a6 - step_in<<1
    slli 	a7, a7, 1  	// a7 - step_in<<1
    slli 	a10, a10, 1 // a8 - step_out<<1

    // s16i	a10, a4, 0
    // s16i	a6, a4, 2
    // s16i	a7, a4, 4
    // s16i	a5, a4, 6

    l16si   a11, a2, 0
    l16si   a8,  a3, 0
    add     a8, a11, a8
    srl     a9, a8          // a8 = a8>>sar    

    loopnez a5, .loop_end_add_s16_ae32
        add.n   a2, a2, a6      // input1+=step_in1;
        add.n   a3, a3, a7      // input2+=step_in2;

        l16si   a11, a2, 0
        l16si   a8,  a3, 0
        s16i	a9,  a4, 0      // store result to the putput
        add     a8, a11, a8
    	srl     a9, a8          // a8 = a8>>sar    

        add.n   a4, a4, a10     // output+=step_out;
.loop_end_add_s16_ae32:
    movi.n	a2, 0 // return status ESP_OK
    retw.n

#endif // dsps_add_s16_ae32_enabled