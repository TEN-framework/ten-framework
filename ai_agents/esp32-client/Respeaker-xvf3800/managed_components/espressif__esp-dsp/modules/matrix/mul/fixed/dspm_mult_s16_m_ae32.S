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


.macro dspm_mult_s16_ae32_MxNxN
// A - a2
// B - a3
// C - a4
// m - a5
// n - a6
// k - a7
// shift - stack (a8) 

	movi  a10, 4 // load 4 as a constant
	// Check if n >=4 then acceleration is possible and 
	blt   a6, a10, do_dotproduct 
	// Here we make operations one by one...


	movi.n	a2, 0 // return status ESP_OK
	retw.n

do_dotproduct:

		mov  a12, a2
		mov  a13, a3
		
		srli a9, a6, 2  // a9 - count/4 - 1
		addi a9, a9, -1

		movi.n	a10, 0 // load 0 to the a10 to increment second array
		dotprod_s16_ae32_full a12, a13, a9, a10, a6

		/* Get accumulator */
		ssr a6
		rsr a2, acchi
		rsr a3, acclo
		src a2, a2, a3
		
		s16i	a2, a4, 0
		movi.n	a2, 0


	movi.n	a2, 0 // return status ESP_OK
	retw.n

.endm // dspm_mult_s16_ae32_MxNxN