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

#include "dsps_mul_platform.h"
#if (dsps_mul_f32_ae32_enabled == 1)

// This is bi quad filter form II for ESP32 processor.
    .text
    .align  4
    .global dsps_mul_f32_ae32
    .type   dsps_mul_f32_ae32,@function
// The function implements the following C code:
// esp_err_t dsps_mul_f32_ansi(const float *input1, const float *input2, float *output, int len, int step1, int step2, int step_out)
// {
//     for (int i = 0 ; i < len ; i++) {
//         output[i * step_out] = input1[i * step1] * input2[i * step2];
//     }
//     return ESP_OK;
// }

dsps_mul_f32_ae32: 
// input1   - a2
// input2   - a3
// output   - a4
// len      - a5
// step1    - a6
// step2    - a7
// step_out - stack (a8) 

    entry	a1, 16
    
    l32i.n	a8, a1, 16 // Load step_out to the a8 register
    slli 	a6, a6, 2  // a6 - step1<<2
    slli 	a7, a7, 2  // a7 - step2<<2
    slli 	a8, a8, 2  // a8 - step_out<<2

        lsi     f0, a2, 0
        add.n   a2, a2, a6     // input1_ptr+=step1;
    loopnez a5, loop_end_mul_f32_ae32
        lsi      f1, a3, 0
        add.n    a3, a3, a7     // input2_ptr+=step2;

        mul.s    f2, f1, f0     // f2 = f1*f0
        lsi      f0, a2, 0
        add.n    a2, a2, a6     // input1_ptr+=step1;        
        ssi	     f2, a4, 0
        add.n    a4, a4, a8     // input2_ptr+=step2;
loop_end_mul_f32_ae32:

    movi.n	a2, 0 // return status ESP_OK
    retw.n

#endif // dsps_mul_f32_ae32_enabled
