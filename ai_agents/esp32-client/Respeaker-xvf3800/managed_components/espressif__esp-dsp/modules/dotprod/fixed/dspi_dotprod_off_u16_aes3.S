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

    # Program Unit: dspi_dotprod_off_u16_aes3
    .type	dspi_dotprod_off_u16_aes3, @function
    .align	 4
    .global	dspi_dotprod_off_u16_aes3
dspi_dotprod_off_u16_aes3:	# 0x4
.LBB1_dspi_dotprod_off_u16_aes3:	# 0x4
    entry	a1,144                  	#  
    l32i.n	a10,a2,4               	# [0]  id:760
    l32i.n	a12,a2,12              	# [1]  id:759
    mull	a8,a10,a5                	# [2]  
    blt	a12,a8,.LBB89_dspi_dotprod_off_u16_aes3 	# [4]  

    l32i.n	a13,a2,8               	# [0]  id:761
    l32i.n	a9,a2,16               	# [1]  id:762
    mull	a11,a13,a6               	# [2]  
    blt	a9,a11,.LBB89_dspi_dotprod_off_u16_aes3 	# [4]  

    l32i.n	a15,a3,4               	# [0]  id:764
    l32i.n	a14,a3,12              	# [1]  id:763
    mull	a11,a15,a5               	# [2]  
    blt	a14,a11,.LBB89_dspi_dotprod_off_u16_aes3 	# [4]  

    l32i.n	a8,a3,16               	# [0]  id:766
    l32i.n	a9,a3,8                	# [1]  id:765
    s32i	a9,a1,104                	# [2]  gra_spill_temp_2
    mull	a9,a9,a6                 	# [3]  
    blt	a8,a9,.LBB89_dspi_dotprod_off_u16_aes3 	# [5]  

    l32i.n	a8,a3,0                	# [0]  id:767
    s32i	a8,a1,100                	# [1]  gra_spill_temp_1
    bbsi	a8,0,.Lt_0_36354         	# [2]  

    bne	a14,a11,.Lt_0_36354       	# [0]  

    bnei	a15,1,.Lt_0_36354        	# [0]  

    l32i	a9,a1,104                	# [0]  gra_spill_temp_2
    beqi	a9,1,.Lt_0_19458         	# [2]  

.Lt_0_36354:	# 0x46
.Lt_0_19714:	# 0x46
    mov.n	a10,a2                  	# [0]  
    mov.n	a11,a3                  	# [1]  
    mov.n	a12,a4                  	# [2]  
    mov.n	a13,a5                  	# [3]  
    mov.n	a14,a6                  	# [4]  
    mov.n	a15,a7                  	# [5]  
    l16ui	a8,a1,144               	# [6]  id:768 offset+0x0
    s32i.n	a8,a1,0                	# [7]  id:876
    .type	dspi_dotprod_off_u16_ansi, @function
    call8	dspi_dotprod_off_u16_ansi 	# [8]  dspi_dotprod_off_u16_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  

.LBB89_dspi_dotprod_off_u16_aes3:	# 0x5e
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
    blt	a14,a5,.LBB27_dspi_dotprod_off_u16_aes3 	# [1]  

.Lt_0_37634:	# 0x7a
.Lt_0_21506:	# 0x7a
    l16ui	a9,a1,144               	# [0]  id:768 offset+0x0
    addi	a8,a1,16                 	# [1]  temp_offset
    l32i.n	a15,a2,0               	# [2]  id:769
    mull	a10,a12,a13              	# [3]  
    l32i	a2,a1,100                	# [4]  gra_spill_temp_1
    slli	a10,a10,1                	# [5]  
    s32i	a10,a1,96                	# [6]  gra_spill_temp_0
    movi.n	a10,2                  	# [7]  
    # loop-count fixed at 2
    loop	a10,.LBB143_dspi_dotprod_off_u16_aes3 	# [8]  

