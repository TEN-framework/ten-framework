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


.macro dotprod_s16_ae32 x1, x2, count
// This macro calculates fixed point dot product for ((count + 1)*4) int16 samples
// x1 - input array1 register (for example a2)
// x2 - input array2 register (for example a3)
// count - counter register (for example a7)
// count -   samples_count / 4 - 1
// acc += x1[i + 0]*x2[i + 0] + x1[i + 1]*x2[i + 1] + x1[i + 2]*x2[i + 2] + x1[i + 3]*x2[i + 3]; i: 0..count
// acchi, and acclo have to be initialize before
// Result - acchi || acclo
// Modifies: 
// m0, m1, m2, m3
// acchi || acclo - must be loaded before (for example 0x3fff to acclo). 

		/*
		 * Data schedule. Each line represents instruction, columns represent
		 * register contents. Last column (MUL) shows the multiplication which
		 * takes place. Values loaded in the given cycle are shown in square brackets.
		 *
		 *  m0     m1     m2     m3     MUL
		 * ---------  pre-load  ------------
		 *[x0 x1]								(no MULs in the first 3 instructions)
		 * x0 x1        [y0 y1]
		 * x0 x1 [x2 x3] y0 y1
		 * x0 x1  x2 x3  y0 y1 [y2 y3] x0*y0
		 * ----------   loop  --------------	(the following 4 instructions are
		 *[x4 x5] x2 x3  y0 y1  y2 y3  x1*y1     repeated as much as needed)
		 * x4 x5  x2 x3 [y4 y5] y2 y3  x2*y2
		 * x4 x5 [x6 x7] y4 y5  y2 y3  x3*y3
		 * x4 x5  x6 x7  y4 y5 [y6 y7] x4*y4
		 * ---------  finalize  ------------
		 * x4 x5  x6 x7  y4 y5  y6 y7  x5*y5	(nothing is load)
		 * x4 x5  x6 x7  y4 y5  y6 y7  x6*y6
		 * x4 x5  x6 x7  y4 y5  y6 y7  x7*y7
		 */

		addi  \x1, \x1, -4 // To arrange fist pointer
		addi  \x2, \x2, -4 // To arrange fist pointer
		//lddec m0, \x1 
		//lddec m2, \x2 // To arrange fist pointer

		ldinc m0, \x1
		ldinc m2, \x2
		ldinc m1, \x1
	
		mula.dd.ll.ldinc m3, \x2, m0, m2
		loopnez \count, .loop_end
		.loop:
			mula.dd.hh.ldinc m0, \x1, m0, m2
			mula.dd.ll.ldinc m2, \x2, m1, m3
			mula.dd.hh.ldinc m1, \x1, m1, m3
			mula.dd.ll.ldinc m3, \x2, m0, m2
		.loop_end:
	
		mula.dd.hh m0, m2
		mula.dd.ll m1, m3
		mula.dd.hh m1, m3

.endm // dotprod_s16_ae32


.macro dotprod_s16_ae32_full x1, x2, count, full_count
// This macro calculates fixed point dot product for ((count + 1)*4) int16 samples
// x1 - input array1 register (for example a2)
// x2 - input array2 register (for example a3)
// count - counter register (for example a7)
// count -   samples_count / 4 - 1
// full_count - samples_count
// acc += x1[i + 0]*x2[i + 0] + x1[i + 1]*x2[i + 1] + x1[i + 2]*x2[i + 2] + x1[i + 3]*x2[i + 3]; i: 0..count
// acchi, and acclo have to be initialize before
// Result - acchi || acclo
// Modifies: 
// m0, m1, m2, m3
// acchi || acclo - must be loaded before (for example 0x3fff to acclo). 

		dotprod_s16_ae32 \x1, \x2, \count

		bbci  \full_count, 1, .mod2chk
		ldinc m0, \x1
		ldinc m2, \x2
		mula.dd.hh m0, m2
		mula.dd.ll m0, m2
	.mod2chk:
		bbci  \full_count, 0, .mod1chk
		ldinc m0, \x1
		ldinc m2, \x2
		mula.dd.ll m0, m2
	.mod1chk:

.endm // dotprod_s16_ae32_full