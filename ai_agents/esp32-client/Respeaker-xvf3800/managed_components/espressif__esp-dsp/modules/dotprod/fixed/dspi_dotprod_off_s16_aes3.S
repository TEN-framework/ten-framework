// Copyright 2018-2021 Espressif Systems (Shanghai) PTE LTD
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

#include "dspi_dotprod_platform.h"
#if (dspi_dotprod_aes3_enabled == 1)

    .text
    .align	4
    .literal	.LC0_1_61, 458755

    # Program Unit: dspi_dotprod_off_s16_aes3
    .type	dspi_dotprod_off_s16_aes3, @function
    .align	 4
    .global	dspi_dotprod_off_s16_aes3
dspi_dotprod_off_s16_aes3:	# 0x4
.LBB1_dspi_dotprod_off_s16_aes3:	# 0x4
    entry	a1,128                  	#  
    l32i.n	a10,a2,4               	# [0]  id:760
    l32i.n	a12,a2,12              	# [1]  id:759
    mull	a8,a10,a5                	# [2]  
    blt	a12,a8,.LBB83_dspi_dotprod_off_s16_aes3 	# [4]  

    l32i.n	a13,a2,8               	# [0]  id:761
    l32i.n	a9,a2,16               	# [1]  id:762
    mull	a11,a13,a6               	# [2]  
    blt	a9,a11,.LBB83_dspi_dotprod_off_s16_aes3 	# [4]  

    l32i.n	a15,a3,4               	# [0]  id:764
    l32i.n	a14,a3,12              	# [1]  id:763
    mull	a11,a15,a5               	# [2]  
    blt	a14,a11,.LBB83_dspi_dotprod_off_s16_aes3 	# [4]  

    l32i.n	a8,a3,16               	# [0]  id:766
    l32i.n	a9,a3,8                	# [1]  id:765
    s32i	a9,a1,88                 	# [2]  gra_spill_temp_2
    mull	a9,a9,a6                 	# [3]  
    blt	a8,a9,.LBB83_dspi_dotprod_off_s16_aes3 	# [5]  

    l32i.n	a8,a3,0                	# [0]  id:767
    s32i	a8,a1,84                 	# [1]  gra_spill_temp_1
    bbsi	a8,0,.Lt_0_36354         	# [2]  

    bne	a14,a11,.Lt_0_36354       	# [0]  

    bnei	a15,1,.Lt_0_36354        	# [0]  

    l32i	a9,a1,88                 	# [0]  gra_spill_temp_2
    beqi	a9,1,.Lt_0_19458         	# [2]  

.Lt_0_36354:	# 0x46
.Lt_0_19714:	# 0x46
    mov.n	a10,a2                  	# [0]  
    mov.n	a11,a3                  	# [1]  
    mov.n	a12,a4                  	# [2]  
    mov.n	a13,a5                  	# [3]  
    mov.n	a14,a6                  	# [4]  
    mov.n	a15,a7                  	# [5]  
    l16si	a8,a1,128               	# [6]  id:768 offset+0x0
    s32i.n	a8,a1,0                	# [7]  id:875
    .type	dspi_dotprod_off_s16_ansi, @function
    call8	dspi_dotprod_off_s16_ansi 	# [8]  dspi_dotprod_off_s16_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  

.LBB83_dspi_dotprod_off_s16_aes3:	# 0x5e
    l32r	a2,.LC0_1_61             	# [0]  
    retw.n                        	# [1]  

.Lt_0_19458:	# 0x63
    addi.n	a9,a10,-1              	# [0]  
    bnez	a9,.Lt_0_37122           	# [1]  

    addi.n	a10,a13,-1             	# [0]  
    bnez	a10,.Lt_0_37122          	# [1]  

    extui	a11,a5,0,3              	# [0]  
    bnez.n	a11,.Lt_0_37122        	# [1]  

    blti	a6,4,.Lt_0_37122         	# [0]  

    movi.n	a14,32                 	# [0]  
    blt	a14,a5,.LBB27_dspi_dotprod_off_s16_aes3 	# [1]  

