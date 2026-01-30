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

#include "dsps_mulc_platform.h"
#if (dsps_mulc_s16_ae32_enabled == 1)

    .text
    .align  4
    .global dsps_mulc_s16_ae32
    .type   dsps_mulc_s16_ae32,@function
// The function implements the following C code:
// esp_err_t dsps_mulc_f32_ansi(const float *input, float *output, int len, float C, int step_in, int step_out)
// {
//     for (int i = 0 ; i < len ; i++) {
//         int32_t acc = (int32_t)input[i * step_in] * (int32_t)C;
//         output[i * step_out] = (int16_t)(acc>>15);
//     }
//     return ESP_OK;
// }
dsps_mulc_s16_ae32: 
// input   - a2
// output   - a3
// len      - a4
// C        - a5
// step_in  - a6
// step_out - a7

    entry	a1, 16

    movi.n	a8, 15      // output shift
    ssr     a8

    srli    a4, a4, 1   // a4 = a4>>1
    slli 	a6, a6, 2  	// a6 - step_in<<3, because we load two inputs per loop
    slli 	a7, a7, 1  	// a7 - step_out<<2

    addi    a6, a6, -4;
    addi    a2, a2, -4;

	ldinc m0, a2

    loopnez a4, loop_end_mulc_f32_ae32
        add.n       a2, a2, a6     // input+=step_input;
        mul.DA.LL   m0, a5 
        rsr a8, acchi
        rsr a9, acclo
        src a8, a8, a9  // Here result in a8
    	s16i	a8, a3, 0   // store result to the putput        
        // rsr a9, acclo
    	// s16i	a9, a3, 0   // store result to the putput        
        add.n   a3, a3, a7     // output+=step_out;
        mul.DA.HL   m0, a5         

        rsr a8, acchi
        rsr a9, acclo
	    ldinc       m0,   a2               // load next data
        src a10, a8, a9  // Here result in a8
    	s16i	a10, a3, 0   // store result to the putput
        // // rsr a9, acclo
    	// // s16i	a9, a3, 0   // store result to the putput        
        add.n   a3, a3, a7  // output+=step_out;
loop_end_mulc_f32_ae32:
    movi.n	a2, 0 // return status ESP_OK
    retw.n
    
#endif // dsps_mulc_s16_ae32_enabled