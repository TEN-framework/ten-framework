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
#if (dsps_dotprod_f32_aes3_enabled == 1)

// This is dot product function for ESP32 processor.
	.text
	.align  4
	.global dsps_dotprod_f32_aes3
	.global .dsps_dotprod_f32_ae32_body
	.type   dsps_dotprod_f32_aes3,@function
// The function implements the following C code:
//esp_err_t dsps_dotprod_f32_ae32(const float* src1, const float* src2, float* dest, int len)
//{
//    float acc = 0;
//    for (int i=0 ; i< len ; i++)
//    {
//        acc += src1[i]*src2[i];
//    }
//    *dest = acc;
//    return ESP_OK;
//}

dsps_dotprod_f32_aes3: 
// src1 - a2
// src2 - a3
// dest - a4
// len  - a5

	entry	a1, 16
	// Check length and align
	movi.n	a10, 3
	and a10, a10, a5
	movi.n	a9, 15
	or  a11, a3, a2
	and a11, a9, a11
	or  a10, a10, a11
	beqz  a10, .dsps_dotprod_f32_aes3_body
	// Call Esp32 function
	J 	.dsps_dotprod_f32_ae32_body
	
.dsps_dotprod_f32_aes3_body:
	// Clear initial state of the result register
	movi.n	a9, 0
	wfr	    f0, a9
	wfr	    f1, a9
	wfr	    f2, a9
	wfr	    f3, a9
	// a2 - input1
	// a3 - input2
	// a5 - length

	srli	    a6, a5, 2		// N count
//	lsx	    f0, a2,  a9
	loopnez a6, .loop_mac_end_m_ae32
		EE.LDF.128.IP  f11, f10, f9, f8, a2, 16
		EE.LDF.128.IP  f7, f6, f5, f4, a3, 16
		madd.s	f0, f4, f8		// f0 = X11*Y11
		madd.s	f1, f5, f9		// f1 = X12*Y11
		madd.s	f2, f6, f10		// f2 = X13*Y11
		madd.s	f3, f7, f11		// f3 = X14*Y11
	.loop_mac_end_m_ae32:

	add.s   f0, f0, f1
	add.s   f0, f0, f2
	add.s   f0, f0, f3

	ssi	f0, a4, 0 // Store result from f1 to memory at a4
	
	movi.n	a2, 0 // return status ESP_OK
	retw.n

#endif // dotprode_f32_ae32_enabled