.Lt_0_37634:	# 0x7a
.Lt_0_21506:	# 0x7a
    l32i	a15,a1,84                	# [0]  gra_spill_temp_1
    l32i.n	a2,a2,0                	# [1]  id:769
    l16si	a9,a1,128               	# [2]  id:768 offset+0x0
    mull	a10,a12,a13              	# [3]  
    addi	a8,a1,16                 	# [4]  temp_offset
    slli	a10,a10,1                	# [5]  
    s32i	a10,a1,80                	# [6]  gra_spill_temp_0
    movi.n	a10,2                  	# [7]  
    # loop-count fixed at 2
    loop	a10,.LBB137_dspi_dotprod_off_s16_aes3 	# [8]  

.LBB132_dspi_dotprod_off_s16_aes3:	# 0x93
    s16i	a9,a8,0                  	# [0*II+0]  id:770 temp_offset+0x0
    s16i	a9,a8,2                  	# [0*II+1]  id:770 temp_offset+0x0
    s16i	a9,a8,4                  	# [0*II+2]  id:770 temp_offset+0x0
    s16i	a9,a8,6                  	# [0*II+3]  id:770 temp_offset+0x0
    s16i	a9,a8,8                  	# [0*II+4]  id:770 temp_offset+0x0
    s16i	a9,a8,10                 	# [0*II+5]  id:770 temp_offset+0x0
    s16i	a9,a8,12                 	# [0*II+6]  id:770 temp_offset+0x0
    s16i	a9,a8,14                 	# [0*II+7]  id:770 temp_offset+0x0
    addi	a8,a8,16                 	# [0*II+8]  

.LBB137_dspi_dotprod_off_s16_aes3:	# 0xae
    mov.n	a3,a6                   	# [0]  
    addi	a11,a5,-24               	# [1]  
    addi	a12,a1,24                	# [3]  temp_offset+8
    movi.n	a13,0                  	# [4]  
    wur.sar_byte	a13              	# [5]  
    wur.accx_0	a13                	# [6]  
    wur.accx_1	a13                	# [7]  
    ee.vld.128.ip	q6,a12,0        	# [8]  id:771
    s32i.n	a12,a1,48              	# [9]  offset_data_ptr
    beqz	a11,.LBB34_dspi_dotprod_off_s16_aes3 	# [10]  

.Lt_0_25602:	# 0xc8
.Lt_0_25090:	# 0xc8
    ee.vld.128.ip	q0,a15,16       	# [0]  id:786
    addi	a14,a5,-16               	# [1]  
    beqz	a14,.LBB40_dspi_dotprod_off_s16_aes3 	# [2]  

.Lt_0_27138:	# 0xd1
.Lt_0_26626:	# 0xd1
    addi	a8,a5,-8                 	# [0]  
    beqz	a8,.LBB46_dspi_dotprod_off_s16_aes3 	# [1]  

.Lt_0_28674:	# 0xd7
.Lt_0_28162:	# 0xd7
    addi	a9,a5,-32                	# [0]  
    beqz	a9,.LBB52_dspi_dotprod_off_s16_aes3 	# [1]  

.Lt_0_30210:	# 0xdd
.Lt_0_29698:	# 0xdd
    addi	a10,a5,-64               	# [0]  
    beqz	a10,.LBB58_dspi_dotprod_off_s16_aes3 	# [1]  

    movi.n	a11,64                 	# [0]  
    bge	a11,a5,.Lt_0_33026        	# [1]  

    movi.n	a12,0                  	# [0]  
    ee.ld.128.usar.ip	q1,a2,16    	# [1]  id:848
    ee.ld.128.usar.ip	q2,a2,16    	# [2]  id:849
    ee.src.q.ld.ip	q3,a2,16,q1,q2 	# [4]  id:850
    beqz.n	a3,.Lt_0_33026         	# [5]  

    slli	a8,a5,1                  	# [0]  
    l32i	a14,a1,80                	# [1]  gra_spill_temp_0
    addi	a13,a5,31                	# [2]  
    movgez	a13,a5,a5              	# [3]  
    srai	a13,a13,5                	# [4]  
    sub	a14,a14,a8                	# [5]  
    addi	a14,a14,16               	# [6]  
    addi.n	a13,a13,-1             	# [7]  

.Lt_0_33794:	# 0x10c
    beqz.n	a13,.Lt_0_34050        	# [0]  

    loopnez	a13,.LBB273_dspi_dotprod_off_s16_aes3 	# [0]  

