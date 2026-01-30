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

#include "dsps_dotprod_platform.h"
#if (dsps_dotprod_s16_ae32_enabled == 1)
#include "dsps_dotprod_s16_m_ae32.S"
#include "dsp_err_codes.h"

	.text
	.align  4
	.global dsps_dotprod_s16_ae32
	.type   dsps_dotprod_s16_ae32,@function


//esp_err_t dsps_dotprod_s16_ae32(const int16_t* src1, const int16_t* src2, int16_t* dest, int len, int8_t shift);
dsps_dotprod_s16_ae32: 
// src1 - a2
// src2 - a3
// dest - a4
// len  - a5
// shift - a6

	entry	a1, 16

	// Check minimum length
	movi a8, 4
	blt a5, a8, dsps_dotprod_s16_ae32_error
	
	// Clear accumulator
	movi a8, 0
	wsr a8, acchi
		
	// Prepare and load round value
	movi a8, 0x7fff
	ssr a6
	srl a8, a8
	wsr a8, acclo // initialize acc with shifted round value

	// Compensate for pre-increment 
	// Right shift to 16 bits
	// RS = -shift + 15
	neg  a6, a6 
	addi a6, a6, 15
	
	/* number of loop iterations (see below):
	 * a7 = count / 4 - 1
	 */
	
	srli a7, a5, 2
	addi a7, a7, -1

	movi.n	a10, 0 // load 0 to the a10 to increment second array

	dotprod_s16_ae32_full a2, a3, a7, a5

	/* Get accumulator */
	ssr a6
	rsr a2, acchi
	rsr a3, acclo
	src a2, a2, a3
		
	s16i	a2, a4, 0
	movi.n	a2, 0
	retw.n
dsps_dotprod_s16_ae32_error:
	movi.n	a2, ESP_ERR_DSP_INVALID_LENGTH
	retw.n

#endif // dsps_dotprod_s16_ae32_enabled