.LBB138_dspi_dotprod_off_u16_aes3:	# 0x93
    s16i	a9,a8,0                  	# [0*II+0]  id:770 temp_offset+0x0
    s16i	a9,a8,2                  	# [0*II+1]  id:770 temp_offset+0x0
    s16i	a9,a8,4                  	# [0*II+2]  id:770 temp_offset+0x0
    s16i	a9,a8,6                  	# [0*II+3]  id:770 temp_offset+0x0
    s16i	a9,a8,8                  	# [0*II+4]  id:770 temp_offset+0x0
    s16i	a9,a8,10                 	# [0*II+5]  id:770 temp_offset+0x0
    s16i	a9,a8,12                 	# [0*II+6]  id:770 temp_offset+0x0
    s16i	a9,a8,14                 	# [0*II+7]  id:770 temp_offset+0x0
    addi	a8,a8,16                 	# [0*II+8]  

.LBB143_dspi_dotprod_off_u16_aes3:	# 0xae
    mov.n	a3,a6                   	# [0]  
    addi	a11,a5,-24               	# [1]  
    addi	a12,a1,24                	# [3]  temp_offset+8
    movi.n	a13,0                  	# [4]  
    wur.sar_byte	a13              	# [5]  
    wur.accx_0	a13                	# [6]  
    wur.accx_1	a13                	# [7]  
    ee.vld.128.ip	q6,a12,0        	# [8]  id:771
    s32i.n	a12,a1,48              	# [9]  offset_data_ptr
    beqz	a11,.LBB34_dspi_dotprod_off_u16_aes3 	# [10]  

    l32i	a2,a1,100                	# [0]  gra_spill_temp_1
    ee.vld.128.ip	q0,a2,16        	# [2]  id:787
    st.qr	q0,a1,64                	# [3]  q0

.Lt_0_25090:	# 0xd1
    addi	a14,a5,-16               	# [0]  
    beqz	a14,.LBB43_dspi_dotprod_off_u16_aes3 	# [1]  

.Lt_0_27138:	# 0xd7
.Lt_0_26626:	# 0xd7
    addi	a8,a5,-8                 	# [0]  
    beqz	a8,.LBB50_dspi_dotprod_off_u16_aes3 	# [1]  

.Lt_0_28674:	# 0xdd
.Lt_0_28162:	# 0xdd
    addi	a9,a5,-32                	# [0]  
    beqz	a9,.LBB57_dspi_dotprod_off_u16_aes3 	# [1]  

.Lt_0_30210:	# 0xe3
.Lt_0_29698:	# 0xe3
    addi	a10,a5,-64               	# [0]  
    beqz	a10,.LBB64_dspi_dotprod_off_u16_aes3 	# [1]  

    movi.n	a11,64                 	# [0]  
    bge	a11,a5,.Lt_0_33026        	# [1]  

    movi.n	a12,0                  	# [0]  
    ee.ld.128.usar.ip	q1,a15,16   	# [1]  id:849
    ee.ld.128.usar.ip	q2,a15,16   	# [2]  id:850
    ee.src.q.ld.ip	q3,a15,16,q1,q2 	# [4]  id:851
    beqz.n	a3,.Lt_0_33026         	# [5]  

    ld.qr	q0,a1,64                	# [0]  q0
    slli	a8,a5,1                  	# [1]  
    l32i	a14,a1,96                	# [2]  gra_spill_temp_0
    addi	a13,a5,31                	# [3]  
    movgez	a13,a5,a5              	# [4]  
    srai	a13,a13,5                	# [5]  
    sub	a14,a14,a8                	# [6]  
    addi	a14,a14,16               	# [7]  
    addi.n	a13,a13,-1             	# [8]  

.Lt_0_33794:	# 0x115
    beqz.n	a13,.Lt_0_34050        	# [0]  

    loopnez	a13,.LBB280_dspi_dotprod_off_u16_aes3 	# [0]  

