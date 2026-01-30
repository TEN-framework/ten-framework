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

#include "dsps_conv_platform.h"
#if (dsps_corr_f32_ae32_enabled == 1)

#include "dsps_dotprod_f32_m_ae32.S"

// This is dot product function for ESP32 processor.
	.text
	.align  4
	.global dsps_corr_f32_ae32
	.type   dsps_corr_f32_ae32,@function
// The function implements the following C code:
//esp_err_t dsps_corr_f32_ansi(const float *Signal, const int siglen, const float *Pattern, const int patlen, float *dest)
//{
//    for (size_t n = 0; n < (siglen - patlen); n++) {
//        float k_corr = 0;
//        for (size_t m = 0; m < patlen; m++) {
//            k_corr += Signal[n + m] * Pattern[m];
//        }
//        dest[n] = k_corr;
//    }
//    return ESP_OK;
//}

dsps_corr_f32_ae32: 
// Signal  - a2
// siglen  - a3
// Pattern - a4
// patlen  - a5
// dest    - a6
// a11 - loop length

	entry	a1, 16
	// Array increment for floating point data should be 4
	movi.n	a8, 4
	movi.n	a13, 4
	sub     a11, a3, a5 // a11 = loop length
	addi	a11, a11, 1
	addi    a12, a2, 0 	// move input pointer to the a12 
	movi.n	a9, 0
	movi.n	a14, 0

corr_loop:	
		// Clear initial state of the result register
		addi    a10, a4, 0  // a10 - pattern
		movi.n	a9, 0		// clear a9
		wfr	    f1, a9		// clrar f1
		// a12 - input1		
		// a10 - input2
		// a5  - length
		// a8  - 4,  step in arrays
		// a9  - 0
		dotprod_f32_ae32 a12, a10, a5, a9, a8;

		ssi		f1, a6, 0 // Store result from f1 to memory at a6
		addi    a6, a6, 4 	// y++ - increment output pointer
		addi 	a12, a12, 4	// Signal++
		addi 	a11, a11, -1
	bnez    a11, corr_loop
	
	movi.n	a2, 0 // return status ESP_OK
	retw.n

#endif // dsps_corr_f32_ae32_enabled