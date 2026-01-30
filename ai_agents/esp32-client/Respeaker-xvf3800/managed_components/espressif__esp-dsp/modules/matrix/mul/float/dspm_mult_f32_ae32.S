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

#include "dspm_mult_platform.h"
#if (dspm_mult_f32_ae32_enabled == 1)

#include "dsps_dotprode_f32_m_ae32.S"

// This is matrix multipliction function for ESP32 processor.
	.text
	.align  4
	.global dspm_mult_f32_ae32
	.global .dspm_mult_f32_ae32_body
	.type   dspm_mult_f32_ae32,@function
// The function implements the following C code:
// esp_err_t dspm_mult_f32_ansi(const float* A, const float* B, float* C, int m, int n, int k)
// {
	// for (int i=0 ; i< m ; i++)
	// {
	//     for (int j=0 ; j< k ; j++)
	//     {
	//         C[i*k + j] = A[i*n]*B[j];
	//         for (int s=1; s< n ; s++)
	//         {
	//             C[i*k + j] += A[i*n + s]*B[s*k + j];
	//         }
	//     }
	// }
//     return ESP_OK;
// }

dspm_mult_f32_ae32: 
// A - a2
// B - a3
// C - a4
// m - a5
// n - a6
// k - a7

// a8  = n*4
// a10 = 4
// a9  - counter loop1: 0..m
// a11 - counter loop2: 0..k
// a12 - A
// a13 - B
// a4  - C

	entry	a1, 16
	// Array increment for floating point data should be 4
.dspm_mult_f32_ae32_body:
	slli    a8, a6, 2 // Pointer increment for A
	slli    a15,a7, 2 // Pointer increment for B

	movi.n	a14, 0 // Innitial state of accumulator f1
	movi.n	a10, 4 // Increment = 4
	movi.n	a9, 0  // counter loop1

.dpf_loop1:    
	movi.n	a11, 0 // reset counter for loop2
.dpf_loop2:

	// Clear initial state of the result register
	// a2 - A
	// a3 - B
	// a6 - n
	// a10 - step == 4 bytes
	// a8 -  step n*4
	mov      a12, a2 // load A

	slli     a13, a11, 2 // loop count to pointer value
	add.n    a13, a3, a13 // load A

	wfr	    f1, a14 // reset f1
	// Calculating dotproduct...
	dotprode_f32_ae32 a12, a13, a6, a10, a15;

	ssi	    f1, a4, 0 // Store result from f1 to memory at a4
	addi    a4, a4, 4 // increment a4 for next time

	// check loop 2
	addi  a11, a11, 1 // Increment loop2 counter
	blt   a11, a7, .dpf_loop2

	// check loop 1
	add.n   a2, a2, a8 // Increment A, A = A[i*n]

	addi  a9, a9, 1 // Increment loop1 counter
	blt   a9, a5, .dpf_loop1

	movi.n	a2, 0 // return status ESP_OK
	retw.n

#endif //dspm_mult_f32_ae32_enabled