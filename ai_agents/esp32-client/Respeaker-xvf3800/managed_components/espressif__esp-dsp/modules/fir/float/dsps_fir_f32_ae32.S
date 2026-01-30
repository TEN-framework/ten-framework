// Copyright 2018-2023 Espressif Systems (Shanghai) PTE LTD
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
#if (dsps_fir_f32_ae32_enabled == 1)

#include "dsps_dotprod_f32_m_ae32.S"

// This is FIR filter for ESP32 processor.
	.text
	.align  4
	.global dsps_fir_f32_ae32
	.type   dsps_fir_f32_ae32,@function
// The function implements the following C code:
//esp_err_t dsps_fir_f32_ae32(fir_f32_t* fir, const float* input, float* output, int len);

dsps_fir_f32_ae32: 
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
	l32i    a6,  a2, 8  // a6  - N

	movi.n a9, 0
	movi.n a8, 4
	movi.n a12, 4

//  a13 - delay index
fir_loop_len:
		// Store to delay line
		l32i    a11, a2, 4      // a11 - delay line
		lsi     f0, a3, 0       // f0 = x[i]
		addi    a3, a3, 4       // x++
		ssx     f0, a11, a13    // delay[a13] = f0;
		addi    a13, a13, 4     // a13++
		addi    a7, a7, 1       // a7++
		// verify deley line
		blt     a7, a6, do_not_reset_a13
			movi    a13, 0
			movi    a7,  0
	do_not_reset_a13:
		// Calc amount for delay line before end
		mov     a15, a10        // a15 - coeffs
		wfr	    f2, a9 // f2 = 0;
		sub   a14, a6, a7   // a14 = N-pos

		// a11 = &delay[pos]
		add     a11, a11, a13

		loopnez  a14, first_fir_loop // pos...N-1
			lsxp     f1, a15, a8     // f1 = *(coeffs--)
			lsxp     f0, a11, a12    // load delay f0 = *(delay++)
			madd.s  f2, f0, f1       // f2 += f0*f1
first_fir_loop:
		l32i    a11, a2, 4           // a11 - delay line
		loopnez  a7, second_fir_loop // 0..pos
			lsxp     f1, a15, a8     // f1 = *(coeffs--)
			lsxp     f0, a11, a12    // load delay f0 = *(delay++)
			madd.s  f2, f0, f1      // f2 += f0*f1
second_fir_loop:

		// and after end
		// Store result
		ssi     f2, a4, 0
		addi    a4, a4, 4 // y++ - increment output pointer
		// Check loop 
		addi   a5, a5, -1
	bnez    a5, fir_loop_len
	// store state

	s32i    a7,  a2, 12 // pos = a7
	movi.n	a2, 0 // return status ESP_OK
	retw.n

#endif // dsps_fir_f32_ae32_enabled