.LBB278_dspi_dotprod_off_u16_aes3:	# 0x11a
    ee.vmulas.u16.accx.ld.ip.qup	q0,a15,16,q0,q1,q2,q3 	# [0*II+0]  id:852
    ee.vmulas.u16.accx.ld.ip	q1,a2,16,q1,q6 	# [0*II+1]  id:853
    ee.vmulas.u16.accx.ld.ip.qup	q1,a15,16,q1,q2,q3,q0 	# [0*II+3]  id:854
    ee.vmulas.u16.accx.ld.ip	q4,a2,16,q2,q6 	# [0*II+4]  id:855
    ee.vmulas.u16.accx.ld.ip.qup	q2,a15,16,q4,q3,q0,q1 	# [0*II+6]  id:856
    ee.vmulas.u16.accx.ld.ip	q4,a2,16,q3,q6 	# [0*II+7]  id:857
    ee.vmulas.u16.accx.ld.ip.qup	q3,a15,16,q4,q0,q1,q2 	# [0*II+9]  id:858
    ee.vmulas.u16.accx.ld.ip	q0,a2,16,q0,q6 	# [0*II+10]  id:859

.LBB280_dspi_dotprod_off_u16_aes3:	# 0x13a

.Lt_0_34050:	# 0x13a
    ee.vmulas.u16.accx.ld.ip.qup	q4,a15,16,q0,q1,q2,q3 	# [0]  id:860
    ee.vmulas.u16.accx.ld.ip	q1,a2,16,q1,q6 	# [1]  id:861
    movi.n	a9,32                  	# [2]  
    ee.vmulas.u16.accx.ld.xp.qup	q0,a15,a14,q1,q2,q3,q4 	# [3]  id:862
    ee.vmulas.u16.accx.ld.ip	q7,a2,16,q2,q6 	# [4]  id:863
    movi.n	a10,-16                	# [5]  
    ee.vmulas.u16.accx.ld.xp.qup	q2,a15,a10,q7,q3,q4,q0 	# [6]  id:864
    ee.vmulas.u16.accx.ld.ip	q5,a2,16,q3,q6 	# [7]  id:866
    ee.ld.128.usar.xp	q1,a15,a9   	# [8]  id:865
    addi.n	a12,a12,1              	# [9]  
    ee.vmulas.u16.accx.ld.ip.qup	q3,a15,16,q5,q4,q1,q2 	# [10]  id:867
    ee.vmulas.u16.accx.ld.ip	q0,a2,16,q4,q6 	# [11]  id:868
    bne	a12,a3,.Lt_0_33794        	# [12]  

.Lt_0_33026:	# 0x166
.Lt_0_32770:	# 0x166
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
    srli	a11,a11,1                	# [8]  
    s16i	a11,a4,0                 	# [9]  id:874
    retw.n                        	# [10]  

.Lt_0_37122:	# 0x18c
.Lt_0_20738:	# 0x18c
    mov.n	a10,a2                  	# [0]  
    mov.n	a11,a3                  	# [1]  
    mov.n	a12,a4                  	# [2]  
    mov.n	a13,a5                  	# [3]  
    mov.n	a14,a6                  	# [4]  
    mov.n	a15,a7                  	# [5]  
    l16ui	a8,a1,144               	# [6]  id:768 offset+0x0
    s32i.n	a8,a1,0                	# [7]  id:877
    call8	dspi_dotprod_off_u16_ansi 	# [8]  dspi_dotprod_off_u16_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  

.LBB27_dspi_dotprod_off_u16_aes3:	# 0x1a4
    extui	a9,a5,0,1               	# [0]  
    beqz	a9,.Lt_0_37634           	# [1]  

    mov.n	a10,a2                  	# [0]  
    mov.n	a11,a3                  	# [1]  
    mov.n	a12,a4                  	# [2]  
    mov.n	a13,a5                  	# [3]  
    mov.n	a14,a6                  	# [4]  
    mov.n	a15,a7                  	# [5]  
    l16ui	a8,a1,144               	# [6]  id:768 offset+0x0
    s32i.n	a8,a1,0                	# [7]  id:878
    call8	dspi_dotprod_off_u16_ansi 	# [8]  dspi_dotprod_off_u16_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  

.LBB34_dspi_dotprod_off_u16_aes3:	# 0x1c2
    ee.ld.128.usar.ip	q0,a15,16   	# [0]  id:776
    ee.ld.128.usar.ip	q2,a15,16   	# [1]  id:777
    ee.src.q.ld.ip	q3,a15,16,q0,q2 	# [3]  id:778
    beqz.n	a6,.Lt_0_25090         	# [4]  

    movi.n	a10,32                 	# [0]  
    l32i	a12,a1,96                	# [1]  gra_spill_temp_0
    movi.n	a11,-16                	# [2]  
    addi	a12,a12,-32              	# [3]  
    loopgtz	a6,.LBB166_dspi_dotprod_off_u16_aes3 	# [4]  

