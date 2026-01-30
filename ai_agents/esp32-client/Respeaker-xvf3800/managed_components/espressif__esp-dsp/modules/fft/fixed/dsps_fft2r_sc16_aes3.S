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
#if (dsps_fft2r_sc16_aes3_enabled == 1)

// This is matrix multipliction function for ESP32 processor.
	.text
	.align	4
	.literal_position
	.literal	.LC0_5_39, dsps_fft2r_sc16_initialized
	.literal	.LC1_5_40, 32767
	.literal	.LC2_5_41, 458756
	.literal	.LC3_5_42, 458753

	# Program Unit: dsps_fft2r_sc16_aes3_
	.type	dsps_fft2r_sc16_aes3_, @function
	.align	 4
	.global	dsps_fft2r_sc16_aes3_
dsps_fft2r_sc16_aes3_:	# 0x4
	# q3 = 16
	# temp_round = 0
	# temp_round_ptr = 4
.LBB1_dsps_fft2r_sc16_aes3_:	# 0x4
	.frequency 1.000 0.000
	entry	a1,64                   	#  
	mov.n	a10,a3                  	# [0]  
	.type	dsp_is_power_of_two, @function
	call8	dsp_is_power_of_two     	# [1]  dsp_is_power_of_two

	beqz.n	a10,.LBB4_dsps_fft2r_sc16_aes3_ 	# [0]  

	l32r	a8,.LC0_5_39             	# [0]  
	l8ui	a8,a8,0                  	# [2]  id:207 dsps_fft2r_sc16_initialized+0x0
	beqz.n	a8,.LBB6_dsps_fft2r_sc16_aes3_ 	# [4]  

	mov.n	a9,a1                   	# [0]  
	l32r	a8,.LC1_5_40             	# [1]  
	addi.n	a6,a3,1                	# [2]  
	movi.n	a10,16                 	# [3]  
	wsr.sar	a10                   	# [4]  
	movgez	a6,a3,a3               	# [5]  
	s16i	a8,a1,0                  	# [6]  temp_round
	ee.vldbc.16.ip	q6,a9,0        	# [7]  id:209
	srai	a6,a6,1                  	# [8]  
	bltui	a6,3,.Lt_0_13826        	# [9]  

	movi.n	a5,1                   	# [0]  

.Lt_0_9218:	# 0x33
	mov.n	a13,a4                  	# [0]  
	mov.n	a9,a2                   	# [1]  
	beqz.n	a5,.Lt_0_9474          	# [2]  

	srli	a3,a6,2                  	# [0]  
	movi.n	a14,0                  	# [1]  
	slli	a15,a6,2                 	# [2]  
	add.n	a8,a15,a9               	# [3]  

.Lt_0_9986:	# 0x43
	ee.vldbc.32.ip	q5,a13,4       	# [0]  id:215
	loopnez	a3,.LBB54_dsps_fft2r_sc16_aes3_ 	# [1]  

.LBB52_dsps_fft2r_sc16_aes3_:	# 0x49
	ee.vld.128.ip	q0,a8,0         	# [0*II+0]  id:217
	ee.vld.128.ip	q2,a9,0         	# [0*II+1]  id:216
	ee.cmul.s16	q1,q5,q0,2        	# [0*II+2]  
	ee.vmul.s16	q2,q2,q6          	# [0*II+3]  
	ee.cmul.s16	q1,q5,q0,3        	# [0*II+4]  
	ee.vsubs.s16	q3,q2,q1         	# [0*II+6]  
	ee.vadds.s16.st.incp	q3,a8,q3,q2,q1 	# [0*II+7]  id:221
	ee.vst.128.ip	q3,a9,16        	# [0*II+8]  id:222

.LBB54_dsps_fft2r_sc16_aes3_:	# 0x62
	addi.n	a14,a14,1              	# [0]  
	add.n	a9,a9,a15               	# [1]  
	add.n	a8,a15,a9               	# [2]  
	bne	a14,a5,.Lt_0_9986         	# [3]  

