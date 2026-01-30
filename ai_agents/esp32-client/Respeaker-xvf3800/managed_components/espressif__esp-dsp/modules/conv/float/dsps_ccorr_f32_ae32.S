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

#include "dsps_conv_platform.h"
#if (dsps_ccorr_f32_ae32_enabled == 1)

#include "dsps_conv_f32_m_ae32.S"

// This is dot product function for ESP32 processor.
	.text
	.align  4
	.global dsps_ccorr_f32_ae32
	.type   dsps_ccorr_f32_ae32,@function
// The function implements the C code from dsps_ccorr_f32_ansi:
//esp_err_t dsps_ccorr_f32_ansi(const float *Signal, const int siglen, const float *Kernel, const int kernlen, float *corrout);
//
dsps_ccorr_f32_ae32:
// Signal  	- a2
// siglen  	- a3
// Kernel 	- a4
// kernlen  - a5
// corrout  - a6
//
// a11 - loop length

	entry	a1, 16
	// Array increment for floating point data should be 4
	sub  a10, a3, a5
	bgez    a10, dsps_ccorr_positive
		addi	a10, a2, 0
		addi	a2,  a4, 0
		addi	a4, a10, 0

		addi	a10, a3, 0
		addi	a3,  a5, 0
		addi	a5, a10, 0

dsps_ccorr_positive:
	movi.n	a8, 4
	addi    a11, a5, 0 	// lkern - loop counter 
	movi.n	a14, 0
	addi	a9, a14, 1
	
	movi.n	a7, 4
	movi.n	a8, -4

	mull    a13, a5, a7		// a13 - kernlen*4
	add		a13, a13, a4 	// a13 - Kernel[kernlen]
	addi	a13, a13, -4	// a13 - Kernel[kernlen - 1]
ccorr_loop1:	
		// Clear initial state of the result register
		addi    a10, a13, 0  // a10 - Kernel
		addi	a12, a2, 0   // a12 - Signal
		wfr	    f1, a14 // clear output: convout[n] = 0;
	
		// a12 - sig[0]
		// a10 - kern[n];
		// a9  - n+1
		// a7  - 4,  
		// a8  - -4,  
		conv_f32_ae32 a12, a10, a9, a7, a7, loop1

		addi	a9, a9, 1  // (n+1)++
		addi    a13, a13, -4  // kern[n] - a4--

		ssi		f1, a6, 0 		// Store result from f1 to memory at a6
		addi    a6, a6, 4 		// convout++ - increment output pointer
		
		addi 	a11, a11, -1
	bnez    a11, ccorr_loop1
	
	// a11 - loop counter = siglen - kernlen - 1
	addi	a9,  a2,  4 		// sig[1] - sig[kmin]
	addi 	a13, a5,  0

	// skip loop if 0
	sub    a11, a3, a5 	// a11 - loop counter 
	beqz   a11, skip_ccorr_loop2

ccorr_loop2:	
		
		// Clear initial state of the result register
		addi	a12, a9, 0  // a12 - Signal[kmin]
		addi    a10, a4, 0  // a10 - Kernel
		wfr	    f1, a14 // clear output: convout[n] = 0;
	
		// a12 - sig[kmin]
		// a10 - kern[0];
		// a11  - kernlen
		// a7  - 4,  
		conv_f32_ae32 a12, a10, a13, a7, a7, loop2

		addi	a9, a9, 4  // in1++

		ssi		f1, a6, 0 		// Store result from f1 to memory at a6
		addi    a6, a6, 4 		// convout++ - increment output pointer
		
		addi 	a11, a11, -1
	bnez    a11, ccorr_loop2

	
skip_ccorr_loop2:

	// a9 - the same
	addi	a11, a5, -1
	addi 	a13, a5, -1
ccorr_loop3:	
		
		// Clear initial state of the result register
		addi	a12, a9, 0  	// a12 - Signal[kmin]
		addi    a10, a4, 0  	// a10 - Kernel
		wfr	    f1, a14 		// clear output: convout[n] = 0;
	
		// a12 - sig[kmin]
		// a10 - kern[n - kmin];
		// a11  - length
		// a7  - 4,  
		// a8  - -4,  
		conv_f32_ae32 a12, a10, a11, a7, a7, loop3

		addi	a9, a9, 4  // n++

		ssi		f1, a6, 0 		// Store result from f1 to memory at a6
		addi    a6, a6, 4 		// convout++ - increment output pointer
				
		addi 	a11, a11, -1
	bnez    a11, ccorr_loop3
skip_ccorr_loop3:

	movi.n	a2, 0 // return status ESP_OK
	retw.n

#endif // dsps_ccorr_f32_ae32_enabled