.LBB164_dspi_dotprod_off_u16_aes3:	# 0x1da
    ee.vmulas.u16.accx.ld.ip	q1,a2,16,q0,q6 	# [0*II+0]  id:779
    ee.vmulas.u16.accx.ld.xp.qup	q1,a15,a12,q1,q0,q2,q3 	# [0*II+2]  id:780
    ee.vmulas.u16.accx.ld.ip	q0,a2,16,q2,q6 	# [0*II+3]  id:781
    ee.vmulas.u16.accx.ld.xp.qup	q2,a15,a11,q0,q2,q3,q1 	# [0*II+5]  id:782
    ee.vmulas.u16.accx.ld.ip	q1,a2,16,q3,q6 	# [0*II+6]  id:784
    ee.ld.128.usar.xp	q0,a15,a10  	# [0*II+7]  id:783
    ee.vmulas.u16.accx.ld.ip.qup	q3,a15,16,q1,q3,q0,q2 	# [0*II+9]  id:785

.LBB166_dspi_dotprod_off_u16_aes3:	# 0x1f5
    st.qr	q1,a1,64                	# [0]  q0
    j	.Lt_0_25090                 	# [1]  

.LBB43_dspi_dotprod_off_u16_aes3:	# 0x1fb
    srli	a3,a6,1                  	# [0]  
    l32i	a12,a1,96                	# [1]  gra_spill_temp_0
    ee.ld.128.usar.ip	q1,a15,16   	# [2]  id:788
    ee.ld.128.usar.ip	q2,a15,16   	# [3]  id:789
    addi	a12,a12,-16              	# [5]  
    ee.src.q.ld.xp	q3,a15,a12,q1,q2 	# [6]  id:790
    beqz.n	a3,.Lt_0_27138         	# [7]  

    ld.qr	q0,a1,64                	# [0]  q0
    movi.n	a10,32                 	# [1]  
    movi.n	a11,-16                	# [2]  
    loopnez	a3,.LBB189_dspi_dotprod_off_u16_aes3 	# [3]  

.LBB187_dspi_dotprod_off_u16_aes3:	# 0x219
    ee.vmulas.u16.accx.ld.xp.qup	q0,a15,a11,q0,q1,q2,q3 	# [0*II+0]  id:791
    ee.vmulas.u16.accx.ld.ip	q3,a2,16,q1,q6 	# [0*II+1]  id:792
    ee.ld.128.usar.xp	q1,a15,a10  	# [0*II+2]  id:793
    ee.vmulas.u16.accx.ld.xp.qup	q3,a15,a12,q3,q2,q1,q0 	# [0*II+4]  id:794
    ee.vmulas.u16.accx.ld.ip	q4,a2,16,q2,q6 	# [0*II+5]  id:795
    ee.vmulas.u16.accx.ld.xp.qup	q2,a15,a11,q4,q1,q0,q3 	# [0*II+7]  id:796
    ee.vmulas.u16.accx.ld.ip	q3,a2,16,q1,q6 	# [0*II+8]  id:797
    ee.ld.128.usar.xp	q1,a15,a10  	# [0*II+9]  id:798
    ee.vmulas.u16.accx.ld.xp.qup	q3,a15,a12,q3,q0,q1,q2 	# [0*II+11]  id:799
    ee.vmulas.u16.accx.ld.ip	q0,a2,16,q0,q6 	# [0*II+12]  id:800

.LBB189_dspi_dotprod_off_u16_aes3:	# 0x23f
    st.qr	q0,a1,64                	# [0]  q0
    j	.Lt_0_27138                 	# [1]  