.Lt_0_9474:	# 0x6b
	slli	a5,a5,1                  	# [0]  
	srli	a6,a6,1                  	# [1]  
	bgeui	a6,3,.Lt_0_9218         	# [2]  

	srli	a10,a5,1                 	# [0]  
	beqz.n	a10,.Lt_0_14594        	# [1]  

	mov.n	a9,a4                   	# [0]  
	mv.qr	q4,q1                   	# [1]  
	mov.n	a8,a2                   	# [2]  
	mv.qr	q1,q0                   	# [3]  
	mv.qr	q0,q2                   	# [4]  
	loopnez	a10,.LBB76_dsps_fft2r_sc16_aes3_ 	# [5]  

.LBB74_dsps_fft2r_sc16_aes3_:	# 0x89
	ee.vld.l.64.ip	q0,a8,8        	# [0*II+0]  id:225
	ee.vldbc.32.ip	q2,a9,4        	# [0*II+1]  id:223
	ee.vld.l.64.ip	q1,a8,8        	# [0*II+2]  id:226
	ee.vldbc.32.ip	q3,a9,4        	# [0*II+3]  id:224
	ee.vld.h.64.ip	q0,a8,8        	# [0*II+4]  id:227
	ee.vunzip.32	q2,q3            	# [0*II+5]  
	ee.vld.h.64.ip	q1,a8,-24      	# [0*II+6]  id:228
	ee.vmul.s16	q0,q0,q6          	# [0*II+7]  
	ee.cmul.s16	q4,q2,q1,2        	# [0*II+8]  
	ee.cmul.s16	q4,q2,q1,3        	# [0*II+9]  
	ee.vadds.s16	q2,q0,q4         	# [0*II+11]  
	ee.vsubs.s16	q3,q0,q4         	# [0*II+12]  
	ee.vst.l.64.ip	q2,a8,8        	# [0*II+13]  id:232
	ee.vst.l.64.ip	q3,a8,8        	# [0*II+14]  id:233
	ee.vst.h.64.ip	q2,a8,8        	# [0*II+15]  id:234
	ee.vst.h.64.ip	q3,a8,8        	# [0*II+16]  id:235

.LBB76_dsps_fft2r_sc16_aes3_:	# 0xb9
	.frequency 0.608 0.000
	st.qr	q4,a1,16                	# [0]  q3

.Lt_0_11778:	# 0xbc
	ld.qr	q3,a1,16                	# [0]  q3
	slli	a10,a5,1                 	# [1]  
	srli	a10,a10,2                	# [2]  
	loopnez	a10,.LBB98_dsps_fft2r_sc16_aes3_ 	# [3]  

.LBB96_dsps_fft2r_sc16_aes3_:	# 0xc8
	ee.vld.128.ip	q0,a2,16        	# [0*II+0]  id:237
	ee.vld.128.ip	q1,a2,-16       	# [0*II+1]  id:238
	ee.vld.128.ip	q2,a4,16        	# [0*II+2]  id:236
	ee.vunzip.32	q0,q1            	# [0*II+3]  
	ee.cmul.s16	q3,q2,q1,2        	# [0*II+4]  
	ee.vmul.s16	q0,q0,q6          	# [0*II+5]  
	ee.cmul.s16	q3,q2,q1,3        	# [0*II+6]  
	ee.vsubs.s16	q1,q0,q3         	# [0*II+8]  
	ee.vadds.s16	q0,q0,q3         	# [0*II+9]  
	ee.vzip.32	q0,q1              	# [0*II+10]  
	ee.vst.128.ip	q0,a2,16        	# [0*II+11]  id:242
	ee.vst.128.ip	q1,a2,16        	# [0*II+12]  id:243

.LBB98_dsps_fft2r_sc16_aes3_:	# 0xec
	movi.n	a2,0                   	# [0]  
	retw.n                        	# [1]  

.Lt_0_13826:	# 0xf0
	movi.n	a5,1                   	# [0]  
	j	.Lt_0_11778                 	# [1]  

.LBB6_dsps_fft2r_sc16_aes3_:	# 0xf5
	l32r	a2,.LC2_5_41             	# [0]  
	retw.n                        	# [1]  

.LBB4_dsps_fft2r_sc16_aes3_:	# 0xfa
	l32r	a2,.LC3_5_42             	# [0]  
	retw.n                        	# [1]  

.Lt_0_14594:	# 0xff
	st.qr	q1,a1,16                	# [0]  q3
	j	.Lt_0_11778                 	# [1]  


#endif // dsps_fft2r_sc16_ae32_enabled