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

#include "dspm_mult_platform.h"
#if (dspm_mult_s16_aes3_enabled == 1)
#include "dsps_dotprod_s16_m_ae32.S"
#include "dspm_mult_s16_m_ae32_vector.S"

//esp_err_t dspm_mult_s16_ae32(const int16_t* A, const int16_t* B, int16_t* C, int m, int n, int k, int shift);

// This is matrix multipliction function for ESP32 processor.
	.text
	.align	4
	.literal_position
	.literal	.LC0_1_38, 32767
	.literal	.LC1_1_39, 16383

	.global  dspm_mult_s16_aes3
	.global .dspm_mult_s16_ae32_body
	.type    dspm_mult_s16_aes3,@function

dspm_mult_s16_aes3: 

	entry	a1,80                   	#  

	movi.n	a10, 7
	and a10, a10, a7
	beqz  a10, .dspm_mult_s16_aes3_body
	// Call Esp32 function
	J 	.dspm_mult_s16_ae32_body

.dspm_mult_s16_aes3_body:
	mov.n	a10,a4                  	# [0]  
	mov.n	a11,a5                  	# [1]  
	l32i	a5,a1,80                 	# [2]  id:77 shift+0x0
	s32i.n	a3,a1,32               	# [3]  gra_spill_temp_0
	
	bltz	a5,.Lt_0_6146            	# [4]  

#.LBB3_dspm_mult_s16_aes3:	# 0x13
	l32r	a9,.LC0_1_38             	# [0]  
	ssr	a5                        	# [1]  
	sra	a9,a9                     	# [2]  

.LBB23_dspm_mult_s16_aes3:	# 0x1c
	s16i	a9,a1,0                  	# [0]  id:78 round_data_64+0x0
	s16i	a9,a1,2                  	# [1]  id:78 round_data_64+0x0
	s16i	a9,a1,4                  	# [2]  id:78 round_data_64+0x0
	s16i	a9,a1,6                  	# [3]  id:78 round_data_64+0x0
	s16i	a9,a1,8                  	# [4]  id:78 round_data_64+0x0
	s16i	a9,a1,10                 	# [5]  id:78 round_data_64+0x0
	s16i	a9,a1,12                 	# [6]  id:78 round_data_64+0x0
	s16i	a9,a1,14                 	# [7]  id:78 round_data_64+0x0

	blti	a11,1,.Lt_0_7426         	# [0]  

	mov.n	a13,a2                  	# [0]  
	slli	a4,a7,1                  	# [1]  
	mov.n	a12,a1                  	# [2]  
	l32i.n	a14,a1,32              	# [3]  gra_spill_temp_0
	movi.n	a15,15                 	# [4]  
	movi.n	a8,0                   	# [5]  
	slli	a9,a6,1                  	# [6]  
	s32i.n	a9,a1,36               	# [7]  gra_spill_temp_1
	s32i.n	a8,a1,44               	# [8]  gra_spill_temp_3
	sub	a15,a15,a5                	# [9]  
	addi.n	a8,a7,7                	# [10]  
	movgez	a8,a7,a7               	# [11]  
	srai	a8,a8,3                  	# [12]  
	s32i.n	a8,a1,40               	# [13]  gra_spill_temp_2
	slli	a8,a8,4                  	# [14]  
	add.n	a14,a14,a8              	# [15]  

.Lt_0_7938:	# 0x5d
	l32i.n	a8,a1,40               	# [0]  gra_spill_temp_2
	beqz.n	a8,.Lt_0_8194          	# [2]  

	l32i.n	a7,a1,32               	# [0]  gra_spill_temp_0
	mov.n	a2,a13                  	# [1]  

.Lt_0_8706:	# 0x65
	ee.ldqa.u16.128.ip	a12,0      	# [0]  id:80
	ee.vldbc.16.ip	q1,a2,2        	# [1]  id:79
	mov.n	a3,a7                   	# [2]  
	ee.vld.128.xp	q0,a3,a4        	# [3]  id:81
	addi	a7,a7,16                 	# [4]  
	blti	a6,1,.Lt_0_8962          	# [5]  

	srai	a5,a6,1                  	# [0]  
	bbci	a6,0,.LBB68_dspm_mult_s16_aes3 	# [1]  

	ee.vmulas.s16.qacc.ldbc.incp	q1,a2,q0,q1 	# [0]  id:82
	ee.vld.128.xp	q0,a3,a4        	# [1]  id:83

.LBB68_dspm_mult_s16_aes3:	# 0x82
	loopgtz	a5,.LBB74_dspm_mult_s16_aes3 	# [0]  

.LBB64_dspm_mult_s16_aes3:	# 0x85
	ee.vld.128.xp	q2,a3,a4        	# [0*II+0]  id:83
	ee.vmulas.s16.qacc.ldbc.incp	q1,a2,q0,q1 	# [0*II+1]  id:82
	ee.vld.128.xp	q0,a3,a4        	# [0*II+2]  id:83
	ee.vmulas.s16.qacc.ldbc.incp	q1,a2,q2,q1 	# [0*II+3]  id:82

.LBB74_dspm_mult_s16_aes3:	# 0x91

.Lt_0_8962:	# 0x91
	mov.n	a2,a13                  	# [0]  
	ee.srcmb.s16.qacc	q0,a15,1    	# [1]  
	ee.vst.128.ip	q0,a10,16       	# [2]  id:85
	bne	a7,a14,.Lt_0_8706         	# [3]  

.Lt_0_8194:	# 0x9c
	l32i.n	a8,a1,36               	# [0]  gra_spill_temp_1
	l32i.n	a9,a1,44               	# [1]  gra_spill_temp_3
	add.n	a13,a13,a8              	# [2]  
	addi.n	a9,a9,1                	# [3]  
	s32i.n	a9,a1,44               	# [4]  gra_spill_temp_3
	bne	a11,a9,.Lt_0_7938         	# [5]  

.Lt_0_7426:	# 0xa9
	movi.n	a2,0                   	# [0]  
	retw.n                        	# [1]  

.Lt_0_6146:	# 0xad
	l32r	a9,.LC1_1_39             	# [0]  
	ssr	a5                        	# [1]  
	sra	a9,a9                     	# [2]  
	j	.LBB23_dspm_mult_s16_aes3   	# [3] 


#endif // dspm_mult_s16_ae32_enabled