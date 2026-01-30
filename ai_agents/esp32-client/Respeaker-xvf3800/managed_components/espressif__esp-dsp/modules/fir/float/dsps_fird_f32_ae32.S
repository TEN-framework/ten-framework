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
#if (dsps_fird_f32_ae32_enabled == 1)

#include "dsps_dotprod_f32_m_ae32.S"

// This is FIR filter for ESP32 processor.
	.text
	.align  4
	.global dsps_fird_f32_ae32
	.type   dsps_fird_f32_ae32,@function
// The function implements the following C code:
//esp_err_t dsps_fird_f32_ae32(fir_f32_t* fir, const float* input, float* output, int len);

dsps_fird_f32_ae32: 
// fir      - a2
// input    - a3
// output   - a4
// len      - a5

	entry	a1, 16
	// Array increment for floating point data should be 4
	l32i    a7,  a2, 12 // a7  - pos
	movi    a10, 4
	mull    a13, a7, a10// a13 - a7*4
	l32i    a6,  a2, 8  // a6  - N
	mull    a6, a6, a10// a6 = a6*4
	l32i    a10, a2, 0  // a10 - coeffs
	l32i    a11, a2, 4  // a11 - delay line
	l32i    a6,  a2, 8  // a6  - N
	l32i    a12, a2, 16  // a12  - decimation
	movi    a8, 0         // result = 0;

//  a13 - delay index
fird_loop_len:
		// Store to delay line
		
        loopnez  a12, .fird_load_data // K loops
			lsip    f0, a3, 4       // f0 = x[i++]
			ssx     f0, a11, a13    // delay[a13] = f0;
			addi    a13, a13, 4     // a13++
			addi    a7, a7, 1       // a7++
			// verify deley line
			blt     a7, a6, do_not_reset_a13
				movi    a13, 0
				movi    a7,  0
			do_not_reset_a13:
        	const.s f2, 0
		.fird_load_data:

		addi    a8, a8, 1

		// Calc amount for delay line before end
		mov     a15, a10        // a15 - coeffs
		sub   a14, a6, a7   // a14 = N-pos
		loopnez  a14, first_fird_loop // pos...N-1
			lsip    f1, a15, 4		// a15++
			lsx     f0, a11, a13    // load delay f0 = delay[pos]
			addi    a13, a13, 4     // a13++, pos++
			madd.s  f2, f0, f1      // f2 += f0*f1
first_fird_loop:
		movi a13, 0    // load delay line counter to 0
		loopnez  a7, second_fird_loop // 0..pos
			lsip    f1, a15, 4		// a15++
			lsx     f0, a11, a13    // load delay f0 = delay[pos]
			addi    a13, a13, 4     // a13++, pos++
			madd.s  f2, f0, f1      // f2 += f0*f1
second_fird_loop:

		// and after end
		// Store result
		ssi     f2, a4, 0
		addi    a4, a4, 4 // y++ - increment output pointer
next_itt_fir32:        
		// Check loop 
		addi   a5, a5, -1
	bnez    a5, fird_loop_len
	// store state

	s32i    a7,  a2, 12 // pos = a7
	
	mov 	a2, a8 // return status ESP_OK
	retw.n

#endif // dsps_fird_f32_ae32_enabled