.LBB271_dspi_dotprod_off_s16_aes3:	# 0x111
    ee.vmulas.s16.accx.ld.ip.qup	q0,a2,16,q0,q1,q2,q3 	# [0*II+0]  id:851
    ee.vmulas.s16.accx.ld.ip	q1,a15,16,q1,q6 	# [0*II+1]  id:852
    ee.vmulas.s16.accx.ld.ip.qup	q1,a2,16,q1,q2,q3,q0 	# [0*II+3]  id:853
    ee.vmulas.s16.accx.ld.ip	q4,a15,16,q2,q6 	# [0*II+4]  id:854
    ee.vmulas.s16.accx.ld.ip.qup	q2,a2,16,q4,q3,q0,q1 	# [0*II+6]  id:855
    ee.vmulas.s16.accx.ld.ip	q4,a15,16,q3,q6 	# [0*II+7]  id:856
    ee.vmulas.s16.accx.ld.ip.qup	q3,a2,16,q4,q0,q1,q2 	# [0*II+9]  id:857
    ee.vmulas.s16.accx.ld.ip	q0,a15,16,q0,q6 	# [0*II+10]  id:858

.LBB273_dspi_dotprod_off_s16_aes3:	# 0x131

.Lt_0_34050:	# 0x131
    ee.vmulas.s16.accx.ld.ip.qup	q0,a2,16,q0,q1,q2,q3 	# [0]  id:859
    ee.vmulas.s16.accx.ld.ip	q1,a15,16,q1,q6 	# [1]  id:860
    movi.n	a9,32                  	# [2]  
    ee.vmulas.s16.accx.ld.xp.qup	q7,a2,a14,q1,q2,q3,q0 	# [3]  id:861
    ee.vmulas.s16.accx.ld.ip	q5,a15,16,q2,q6 	# [4]  id:862
    movi.n	a10,-16                	# [5]  
    ee.vmulas.s16.accx.ld.xp.qup	q2,a2,a10,q5,q3,q0,q7 	# [6]  id:863
    ee.vmulas.s16.accx.ld.ip	q4,a15,16,q3,q6 	# [7]  id:865
    ee.ld.128.usar.xp	q1,a2,a9    	# [8]  id:864
    addi.n	a12,a12,1              	# [9]  
    ee.vmulas.s16.accx.ld.ip.qup	q3,a2,16,q4,q0,q1,q2 	# [10]  id:866
    ee.vmulas.s16.accx.ld.ip	q0,a15,16,q0,q6 	# [11]  id:867
    bne	a12,a3,.Lt_0_33794        	# [12]  

.Lt_0_33026:	# 0x15d
.Lt_0_32770:	# 0x15d
    rur.accx_0	a9                 	# [0]  
    rur.accx_1	a10                	# [1]  
    blti	a7,1,.Lt_0_35586         	# [2]  

    movi.n	a2,0                   	# [0]  
    addi	a13,a7,-33               	# [1]  
    addi.n	a14,a7,-1              	# [2]  
    ssr	a14                       	# [3]  
    sra	a12,a10                   	# [4]  
    src	a11,a10,a9                	# [5]  
    movgez	a11,a12,a13            	# [6]  
    addi.n	a11,a11,1              	# [7]  
    srai	a11,a11,1                	# [8]  
    s16i	a11,a4,0                 	# [9]  id:873
    retw.n                        	# [10]  

.Lt_0_37122:	# 0x183
.Lt_0_20738:	# 0x183
    mov.n	a10,a2                  	# [0]  
    mov.n	a11,a3                  	# [1]  
    mov.n	a12,a4                  	# [2]  
    mov.n	a13,a5                  	# [3]  
    mov.n	a14,a6                  	# [4]  
    mov.n	a15,a7                  	# [5]  
    l16si	a8,a1,128               	# [6]  id:768 offset+0x0
    s32i.n	a8,a1,0                	# [7]  id:876
    call8	dspi_dotprod_off_s16_ansi 	# [8]  dspi_dotprod_off_s16_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  

