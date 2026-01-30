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


.macro dotprode_f32_ae32 x1 x2 count step1  step2
// This macro calculates floating point dot product for count float samples
// x1, x2 - input arrays
// count - amount of samples
// step1,step2 - A register for array step. (should be divided by 4)
// f1 - contains initial value 
//
// result in f1
// 
// Macros body:
// f1 += x1[i*step1]*x2[i*step2]; i: 0..counter-1
// affected: f0, f1, f2
// Example: dotprod_f32_ae32 a2 a3 a5 a8 a9
// a8 == 4, step is 4 bytes
// a5 == 32, length of array is 32
// 
	lsi	    f0, \x2, 0
	sub	    \x1, \x1, \step1 // To compensate first increment
	loopnez \count, .loop_mace_end_m_ae32
		add.n	\x1, \x1, \step1
		lsi	    f2, \x1, 0
		madd.s	f1, f2, f0
		add.n	\x2, \x2, \step2
		lsi	     f0, \x2, 0
	.loop_mace_end_m_ae32:
.endm
