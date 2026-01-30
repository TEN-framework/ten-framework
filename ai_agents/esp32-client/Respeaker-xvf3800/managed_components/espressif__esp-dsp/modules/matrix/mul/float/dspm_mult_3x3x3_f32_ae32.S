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
#if (dspm_mult_3x3x3_f32_ae32_enabled == 1)

// This is matrix multipliction function for ESP32 processor.
	.text
	.align  4
	.global dspm_mult_3x3x3_f32_ae32
	.type   dspm_mult_3x3x3_f32_ae32,@function
// The function implements the following C code:
// esp_err_t dspm_mult_3x3x1_f32_ansi(const float* A, const float* B, float* C, int m, int n, int k)
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

dspm_mult_3x3x3_f32_ae32: 
// A - a2
// B - a3
// C - a4

// a5 - 0
// a6 - 3 - internal loop for n
// a7 - 3 - external loop for M
	entry	a1, 16

	movi a5, 0
	movi a6, 3
	movi a7, 3 // loop ccount
	
m_loop_3x3x3:
		mov a12, a2 // A
		mov a14, a4 // output pointer

		lsi	    f12, a3, 0  // B[0][0]
		lsi	    f13, a3, 12 // B[1][0]
		lsi	    f14, a3, 24 // B[2][0]

		loopnez     a6, loop_mac_3x3x3_end_m_ae32
			wfr	    f0, a5
			
			lsi	    f2, a12, 0
			madd.s	f0, f2, f12
			lsi	    f3, a12, 4
			madd.s	f0, f3, f13
			lsi	    f4, a12, 8
			madd.s	f0, f4, f14
			
			addi	a12, a12, 12
			ssi	    f0, a14, 0
			addi    a14, a14, 12
		loop_mac_3x3x3_end_m_ae32:

		addi a3,  a3,  4  // increment input pointer B 
		addi a4, a4, 4
		addi a7, a7, -1
	bnez    a7, m_loop_3x3x3

	movi.n	a2, 0 // return status ESP_OK
	retw.n

#endif //