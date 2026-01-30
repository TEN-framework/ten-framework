/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

.macro fir_s16_ae32_mul x1, x2, count, ID
// This macro calculates fixed point dot product for ((count + 1)*4) int16 samples
// x1 - input array1 register (samples)
// x2 - input array2 register (coefficients) the array is inverted and is being decremented
// count - counter register (for example a7)
// count - (samples_count / 4) - 1
// acc += x1[i + 0]*x2[N - i - 1] + x1[i + 1]*x2[N - i - 2] + x1[i + 2]*x2[N - i - 3] + x1[i + 3]*x2[N - i - 4]; i: 0..count
// acchi, and acclo have to be initialized before
// Result - acchi || acclo
// Modifies: 
// m0, m1, m2, m3
// acchi || acclo - must be loaded before (for example 0x3fff to acclo). 

		/*
		 * Data schedule. Each line represents instruction and columns represent
		 * register contents. Last column (MUL) shows the multiplication which
		 * takes place. Values loaded in the given cycle are shown in square brackets.
		 *
		 *  m0     m1         m2              m3          MUL
		 * -----------------  pre-load  --------------------------
		 *[x0 x1]								                  (no MULs in the first 3 instructions)
		 * x0 x1        [y(N-1) y(N-2)]
		 * x0 x1 [x2 x3] y(N-1) y(N-2)
		 * x0 x1  x2 x3  y(N-1) y(N-2) [y(N-3) y(N-4)] x0*y(N-1)
		 * --------------------   loop  ------------------------	 (the following 4 instructions are
		 *[x4 x5] x2 x3  y(N-1) y(N-2)  y(N-3) y(N-4)  x1*y(N-2)     repeated as much as needed)
		 * x4 x5  x2 x3 [y(N-5) y(M-6)] y(N-3) y(N-4)  x2*y(N-3)
		 * x4 x5 [x6 x7] y(N-5) y(M-6)  y(N-3) y(N-4)  x3*y(N-4)
		 * x4 x5  x6 x7  y(N-5) y(M-6) [y(N-7) y(M-8)] x4*y(N-5)
		 * -------------------  finalize  ----------------------
		 * x4 x5  x6 x7  y(N-5) y(M-6)  y(N-7) y(M-8)  x5*y(N-6)	(nothing is load)
		 * x4 x5  x6 x7  y(N-5) y(M-6)  y(N-7) y(M-8)  x6*y(N-7)
		 * x4 x5  x6 x7  y(N-5) y(M-6)  y(N-7) y(M-8)  x7*y(N-8)
		 */

		ldinc m0, \x1
		lddec m2, \x2
		ldinc m1, \x1
	
		mula.dd.lh.lddec m3, \x2, m0, m2
		loopnez \count, .loop_end_\ID
		.loop_\ID:
			mula.dd.hl.ldinc m0, \x1, m0, m2
			mula.dd.lh.lddec m2, \x2, m1, m3
			mula.dd.hl.ldinc m1, \x1, m1, m3
			mula.dd.lh.lddec m3, \x2, m0, m2
		.loop_end_\ID:
	
		mula.dd.hl m0, m2
		mula.dd.lh m1, m3
		mula.dd.hl m1, m3

.endm // fir_s16_ae32_mul

.macro fir_s16_ae32_full x1, x2, count, full_count, ID
// This macro calculates fixed point dot product for ((count + 1)*4) int16 samples
// x1 - input array1 register (for example a2)
// x2 - input array2 register (for example a3)
// count - counter register (for example a7)
// count -   samples_count / 4 - 1
// full_count - samples_count
// acc += x1[i + 0]*x2[N - i - 1] + x1[i + 1]*x2[N - i - 2] + x1[i + 2]*x2[N - i - 3] + x1[i + 3]*x2[N - i - 4]; i: 0..count
// acchi, and acclo have to be initialized before
// Result - acchi || acclo
// Modifies: 
// m0, m1, m2, m3
// acchi || acclo - must be loaded before (for example 0x3fff to acclo). 

		// the main mac16 multiplication loop is skipped for cases with less than 4 samples
		blti \full_count, 4, .less_than_4_operands_\ID
        	fir_s16_ae32_mul \x1, \x2, \count, \ID

		.less_than_4_operands_\ID:

        bbci  \full_count, 1, .mod2chk_\ID
		    ldinc m0, \x1
		    lddec m2, \x2
		    mula.dd.hl m0, m2
		    mula.dd.lh m0, m2
	    .mod2chk_\ID:

		bbci  \full_count, 0, .mod1chk_\ID
		    ldinc m0, \x1
		    lddec m2, \x2
		    mula.dd.lh m0, m2
	    .mod1chk_\ID:

.endm // fir_s16_ae32_full
