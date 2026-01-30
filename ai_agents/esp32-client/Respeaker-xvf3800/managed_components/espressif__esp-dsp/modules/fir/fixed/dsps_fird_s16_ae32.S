/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsps_fir_platform.h"
#if (dsps_fird_s16_ae32_enabled == 1)

#include "dsps_fir_s16_m_ae32.S"
 
// This is FIR filter for ESP32 processor.
	.text
	.align  4
	.global dsps_fird_s16_ae32
	.type   dsps_fird_s16_ae32,@function
// The function implements the following C code:
//int32_t dsps_fird_s16_ansi(fir_s16_t *fir, const int16_t *input, int16_t *output, int32_t len)


dsps_fird_s16_ae32: 
// Input params					Variables
//
// fir      - a2				N			- a6
// input    - a3				pos			- a7
// output   - a4				rounding_lo - a8
// len      - a5				d_pos		- a9
//								&coeffs[N]	- a10
//								delay		- a11
//								decim		- a12
//								rounding_hi - a13	
//								final_shift - a14 (shift)

	entry    a1,    32				

	l16si    a7,    a2,    10					// a7  - pos
	l16si	 a6,    a2,    8					// a6  - N
	l32i	 a10,   a2,    0					// a10 - coeffs
	addx2	 a10,   a6,    a10					// a10 - coeffs[N+1]
	addi     a10,   a10,  -4					// a10 - coeffs[N]
	s32i	 a10,   a1,    0					// save pointer to a1
	l32i     a11,   a2,    4         			// a11 - delay line
	l16si    a12,   a2,    12        			// a12 - decimation		
	l16si    a9,    a2,    14        			// a9  - d_pos			
	l16si    a14,   a2,    16					// a14 - shift		    
		
	// prepare rounding value		
	l32i     a15,   a2,    20                   // get address of rounding array to a15
	l32i 	 a8,    a15,    0					// a8 =  lower 32 bits of the rounding value (acclo)
	l32i     a13,   a15,    4					// a13 = higher 8 bits of the rounding value (acchi), offset 4 (32 bits)
		
	// prepare final_shift value					
	addi 	 a14,   a14,  -15					// shift - 15
	abs		 a15, 	a14
	blti	 a15,    32,   _shift_lower_than_32_init		// check if lower than 32

												// greater than 32 could only be negative shift ((-40 to +40) - 15) -> -55 to +25 
	addi	 a14, 	a14,   32					// if greater than 32, add 32 (SRC is not defined for SAR greater than 32)
	_shift_lower_than_32_init:

	bltz	 a14,   _shift_negative_init		// branch if lower than zero (not including zero)
	beqz	 a14,	_shift_negative_init		// branch if equal to zero (add zero to the previous statement)
	ssl		 a14								// if positive, set SAR register to left shift value (SAR = 32 - shift)
	
	j _end_of_shift_init

	_shift_negative_init:						// negative shift
	abs		 a14,   a14							// absolute value
	ssr		 a14								// SAR = -shift
	// final_shift is saved to SAR register, SAR is not being changed during the execution

	_end_of_shift_init:	
	l16si    a14,   a2,    16					// a14 - load shift value
	addi 	 a14,   a14,  -15					// shift - 15
	
	s32i     a5,    a1,    4             		// save len to a1, used as the return value


 	// first delay line load (decim - d_pos) when d_pos is not 0
	beqz	a9,     _fird_loop_len
	sub		a15,    a12, a9						// a15 = decim - d_pos

	loopnez a15,  ._loop_d_pos
		 
		blt    a7,   a6,   reset_fir_pos_d_pos	// branch if fir->pos >= fir->N
			movi.n   a7,   0					// fir->pos = 0
			l32i     a11,  a2,   4      		// reset delay line to the beginning
		reset_fir_pos_d_pos:		

		l16si	 a15,  a3,   0					// load 16 bits from input (a3) to a15
		addi 	 a7,   a7,   1					// fir->pos++
		s16i	 a15,  a11,  0					// save 16 bits from a15 to delay line (a11)
		addi	 a3,   a3,   2					// increment input pointer
		addi	 a11, a11,   2					// increment delay line pointer
	._loop_d_pos:		

	j .fill_delay_line							// skip the first iteration of the delay line filling routine

	// outer loop
	_fird_loop_len:

		loopnez a12, .fill_delay_line

			blt a7, a6, reset_fir_pos			// branch if fir->pos >= fir->N
				movi.n   a7,   0				// fir->pos = 0
				l32i	 a11,  a2,  4       	// reset delay line to the beginning
			reset_fir_pos:		

			l16si	 a15,  a3,    0				// load 16 bits from input (a3) to a15
			addi 	 a7,   a7,    1				// fir->pos++
			s16i	 a15,  a11,   0				// save 16 bits from a15 to delay line (a11)
			addi	 a3,   a3,    2				// increment input pointer
			addi	 a11,  a11,   2				// increment delay line pointer
		.fill_delay_line:

		// prepare MAC unit
		wsr	    a8,   acclo						// acclo = a8
		wsr		a13,  acchi						// acchi = a13

		addi    a11,  a11,  -4 					// preset delay line pointer, samples (array is being incremented)
		sub     a9,   a6,    a7   				// a9 = full_count = fir->N - fir->pos

		// (Count / 4) - 1		
		srli    a15,  a9,    2					// a15 = count = full_count /4
		addi    a10,  a10,   4 					// preset coeffs pointer, samples (array is being decremented)
		addi    a15,  a15,  -1					// count - 1

		// x1, x2, count, full_count, ID
		fir_s16_ae32_full a11, a10, a15, a9, __LINE__

		l32i	a10,  a2,    0         			// load coeffs
		l32i 	a11,  a2,    4					// reset delay line to the beginning
		addx2	a10,  a7,    a10				// move coeffs pointer to the end
		
		srli 	a15,  a7,    2					// a15 = count = full_count (fir->pos) / 4
		addi    a11,  a11,  -4 					// preset delay line pointer, samples (array is being incremented)
		addi    a15,  a15,  -1					// count - 1

		// x1, x2, count, full_count, ID
		fir_s16_ae32_full a11, a10, a15, a7, __LINE__

		// SAR already set from the beginning to final_shift value
		abs		a15,  a14						// absolute value of shift
		l32i	a10,  a1, 	 0					// reset coefficient pointer
		blti    a15,  32,   _shift_lower_than_32
		rsr 	a9,   acchi						// get only higher 8 bits of the acc register
		movi.n	a15,  0xFF						// higher 8 bits mask
		and		a9,   a9,  a15					// apply mask
		srl		a15,  a9	
		j 		_shift_set

		_shift_lower_than_32:
		rsr 	a9,   acchi						// get higher 8 bits of the acc register
		movi.n	a11,  0xFF						// higher 8 bits mask
		rsr 	a15,  acclo						// get lower 32 bits of the acc register
		and		a9,   a9,  a11					// apply mask


		bltz	a14,  _shift_negative 			// branch if lower than zero (if negative)
		beqz	a14,  _shift_negative
		src		a15,  a15,  a9					// funnel shift left
		j 		_shift_set

		_shift_negative:						// negative shift
		src		a15,  a9,  a15					// funnel shift right

		_shift_set:
		
		l32i    a11,  a2,    4					// Load initial position of the delay line
		s16i	a15,  a4, 	 0					// save the shifted value to the output array (a4)
		addi 	a5,   a5,   -1					// len--
		addi	a4,   a4, 	 2					// increase pointer of the output array		
		addx2	a11,  a7,    a11				// p_delay[fir->pos] - (two times the fir->pos)	

		// counter				
		bnez    a5,   _fird_loop_len			// break if a5 == 0

	l32i.n     a2,  a1,  4                     	// load return value to a2
	retw.n

#endif // dsps_fird_s16_ae32_enabled