.LBB27_dspi_dotprod_off_s16_aes3:	# 0x19b
    extui	a9,a5,0,1               	# [0]  
    beqz	a9,.Lt_0_37634           	# [1]  

    mov.n	a10,a2                  	# [0]  
    mov.n	a11,a3                  	# [1]  
    mov.n	a12,a4                  	# [2]  
    mov.n	a13,a5                  	# [3]  
    mov.n	a14,a6                  	# [4]  
    mov.n	a15,a7                  	# [5]  
    l16si	a8,a1,128               	# [6]  id:768 offset+0x0
    s32i.n	a8,a1,0                	# [7]  id:877
    call8	dspi_dotprod_off_s16_ansi 	# [8]  dspi_dotprod_off_s16_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  

.LBB34_dspi_dotprod_off_s16_aes3:	# 0x1b9
    movi.n	a10,32                 	# [0]  
    movi.n	a11,-16                	# [1]  
    l32i	a12,a1,80                	# [2]  gra_spill_temp_0
    ee.ld.128.usar.ip	q0,a2,16    	# [3]  id:776
    ee.ld.128.usar.ip	q2,a2,16    	# [4]  id:777
    addi	a12,a12,-32              	# [5]  
    ee.src.q.ld.ip	q3,a2,16,q0,q2 	# [6]  id:778
    loopgtz	a6,.LBB159_dspi_dotprod_off_s16_aes3 	# [7]  

.LBB157_dspi_dotprod_off_s16_aes3:	# 0x1cf
    ee.vmulas.s16.accx.ld.ip	q1,a15,16,q0,q6 	# [0*II+0]  id:779
    ee.vmulas.s16.accx.ld.xp.qup	q1,a2,a12,q1,q0,q2,q3 	# [0*II+2]  id:780
    ee.vmulas.s16.accx.ld.ip	q0,a15,16,q2,q6 	# [0*II+3]  id:781
    ee.vmulas.s16.accx.ld.xp.qup	q2,a2,a11,q0,q2,q3,q1 	# [0*II+5]  id:782
    ee.vmulas.s16.accx.ld.ip	q1,a15,16,q3,q6 	# [0*II+6]  id:784
    ee.ld.128.usar.xp	q0,a2,a10   	# [0*II+7]  id:783
    ee.vmulas.s16.accx.ld.ip.qup	q3,a2,16,q1,q3,q0,q2 	# [0*II+9]  id:785

.LBB159_dspi_dotprod_off_s16_aes3:	# 0x1ea
    j	.Lt_0_25602                 	# [0]  

.LBB40_dspi_dotprod_off_s16_aes3:	# 0x1ed
    movi.n	a10,32                 	# [0]  
    movi.n	a11,-16                	# [1]  
    srli	a3,a6,1                  	# [2]  
    l32i	a12,a1,80                	# [3]  gra_spill_temp_0
    ee.ld.128.usar.ip	q1,a2,16    	# [4]  id:787
    ee.ld.128.usar.ip	q2,a2,16    	# [5]  id:788
    addi	a12,a12,-16              	# [7]  
    ee.src.q.ld.xp	q3,a2,a12,q1,q2 	# [8]  id:789
    loopnez	a3,.LBB182_dspi_dotprod_off_s16_aes3 	# [9]  

.LBB180_dspi_dotprod_off_s16_aes3:	# 0x206
    ee.vmulas.s16.accx.ld.xp.qup	q0,a2,a11,q0,q1,q2,q3 	# [0*II+0]  id:790
    ee.vmulas.s16.accx.ld.ip	q3,a15,16,q1,q6 	# [0*II+1]  id:791
    ee.ld.128.usar.xp	q1,a2,a10   	# [0*II+2]  id:792
    ee.vmulas.s16.accx.ld.xp.qup	q3,a2,a12,q3,q2,q1,q0 	# [0*II+4]  id:793
    ee.vmulas.s16.accx.ld.ip	q4,a15,16,q2,q6 	# [0*II+5]  id:794
    ee.vmulas.s16.accx.ld.xp.qup	q2,a2,a11,q4,q1,q0,q3 	# [0*II+7]  id:795
    ee.vmulas.s16.accx.ld.ip	q3,a15,16,q1,q6 	# [0*II+8]  id:796
    ee.ld.128.usar.xp	q1,a2,a10   	# [0*II+9]  id:797
    ee.vmulas.s16.accx.ld.xp.qup	q3,a2,a12,q3,q0,q1,q2 	# [0*II+11]  id:798
    ee.vmulas.s16.accx.ld.ip	q0,a15,16,q0,q6 	# [0*II+12]  id:799

