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

#include "dsps_addc_platform.h"
#if (dsps_addc_f32_ae32_enabled == 1)

// This is bi quad filter form II for ESP32 processor.
    .text
    .align  4
    .global dsps_addc_f32_ae32
    .type   dsps_addc_f32_ae32,@function
// The function implements the following C code:
// esp_err_t dsps_addc_f32_ansi(const float *input, float *output, int len, float C, int step_in, int step_out)
// {
//     for (int i = 0 ; i < len ; i++) {
//         output[i * step_out] = input[i * step_in] + C;
//     }
//     return ESP_OK;
// }
dsps_addc_f32_ae32: 
// input   - a2
// output   - a3
// len      - a4
// C        - a5
// step_in  - a6
// step_out - a7

    entry	a1, 16
    
    slli 	a6, a6, 2  	// a6 - step_in<<2
    slli 	a7, a7, 2  	// a7 - step_out<<2
    wfr		f0, a5		// a5 - load to the f0

    loopnez a4, loop_end_addc_f32_ae32
        lsi      f1, a2, 0

        add.s    f2, f1, f0     // f2 = f1 + f0
        add.n    a2, a2, a6     // input1_ptr+=step_in;
        ssi	     f2, a3, 0
        add.n    a3, a3, a7     // output+=step_out;
loop_end_addc_f32_ae32:

    movi.n	a2, 0 // return status ESP_OK
    retw.n

#endif // dsps_addc_f32_ae32_enabled