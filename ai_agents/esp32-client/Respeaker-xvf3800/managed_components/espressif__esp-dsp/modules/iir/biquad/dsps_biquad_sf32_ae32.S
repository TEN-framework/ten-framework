// Copyright 2018-2025 Espressif Systems (Shanghai) PTE LTD
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

#include "dsps_biquad_platform.h"
#if (dsps_biquad_f32_ae32_enabled == 1)

// This is bi quad filter form II for ESP32 processor.
	.text
	.align  4
	.global dsps_biquad_sf32_ae32
	.type   dsps_biquad_sf32_ae32,@function
// The function implements the following C code:
//esp_err_t dsps_biquad_f32_ae32(const float* input, float* output, int len, float* coef, float* w)
//  {
//    for (int i=0 ; i< len ; i++)
//    {
//        float d0 = input[i] - coef[3]*w[0] - coef[4]*w[1]; (input[i] - a[1]*w[0] - a[2]*w[1];)
//        output[i] = coef[0]*d0 +  coef[1]*w[0] + coef[2]*w[1];
//        w[1] = w[0];
//        w[0] = d0;
//    }
//    return ESP_OK;
//  }

dsps_biquad_sf32_ae32: 
// input    - a2
// output   - a3
// len  - a4
// coeffs  - a5
// w- a6

// f0 - b0
// f1 - b1
// f2 - b2
// f3 - a1
// f4 - a2

// f5 - w0
// f6 - w1

	entry	a1, 16
	// Array increment for floating point data should be 4
	lsi   f0, a5, 0
	lsi   f1, a5, 4
	lsi   f2, a5, 8
	lsi   f3, a5, 12
	lsi   f4, a5, 16

	
	neg.s  f5, f3   // -a[1]
	neg.s  f6, f4   // -a[2]

	lsi   f7,  a6, 0 // w[0]
	lsi   f8,  a6, 4 // w[1]
	lsi   f11, a6, 8 // w[0]
	lsi   f12, a6, 12 // w[1]
	
	//addi    a3, a3, -4       // i-- // preset a3
	lsip     f9, a2, 4        // f9 = x[i]
	loopnez a4, .loop_bq_end_m_ae32
		madd.s  f9, f7, f5   // f9 += -a1*w0
		mul.s   f10, f1, f7  // f10 = b1*w0
		madd.s  f9, f8, f6   // f9 += -a2*w1
		madd.s  f10, f9, f0  // f10 += b0*d0
		madd.s  f10, f2, f8  // f10+= b2*w1, f10 - result
		mov.s   f8, f7       // w1 = w0
		mov.s   f7, f9       // w0 = d0
		lsip    f9, a2,  4   // f9 = x[i]
		ssip	f10, a3, 4   // y[i] = result


		madd.s  f9, f11, f5   // f9 += -a1*w0
		mul.s   f10, f1, f11  // f10 = b1*w0
		madd.s  f9, f12, f6   // f9 += -a2*w1
		madd.s  f10, f9, f0  // f10 += b0*d0
		madd.s  f10, f2, f12  // f10+= b2*w1, f10 - result
		mov.s   f12, f11       // w1 = w0
		mov.s   f11, f9       // w0 = d0
		lsip    f9, a2,  4   // f9 = x[i]
		ssip	f10, a3, 4   // y[i] = result
.loop_bq_end_m_ae32:
		// Store delay line
		ssi     f7, a6, 0
		ssi     f8, a6, 4
		ssi     f11, a6, 8
		ssi     f12, a6, 12

	movi.n	a2, 0 // return status ESP_OK
	retw.n

#endif // dsps_biquad_f32_ae32_enabled