.LBB182_dspi_dotprod_off_s16_aes3:	# 0x22c
    j	.Lt_0_27138                 	# [0]  

.LBB46_dspi_dotprod_off_s16_aes3:	# 0x22f
    movi.n	a10,-16                	# [0]  
    l32i	a11,a1,80                	# [1]  gra_spill_temp_0
    addi	a8,a2,16                 	# [2]  
    addi	a11,a11,16               	# [3]  
    ee.ld.128.usar.xp	q2,a8,a10   	# [4]  id:800
    ee.ld.128.usar.xp	q1,a8,a11   	# [5]  id:801
    ee.src.q.ld.xp	q3,a8,a10,q1,q2 	# [7]  id:802
    ee.ld.128.usar.xp	q2,a8,a11   	# [8]  id:803
    srli	a3,a3,2                  	# [9]  
    mov.n	a2,a8                   	# [10]  
    loopnez	a3,.LBB205_dspi_dotprod_off_s16_aes3 	# [11]  

.LBB203_dspi_dotprod_off_s16_aes3:	# 0x24e
    ee.vmulas.s16.accx.ld.xp.qup	q3,a2,a10,q0,q1,q2,q3 	# [0*II+0]  id:804
    ee.vmulas.s16.accx.ld.ip	q0,a15,16,q1,q6 	# [0*II+1]  id:805
    ee.ld.128.usar.xp	q1,a2,a11   	# [0*II+2]  id:806
    ee.vmulas.s16.accx.ld.xp.qup	q3,a2,a10,q0,q2,q1,q3 	# [0*II+4]  id:807
    ee.vmulas.s16.accx.ld.ip	q0,a15,16,q2,q6 	# [0*II+5]  id:808
    ee.ld.128.usar.xp	q4,a2,a11   	# [0*II+6]  id:809
    ee.vmulas.s16.accx.ld.xp.qup	q3,a2,a10,q0,q1,q4,q3 	# [0*II+8]  id:810
    ee.vmulas.s16.accx.ld.ip	q0,a15,16,q1,q6 	# [0*II+9]  id:811
    ee.ld.128.usar.xp	q1,a2,a11   	# [0*II+10]  id:812
    ee.vmulas.s16.accx.ld.xp.qup	q3,a2,a10,q0,q4,q1,q3 	# [0*II+12]  id:813
    ee.vmulas.s16.accx.ld.ip	q0,a15,16,q4,q6 	# [0*II+13]  id:814
    ee.ld.128.usar.xp	q2,a2,a11   	# [0*II+14]  id:815

.LBB205_dspi_dotprod_off_s16_aes3:	# 0x27a
    j	.Lt_0_28674                 	# [0]  

.LBB52_dspi_dotprod_off_s16_aes3:	# 0x27d
    movi.n	a10,32                 	# [0]  
    movi.n	a11,-16                	# [1]  
    slli	a13,a5,1                 	# [2]  
    l32i	a12,a1,80                	# [3]  gra_spill_temp_0
    ee.ld.128.usar.ip	q1,a2,16    	# [4]  id:816
    ee.ld.128.usar.ip	q2,a2,16    	# [5]  id:817
    sub	a12,a12,a13               	# [6]  
    ee.src.q.ld.ip	q3,a2,16,q1,q2 	# [8]  id:818
    addi	a12,a12,16               	# [9]  
    loopnez	a3,.LBB228_dspi_dotprod_off_s16_aes3 	# [10]  

.LBB226_dspi_dotprod_off_s16_aes3:	# 0x299
    ee.vmulas.s16.accx.ld.ip.qup	q0,a2,16,q0,q1,q2,q3 	# [0*II+0]  id:819
    ee.vmulas.s16.accx.ld.ip	q4,a15,16,q1,q6 	# [0*II+1]  id:820
    ee.vmulas.s16.accx.ld.xp.qup	q4,a2,a12,q4,q2,q3,q0 	# [0*II+3]  id:821
    ee.vmulas.s16.accx.ld.ip	q1,a15,16,q2,q6 	# [0*II+4]  id:822
    ee.vmulas.s16.accx.ld.xp.qup	q2,a2,a11,q1,q3,q0,q4 	# [0*II+6]  id:823
    ee.vmulas.s16.accx.ld.ip	q4,a15,16,q3,q6 	# [0*II+7]  id:825
    ee.ld.128.usar.xp	q1,a2,a10   	# [0*II+8]  id:824
    ee.vmulas.s16.accx.ld.ip.qup	q3,a2,16,q4,q0,q1,q2 	# [0*II+10]  id:826
    ee.vmulas.s16.accx.ld.ip	q0,a15,16,q0,q6 	# [0*II+11]  id:827

