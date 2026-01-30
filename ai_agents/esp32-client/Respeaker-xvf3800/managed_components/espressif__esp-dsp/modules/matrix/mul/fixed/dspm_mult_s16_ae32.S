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
#if (dspm_mult_s16_ae32_enabled == 1)

#include "dsps_dotprod_s16_m_ae32.S"
#include "dspm_mult_s16_m_ae32_vector.S"
//esp_err_t dspm_mult_s16_ae32(const int16_t* A, const int16_t* B, int16_t* C, int m, int n, int k, int shift);

// This is matrix multipliction function for ESP32 processor.
	.text
	.align  4
	.global dspm_mult_s16_ae32
	.global .dspm_mult_s16_ae32_body
	.type   dspm_mult_s16_ae32,@function

dspm_mult_s16_ae32: 
// A - a2
// B - a3
// C - a4
// m - a5 - any > 0
// n - a6 - 1,2,3, any
// k - a7 - 1, any
// shift - stack (a8) 

// a14 - n*4 - pointer increment
//
	entry	a1, 80
// ======     process matrices when k == 1   ============
.dspm_mult_s16_ae32_body:
	l32i.n	a8, a1, 80 // Load shift to the a8 register
	

	// Prepare and load round value
	ssr a8 // store shift to ssa
	movi a15, 0x7fff
	srl a15, a15

	neg  a8, a8 
	addi a8, a8, 15
	ssr a8 // store shift to ssa
	movi a8, 0  // Clear a8 

	slli    a14, a6, 1 // Pointer increment for n
	movi.n	a10, 2 // Increment = 2
	movi.n	a9, 0  // initial counter loop1

	movi     a12, 1
	beq      a7, a12, vector_mult
	// We have normal path with k > 1
	// a2, a3, a4 - A,B,C
	// a5 - m
	// a6 - n
	// a7 - k
	// a8 - temp
	// a9 - temp
	// a10- k counter
	// a11- m counter
	// a12- B
	// a13- A 
	// a14 - pointer increment for n
	// a15 - round value

	bbsi  a6, 0, even_N_samples
//  ----------------  for odd N
	srli    a6, a6, 1 // counter a6 = a6/2. We have to do it only once
	slli    a7, a7, 1 // counter a7 = a7*2. We have to do it only once
	
	// loop for M
m_loop_mmult:
	movi    a10, 0  // reset k loop counter
	mov     a13, a3 // set pointer to the first column
// loop for K
k_loop_mmult:

		addi     a12, a2, -4 // every loop the same start position

		movi    a8, 0
		wsr     a8, acchi
		wsr     a15, acclo // initialize acc with shifted round value

		loopnez a6, .loop_end_mmult // loop for N
		.loop_mmult:
			ldinc       m3, a12
			l16si       a8, a13, 0
			add         a13, a13, a7
			mula.ad.ll  a8, m3
			l16si       a8, a13, 0
			add         a13, a13, a7            
			mula.ad.lh  a8, m3
		.loop_end_mmult:

		rsr     a8, acchi
		rsr     a9, acclo
		src     a8, a8, a9        
		s16i	a8, a4, 0
		addi    a4, a4, 2
		// check and increment for K
		
		addi    a10, a10, 2
		add     a13, a3, a10 // we shift collumn 
		bne     a10, a7, k_loop_mmult

		// Check and increment for M
		add     a2, a2, a14 // move to the next raw
		addi    a5, a5, -1
		bnez.n  a5, m_loop_mmult

	movi.n	a2, 0 // return status ESP_OK
	retw.n

even_N_samples:
//  ----------------  for odd N
	slli    a7, a7, 1 // counter a7 = a7*2. We have to do it only once
	
	// loop for M
m_loop_mmult_even:
	movi    a10, 0  // reset k loop counter
	mov     a13, a3 // set pointer to the first column
// loop for K
k_loop_mmult_even:

		mov     a12, a2     // every loop the same start position

		movi    a8, 0
		wsr     a8,  acchi
		wsr     a15, acclo // initialize acc with shifted round value

		loopnez a6, .loop_end_mmult_even // loop for N
		.loop_mmult_even:
			l16si       a9, a12, 0
			l16si       a8, a13, 0
			addi        a12, a12, 2
			add         a13, a13, a7
			mula.aa.ll  a8, a9
		.loop_end_mmult_even:

		rsr     a8, acchi
		rsr     a9, acclo
		src     a8, a8, a9        
		s16i	a8, a4, 0
		addi    a4, a4, 2
		// check and increment for K
		
		addi    a10, a10, 2
		add     a13, a3, a10 // we shift collumn 
		bne     a10, a7, k_loop_mmult_even

		// Check and increment for M
		add     a2, a2, a14 // move to the next raw
		addi    a5, a5, -1
		bnez.n  a5, m_loop_mmult_even

	movi.n	a2, 0 // return status ESP_OK
	retw.n

// The path where n > 1
vector_mult:
	dspm_mult_s16_m_ae32_vector;


#endif // dspm_mult_s16_ae32_enabled