.LBB50_dspi_dotprod_off_u16_aes3:	# 0x245
    srli	a3,a3,2                  	# [0]  
    movi.n	a13,-16                	# [1]  
    l32i	a11,a1,96                	# [2]  gra_spill_temp_0
    addi	a15,a15,16               	# [3]  
    addi	a11,a11,16               	# [4]  
    ee.ld.128.usar.xp	q2,a15,a13  	# [5]  id:801
    ee.ld.128.usar.xp	q1,a15,a11  	# [6]  id:802
    ee.src.q.ld.xp	q3,a15,a13,q1,q2 	# [8]  id:803
    ee.ld.128.usar.xp	q2,a15,a11  	# [9]  id:804
    beqz.n	a3,.Lt_0_28674         	# [10]  

    ld.qr	q0,a1,64                	# [0]  q0
    movi.n	a10,-16                	# [1]  
    loopnez	a3,.LBB212_dspi_dotprod_off_u16_aes3 	# [2]  

.LBB210_dspi_dotprod_off_u16_aes3:	# 0x269
    ee.vmulas.u16.accx.ld.xp.qup	q3,a15,a10,q0,q1,q2,q3 	# [0*II+0]  id:805
    ee.vmulas.u16.accx.ld.ip	q0,a2,16,q1,q6 	# [0*II+1]  id:806
    ee.ld.128.usar.xp	q1,a15,a11  	# [0*II+2]  id:807
    ee.vmulas.u16.accx.ld.xp.qup	q3,a15,a10,q0,q2,q1,q3 	# [0*II+4]  id:808
    ee.vmulas.u16.accx.ld.ip	q0,a2,16,q2,q6 	# [0*II+5]  id:809
    ee.ld.128.usar.xp	q4,a15,a11  	# [0*II+6]  id:810
    ee.vmulas.u16.accx.ld.xp.qup	q3,a15,a10,q0,q1,q4,q3 	# [0*II+8]  id:811
    ee.vmulas.u16.accx.ld.ip	q0,a2,16,q1,q6 	# [0*II+9]  id:812
    ee.ld.128.usar.xp	q1,a15,a11  	# [0*II+10]  id:813
    ee.vmulas.u16.accx.ld.xp.qup	q3,a15,a10,q0,q4,q1,q3 	# [0*II+12]  id:814
    ee.vmulas.u16.accx.ld.ip	q0,a2,16,q4,q6 	# [0*II+13]  id:815
    ee.ld.128.usar.xp	q2,a15,a11  	# [0*II+14]  id:816

.LBB212_dspi_dotprod_off_u16_aes3:	# 0x295
    st.qr	q0,a1,64                	# [0]  q0
    j	.Lt_0_28674                 	# [1]  

.LBB57_dspi_dotprod_off_u16_aes3:	# 0x29b
    ee.ld.128.usar.ip	q1,a15,16   	# [0]  id:817
    ee.ld.128.usar.ip	q2,a15,16   	# [1]  id:818
    ee.src.q.ld.ip	q3,a15,16,q1,q2 	# [3]  id:819
    beqz.n	a3,.Lt_0_30210         	# [4]  

    ld.qr	q0,a1,64                	# [0]  q0
    movi.n	a10,32                 	# [1]  
    movi.n	a11,-16                	# [2]  
    l32i	a12,a1,96                	# [3]  gra_spill_temp_0
    slli	a13,a5,1                 	# [4]  
    sub	a12,a12,a13               	# [5]  
    addi	a12,a12,16               	# [6]  
    loopnez	a3,.LBB235_dspi_dotprod_off_u16_aes3 	# [7]  

.LBB233_dspi_dotprod_off_u16_aes3:	# 0x2bc
    ee.vmulas.u16.accx.ld.ip.qup	q0,a15,16,q0,q1,q2,q3 	# [0*II+0]  id:820
    ee.vmulas.u16.accx.ld.ip		q4,a2,16,q1,q6 	# [0*II+1]  id:821
    ee.vmulas.u16.accx.ld.xp.qup	q4,a15,a12,q4,q2,q3,q0 	# [0*II+3]  id:822
    ee.vmulas.u16.accx.ld.ip		q1,a2,16,q2,q6 	# [0*II+4]  id:823
    ee.vmulas.u16.accx.ld.xp.qup	q2,a15,a11,q1,q3,q0,q4 	# [0*II+6]  id:824
    ee.vmulas.u16.accx.ld.ip		q4,a2,16,q3,q6 	# [0*II+7]  id:826
    ee.ld.128.usar.xp				q1,a15,a10  	# [0*II+8]  id:825
    ee.vmulas.u16.accx.ld.ip.qup	q3,a15,16,q4,q0,q1,q2 	# [0*II+10]  id:827
    ee.vmulas.u16.accx.ld.ip		q0,a2,16,q0,q6 	# [0*II+11]  id:828

