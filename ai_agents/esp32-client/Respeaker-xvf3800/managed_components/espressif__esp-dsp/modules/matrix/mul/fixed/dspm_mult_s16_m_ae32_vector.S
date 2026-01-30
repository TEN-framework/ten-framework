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


.macro dspm_mult_s16_m_ae32_vector
// m - a5 - any > 0
// n - a6 - 1,2,3, any
// k - a7 - 1, any


	// Define path for n < 4
	movi a7, 4
	blt  a6, a7, small_process_loop // jump for n < 4

	srli a7, a6,  2
	addi a7, a7, -1
 

mmultv_loop1:
	wsr a8, acchi
	wsr a15, acclo // initialize acc with shifted round value

	// Clear initial state of the result register
	// a2 - A
	// a3 - B
	// a4 - C
	// a6 - n
	// a7 - n/4 - 1
	// a8 - 0
	// a15- 0x7fff>>shift

		mov      a12, a2 // load A
		mov      a13, a3 // Load B

		dotprod_s16_ae32_full a12, a13, a7, a6

	// check loop 1
		/* Get accumulator */
		rsr a12, acchi
		rsr a13, acclo
		src a12, a12, a13
		
		s16i	a12, a4, 0
		addi    a4, a4, 2

		add.n   a2, a2, a14 // Increment A, A = A[i*n]
		addi    a9, a9, 1 // Increment loop1 counter    
	blt     a9, a5, mmultv_loop1


	movi.n	a2, 0 // return status ESP_OK
	retw.n

small_process_loop:

	wsr a8, acchi
	wsr a15, acclo // initialize acc with shifted round value

	mov      a12, a2 // load A
	mov      a13, a3 // Load B

	addi  a12, a12, -4 // To arrange fist pointer
	addi  a13, a13, -4 // To arrange fist pointer

		bbci  a6, 1, .mod2chk_short
		ldinc m0, a12
		ldinc m2, a13
		mula.dd.hh m0, m2
		mula.dd.ll m0, m2
	.mod2chk_short:
		bbci  a6, 0, .mod1chk_short
		ldinc m0, a12
		ldinc m2, a13
		mula.dd.ll m0, m2
	.mod1chk_short:

	// check loop 1
		/* Get accumulator */
		rsr a12, acchi
		rsr a13, acclo
		src a12, a12, a13
		
		s16i	a12, a4, 0
		addi     a4, a4, 2

		add.n   a2, a2, a14 // Increment A, A = A[i*n]
		addi    a9, a9, 1 // Increment loop1 counter    
	blt     a9, a5, small_process_loop

	movi.n	a2, 0 // return status ESP_OK
	retw.n


.endm // dspm_mult_s16_m_ae32_vector