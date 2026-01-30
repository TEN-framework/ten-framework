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

#include "dsps_fft2r_platform.h"
#if (dsps_fft2r_sc16_ae32_enabled == 1)

// This is matrix multipliction function for ESP32 processor.
	.text
	.align  4
	.global dsps_fft2r_sc16_ae32_
	.type   dsps_fft2r_sc16_ae32_,@function

    .global dsps_fft_w_table_sc16;

//The function implements the following C code:
//esp_err_t dsps_fft2r_sc16_ansi(int16_t *data, int N)
//{
//  esp_err_t result = ESP_OK;
//  uint32_t *w = (uint32_t*)dsps_fft_w_table_sc16;
//  uint32_t *in_data = (uint32_t *)data;
// 	int ie, ia, m;
// 	sc16_t temp;
// 	sc16_t cs;// c - re, s - im
// 	sc16_t m_data;
// 	sc16_t a_data;

// 	ie = 1;
// 	for (int N2 = N / 2; N2 > 0; N2 >>= 1) {
// 		ia = 0;
// 		for (int j = 0; j < ie; j++) {
// 			cs.data = w[j];
// 			//c = w[2 * j];
// 			//s = w[2 * j + 1];
// 			for (int i = 0; i < N2; i++) {
// 				m = ia + N2;
// 				m_data.data = in_data[m];
// 				a_data.data = in_data[ia];
// 				sc16_t m1;
// 				m1.re = xtfixed_bf_1(a_data.re, cs.re, m_data.re, cs.im, m_data.im, 16);//(a_data.re - temp.re + shift_const) >> 1;
// 				m1.im = xtfixed_bf_2(a_data.im, cs.re, m_data.im, cs.im, m_data.re, 16);//(a_data.im - temp.im + shift_const) >> 1;
// 				in_data[m] = m1.data; 
// 				sc16_t m2;
// 				m2.re = xtfixed_bf_3(a_data.re, cs.re, m_data.re, cs.im, m_data.im, 16);//(a_data.re + temp.re + shift_const) >> 1;
// 				m2.im = xtfixed_bf_4(a_data.im, cs.re, m_data.im, cs.im, m_data.re, 16);//(a_data.im + temp.im + shift_const)>>1;
// 				in_data[ia] = m2.data;
// 				ia++;
// 			}
// 			ia += N2;
// 		}
// 		ie <<= 1;
// 	}
// 	return result;
// }

dsps_fft2r_sc16_ae32_: 
//esp_err_t dsps_fft2r_sc16_ansi(float *data, int N, float* dsps_fft_w_table_sc16)

	entry	a1, 16
	// Array increment for floating point data should be 4
// data - a2
// N - a3
// dsps_fft_w_table_sc16 - a4 - for now

// a5 - 1, used to initialize acc
// a6 - k, main loop counter; N2 - for (int N2 = N/2; N2 > 0; N2 >>= 1)
// a7 - ie
// a8 - j
// a9 - test
// a10 - (j)<<2,  or a10 - j<<2
// a11 - ia
// a12 - m
// a13 - ia pointer
// a14 - m pointer
// a15 - used to shift result

    // This instruction are not working. Have to be fixed!!!
    // For now theres no solution...
//    l32r    a4, dsps_fft_w_table_sc16_ae32
    // To use ldinc operation we have to prepare a4:
    addi a4, a4, -4 
    addi a9, a2, -4 // prepare input pointer for ldinc operation
    
    ldinc   m1, a4 // Load [0x7fff j0] value to the m1
    addi    a4, a4, -4 

    // a5 used to load 0x7fff and clear acch/l
    movi.n  a5, 1   // a5 = 1;

    srli a6, a3, 1 // a6 = N2 = N/2
    
    // Load shift register
    movi     a7, 16
    ssr      a7

    movi a7, 1     // a7 - ie

fft2r_l1: 
    movi a8, 0     // a8 - j
    movi a11,0     // a11 = ia = 0;

fft2r_l2:           // loop for j, a8 - j
        slli    a10, a8, 2   // a10 = j<<2 (4 bytes per address) // shift for cs.data = w[j];
        add.n   a10, a10, a4 // a10 - pointer to w tables
		ldinc   m0, a10      // cs.data = w[j];
        // here we have m0 and m1

        loopnez a6, fft2r_l3
            add.n    a12, a11, a6   // a12 = m = ia + N2

            slli     a14, a12, 2    // a14 - pointer for m, m_data.data = in_data[m];
            slli     a13, a11, 2    // a13 - pointer for ia, a_data.data = in_data[ia];
            add.n    a14, a14, a9   // pointers to data arrays
            add.n    a13, a13, a9   // These pointers are -4 from expected values...

            ldinc      m2, a14        // m_data, a14 += 4; The pointers ready to store data
            mul.da.ll  m1, a5         // acc = 0x7fff*1  
            ldinc      m3, a13        // ai_data a13 += 4;
            // re - l, im - h
            muls.dd.ll m0, m2         // acc -= cs.re*m_data.re
            mula.dd.ll m1, m3         // acc += 0x7fff*a_data.re
            muls.dd.hh m0, m2         // acc -= cs.im*m_data.im
            // result in acclo        in_data[m].re
            rsr      a15, acclo
            mul.da.ll  m1, a5         // acc = 0x7fff*1  
            sra      a15, a15
            muls.dd.lh m0, m2         // acc -= cs.re*m_data.im
            s16i     a15, a14, 0
            mula.dd.lh m1, m3         // acc += 0x7fff*a_data.im
            mula.dd.hl m0, m2         // acc += cs.im*m_data.re
            // result in acclo        in_data[m].im
            rsr      a15, acclo
            mul.da.ll  m1, a5         // acc = 0x7fff*1  
            sra      a15, a15
            mula.dd.ll m0, m2         // acc += cs.re*m_data.re
            s16i     a15, a14, 2
            mula.dd.ll m1, m3         // acc += 0x7fff*a_data.re
            mula.dd.hh m0, m2         // acc += cs.im*m_data.im
            // result in acclo        // in_data[ia].re
            rsr      a15, acclo
            mul.da.ll  m1, a5         // acc = 0x7fff*1  

            sra      a15, a15
            mula.dd.lh m0, m2         // acc += cs.re*m_data.im
            s16i     a15, a13, 0

            mula.dd.lh m1, m3         // acc += 0x7fff*a_data.im
            muls.dd.hl m0, m2         // acc -= cs.im*m_data.re
            // result in acclo        // in_data[ia].im
            rsr      a15, acclo

            sra      a15, a15
            s16i     a15, a13, 2

            // Here we have m0 - w, m2 - m_data, m3 - ai_data, 
            addi     a11, a11, 1// ia++
fft2r_l3:
        add     a11, a11, a6

        addi    a8, a8, 1     // j++
        BNE     a8, a7, fft2r_l2 // 
    slli    a7, a7, 1  // ie = ie<<1
// main loop: for (int k = N/2; k > 0; k >>= 1)
    srli    a6, a6, 1  // a6 = a6>>1
    BNEZ    a6, fft2r_l1// Jump if > 0

	movi.n	a2, 0 // return status ESP_OK
	retw.n

#endif // dsps_fft2r_sc16_ae32_enabled