.LBB235_dspi_dotprod_off_u16_aes3:	# 0x2df
    st.qr	q0,a1,64                	# [0]  q0
    j	.Lt_0_30210                 	# [1]  

.LBB64_dspi_dotprod_off_u16_aes3:	# 0x2e5
    movi.n	a10,32                 	# [0]  
    movi.n	a11,-16                	# [1]  
    slli	a13,a5,1                 	# [2]  
    l32i	a12,a1,96                	# [3]  gra_spill_temp_0
    ee.ld.128.usar.ip	q1,a15,16   	# [4]  id:829
    ee.ld.128.usar.ip	q2,a15,16   	# [5]  id:830
    sub	a12,a12,a13               	# [7]  
    addi	a12,a12,16               	# [8]  
    ld.qr	q0,a1,64                	# [9]  q0
    ee.src.q.ld.ip	q3,a15,16,q1,q2 	# [10]  id:831
    mov.n	a8,a15                  	# [11]  
    loopnez	a3,.LBB257_dspi_dotprod_off_u16_aes3 	# [12]  

.LBB255_dspi_dotprod_off_u16_aes3:	# 0x306
    ee.vmulas.u16.accx.ld.ip.qup	q0,a8,16,q0,q1,q2,q3 	# [0*II+0]  id:832
    ee.vmulas.u16.accx.ld.ip		q4,a2,16,q1,q6 	# [0*II+1]  id:833
    ee.vmulas.u16.accx.ld.ip.qup	q4,a8,16,q4,q2,q3,q0 	# [0*II+3]  id:834
    ee.vmulas.u16.accx.ld.ip		q1,a2,16,q2,q6 	# [0*II+4]  id:835
    ee.vmulas.u16.accx.ld.ip.qup	q1,a8,16,q1,q3,q0,q4 	# [0*II+6]  id:836
    ee.vmulas.u16.accx.ld.ip		q5,a2,16,q3,q6 	# [0*II+7]  id:837
    ee.vmulas.u16.accx.ld.ip.qup	q5,a8,16,q5,q0,q4,q1 	# [0*II+9]  id:838
    ee.vmulas.u16.accx.ld.ip		q0,a2,16,q0,q6 	# [0*II+10]  id:839
    ee.vmulas.u16.accx.ld.ip.qup	q0,a8,16,q0,q4,q1,q5 	# [0*II+12]  id:840
    ee.vmulas.u16.accx.ld.ip		q4,a2,16,q4,q6 	# [0*II+13]  id:841
    ee.vmulas.u16.accx.ld.xp.qup	q4,a8,a12,q4,q1,q5,q0 	# [0*II+15]  id:842
    ee.vmulas.u16.accx.ld.ip		q1,a2,16,q1,q6 	# [0*II+16]  id:843
    ee.vmulas.u16.accx.ld.xp.qup	q2,a8,a11,q1,q5,q0,q4 	# [0*II+18]  id:844
    ee.vmulas.u16.accx.ld.ip		q4,a2,16,q5,q6 	# [0*II+19]  id:846
    ee.ld.128.usar.xp				q1,a8,a10   	# [0*II+20]  id:845
    ee.vmulas.u16.accx.ld.ip.qup	q3,a8,16,q4,q0,q1,q2 	# [0*II+22]  id:847
    ee.vmulas.u16.accx.ld.ip		q0,a2,16,q0,q6 	# [0*II+23]  id:848

.LBB257_dspi_dotprod_off_u16_aes3:	# 0x349
    j	.Lt_0_33026                 	# [0]  

.Lt_0_35586:	# 0x34c
    movi.n	a2,0                   	# [0]  
    sext	a14,a9,15                	# [1]  
    s16i	a14,a4,0                 	# [2]  id:875
    retw.n                        	# [3]  

#endif // dsps_dotprod_s16_aes3_enabled