.LBB228_dspi_dotprod_off_s16_aes3:	# 0x2bc
    j	.Lt_0_30210                 	# [0]  

.LBB58_dspi_dotprod_off_s16_aes3:	# 0x2bf
    movi.n	a10,32                 	# [0]  
    movi.n	a11,-16                	# [1]  
    slli	a13,a5,1                 	# [2]  
    l32i	a12,a1,80                	# [3]  gra_spill_temp_0
    ee.ld.128.usar.ip	q1,a2,16    	# [4]  id:828
    ee.ld.128.usar.ip	q2,a2,16    	# [5]  id:829
    sub	a12,a12,a13               	# [7]  
    addi	a12,a12,16               	# [8]  
    ee.src.q.ld.ip	q3,a2,16,q1,q2 	# [9]  id:830
    mov.n	a8,a2                   	# [10]  
    loopnez	a3,.LBB250_dspi_dotprod_off_s16_aes3 	# [11]  

.LBB248_dspi_dotprod_off_s16_aes3:	# 0x2dd
    ee.vmulas.s16.accx.ld.ip.qup	q0,a8,16,q0,q1,q2,q3 	# [0*II+0]  id:831
    ee.vmulas.s16.accx.ld.ip	q4,a15,16,q1,q6 	# [0*II+1]  id:832
    ee.vmulas.s16.accx.ld.ip.qup	q4,a8,16,q4,q2,q3,q0 	# [0*II+3]  id:833
    ee.vmulas.s16.accx.ld.ip	q1,a15,16,q2,q6 	# [0*II+4]  id:834
    ee.vmulas.s16.accx.ld.ip.qup	q1,a8,16,q1,q3,q0,q4 	# [0*II+6]  id:835
    ee.vmulas.s16.accx.ld.ip	q5,a15,16,q3,q6 	# [0*II+7]  id:836
    ee.vmulas.s16.accx.ld.ip.qup	q5,a8,16,q5,q0,q4,q1 	# [0*II+9]  id:837
    ee.vmulas.s16.accx.ld.ip	q0,a15,16,q0,q6 	# [0*II+10]  id:838
    ee.vmulas.s16.accx.ld.ip.qup	q0,a8,16,q0,q4,q1,q5 	# [0*II+12]  id:839
    ee.vmulas.s16.accx.ld.ip	q4,a15,16,q4,q6 	# [0*II+13]  id:840
    ee.vmulas.s16.accx.ld.xp.qup	q4,a8,a12,q4,q1,q5,q0 	# [0*II+15]  id:841
    ee.vmulas.s16.accx.ld.ip	q1,a15,16,q1,q6 	# [0*II+16]  id:842
    ee.vmulas.s16.accx.ld.xp.qup	q2,a8,a11,q1,q5,q0,q4 	# [0*II+18]  id:843
    ee.vmulas.s16.accx.ld.ip	q4,a15,16,q5,q6 	# [0*II+19]  id:845
    ee.ld.128.usar.xp	q1,a8,a10   	# [0*II+20]  id:844
    ee.vmulas.s16.accx.ld.ip.qup	q3,a8,16,q4,q0,q1,q2 	# [0*II+22]  id:846
    ee.vmulas.s16.accx.ld.ip	q0,a15,16,q0,q6 	# [0*II+23]  id:847

.LBB250_dspi_dotprod_off_s16_aes3:	# 0x320
    j	.Lt_0_33026                 	# [0]  

.Lt_0_35586:	# 0x323
    movi.n	a2,0                   	# [0]  
    sext	a14,a9,15                	# [1]  
    s16i	a14,a4,0                 	# [2]  id:874
    retw.n                        	# [3]  

#endif // dsps_dotprod_s16_aes3_enabled