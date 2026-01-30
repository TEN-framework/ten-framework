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
    .literal	.LC0_1_57, 458755

    # Program Unit: dspi_dotprod_off_u8_aes3
    .type	dspi_dotprod_off_u8_aes3, @function
    .align	 4
    .global	dspi_dotprod_off_u8_aes3
dspi_dotprod_off_u8_aes3:	# 0x4

.LBB1_dspi_dotprod_off_u8_aes3:	# 0x4
    entry	a1,112                  	#  
    l32i.n	a10,a2,4               	# [0]  id:745
    l32i.n	a12,a2,12              	# [1]  id:744
    mull	a8,a10,a5                	# [2]  
    blt	a12,a8,.LBB86_dspi_dotprod_off_u8_aes3 	# [4]  

    l32i.n	a13,a2,8               	# [0]  id:746
    l32i.n	a9,a2,16               	# [1]  id:747
    mull	a11,a13,a6               	# [2]  
    blt	a9,a11,.LBB86_dspi_dotprod_off_u8_aes3 	# [4]  

    l32i.n	a15,a3,4               	# [0]  id:749
    l32i.n	a14,a3,12              	# [1]  id:748
    mull	a11,a15,a5               	# [2]  
    blt	a14,a11,.LBB86_dspi_dotprod_off_u8_aes3 	# [4]  

    l32i.n	a8,a3,16               	# [0]  id:751
    l32i.n	a9,a3,8                	# [1]  id:750
    s32i	a9,a1,72                 	# [2]  gra_spill_temp_2
    mull	a9,a9,a6                 	# [3]  
    blt	a8,a9,.LBB86_dspi_dotprod_off_u8_aes3 	# [5]  

    l32i.n	a8,a3,0                	# [0]  id:752
    s32i	a8,a1,68                 	# [1]  gra_spill_temp_1
    bbsi	a8,0,.Lt_0_35330         	# [2]  

    bne	a14,a11,.Lt_0_35330       	# [0]  

    bnei	a15,1,.Lt_0_35330        	# [0]  

    l32i	a11,a1,72                	# [0]  gra_spill_temp_2
    beqi	a11,1,.Lt_0_18946        	# [2]  

.Lt_0_35330:	# 0x46
.Lt_0_19202:	# 0x46
    mov.n	a10,a2                  	# [0]  
    mov.n	a11,a3                  	# [1]  
    mov.n	a12,a4                  	# [2]  
    mov.n	a13,a5                  	# [3]  
    mov.n	a14,a6                  	# [4]  
    mov.n	a15,a7                  	# [5]  
    .type	dspi_dotprod_u8_ansi, @function
    call8	dspi_dotprod_u8_ansi    	# [6]  dspi_dotprod_u8_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  

.LBB86_dspi_dotprod_off_u8_aes3:	# 0x59
    l32r	a2,.LC0_1_57             	# [0]  
    retw.n                        	# [1]  

.Lt_0_18946:	# 0x5e
    addi.n	a14,a10,-1             	# [0]  
    bnez	a14,.Lt_0_36098          	# [1]  

    addi.n	a15,a13,-1             	# [0]  
    bnez	a15,.Lt_0_36098          	# [1]  

    extui	a8,a5,0,4               	# [0]  
    bnez.n	a8,.Lt_0_36098         	# [1]  

    blti	a6,4,.Lt_0_36098         	# [0]  

    movi.n	a9,64                  	# [0]  
    blt	a9,a5,.LBB27_dspi_dotprod_off_u8_aes3 	# [1]  

.Lt_0_36610:	# 0x75
.Lt_0_20994:	# 0x75
    l8ui	a9,a1,112                	# [0]  id:754 offset+0x0
    mov.n	a8,a1                   	# [1]  
    l32i.n	a15,a2,0               	# [2]  id:753
    mull	a10,a12,a13              	# [3]  
    l32i	a2,a1,68                 	# [4]  gra_spill_temp_1
    s32i	a10,a1,64                	# [5]  gra_spill_temp_0
    movi.n	a10,4                  	# [6]  
    # loop-count fixed at 4
    loop	a10,.LBB140_dspi_dotprod_off_u8_aes3 	# [7]  

.LBB135_dspi_dotprod_off_u8_aes3:	# 0x8a
    s8i	a9,a8,0                   	# [0*II+0]  id:755 temp_offset+0x0
    s8i	a9,a8,1                   	# [0*II+1]  id:755 temp_offset+0x0
    s8i	a9,a8,2                   	# [0*II+2]  id:755 temp_offset+0x0
    s8i	a9,a8,3                   	# [0*II+3]  id:755 temp_offset+0x0
    s8i	a9,a8,4                   	# [0*II+4]  id:755 temp_offset+0x0
    s8i	a9,a8,5                   	# [0*II+5]  id:755 temp_offset+0x0
    s8i	a9,a8,6                   	# [0*II+6]  id:755 temp_offset+0x0
    s8i	a9,a8,7                   	# [0*II+7]  id:755 temp_offset+0x0
    addi.n	a8,a8,8                	# [0*II+8]  

.LBB140_dspi_dotprod_off_u8_aes3:	# 0xa4
    mov.n	a3,a6                   	# [0]  
    addi	a11,a5,-48               	# [1]  
    addi.n	a12,a1,8               	# [3]  temp_offset+8
    movi.n	a13,0                  	# [4]  
    wur.accx_0	a13                	# [5]  
    wur.accx_1	a13                	# [6]  
    ee.vld.128.ip	q6,a12,0        	# [7]  id:756
    s32i.n	a12,a1,32              	# [8]  offset_data_ptr
    beqz	a11,.LBB34_dspi_dotprod_off_u8_aes3 	# [9]  

    l32i	a2,a1,68                 	# [0]  gra_spill_temp_1
    ee.vld.128.ip	q0,a2,16        	# [2]  id:771
    st.qr	q0,a1,48                	# [3]  q0

.Lt_0_24578:	# 0xc3
    addi	a14,a5,-32               	# [0]  
    beqz	a14,.LBB43_dspi_dotprod_off_u8_aes3 	# [1]  

.Lt_0_26626:	# 0xc9
.Lt_0_26114:	# 0xc9
    addi	a8,a5,-16                	# [0]  
    beqz	a8,.LBB50_dspi_dotprod_off_u8_aes3 	# [1]  

.Lt_0_28162:	# 0xcf
.Lt_0_27650:	# 0xcf
    addi	a9,a5,-64                	# [0]  
    beqz	a9,.LBB57_dspi_dotprod_off_u8_aes3 	# [1]  

.Lt_0_29698:	# 0xd5
.Lt_0_29186:	# 0xd5
    addi	a10,a5,-128              	# [0]  
    beqz	a10,.LBB64_dspi_dotprod_off_u8_aes3 	# [1]  

    movi	a11,128                  	# [0]  
    bge	a11,a5,.Lt_0_32514        	# [1]  

    movi.n	a12,0                  	# [0]  
    ee.ld.128.usar.ip	q1,a15,16   	# [1]  id:833
    ee.ld.128.usar.ip	q2,a15,16   	# [2]  id:834
    ee.src.q.ld.ip	q3,a15,16,q1,q2 	# [4]  id:835
    beqz.n	a3,.Lt_0_32514         	# [5]  

    ld.qr	q0,a1,48                	# [0]  q0
    l32i	a14,a1,64                	# [1]  gra_spill_temp_0
    addi	a13,a5,31                	# [2]  
    movgez	a13,a5,a5              	# [3]  
    srai	a13,a13,5                	# [4]  
    sub	a14,a14,a5                	# [5]  
    addi	a14,a14,16               	# [6]  
    addi.n	a13,a13,-1             	# [7]  

.Lt_0_33282:	# 0x105
    beqz.n	a13,.Lt_0_33538        	# [0]  

    loopnez	a13,.LBB277_dspi_dotprod_off_u8_aes3 	# [0]  

.LBB275_dspi_dotprod_off_u8_aes3:	# 0x10a
    ee.vmulas.u8.accx.ld.ip.qup	q0,a15,16,q0,q1,q2,q3 	# [0*II+0]  id:836
    ee.vmulas.u8.accx.ld.ip	q1,a2,16,q1,q6 	# [0*II+1]  id:837
    ee.vmulas.u8.accx.ld.ip.qup	q1,a15,16,q1,q2,q3,q0 	# [0*II+3]  id:838
    ee.vmulas.u8.accx.ld.ip	q4,a2,16,q2,q6 	# [0*II+4]  id:839
    ee.vmulas.u8.accx.ld.ip.qup	q2,a15,16,q4,q3,q0,q1 	# [0*II+6]  id:840
    ee.vmulas.u8.accx.ld.ip	q4,a2,16,q3,q6 	# [0*II+7]  id:841
    ee.vmulas.u8.accx.ld.ip.qup	q3,a15,16,q4,q0,q1,q2 	# [0*II+9]  id:842
    ee.vmulas.u8.accx.ld.ip	q0,a2,16,q0,q6 	# [0*II+10]  id:843

.LBB277_dspi_dotprod_off_u8_aes3:	# 0x12a

.Lt_0_33538:	# 0x12a
    ee.vmulas.u8.accx.ld.ip.qup	q4,a15,16,q0,q1,q2,q3 	# [0]  id:844
    ee.vmulas.u8.accx.ld.ip	q1,a2,16,q1,q6 	# [1]  id:845
    movi.n	a8,32                  	# [2]  
    ee.vmulas.u8.accx.ld.xp.qup	q0,a15,a14,q1,q2,q3,q4 	# [3]  id:846
    ee.vmulas.u8.accx.ld.ip	q7,a2,16,q2,q6 	# [4]  id:847
    movi.n	a9,-16                 	# [5]  
    ee.vmulas.u8.accx.ld.xp.qup	q2,a15,a9,q7,q3,q4,q0 	# [6]  id:848
    ee.vmulas.u8.accx.ld.ip	q5,a2,16,q3,q6 	# [7]  id:850
    ee.ld.128.usar.xp	q1,a15,a8   	# [8]  id:849
    addi.n	a12,a12,1              	# [9]  
    ee.vmulas.u8.accx.ld.ip.qup	q3,a15,16,q5,q4,q1,q2 	# [10]  id:851
    ee.vmulas.u8.accx.ld.ip	q0,a2,16,q4,q6 	# [11]  id:852
    bne	a12,a3,.Lt_0_33282        	# [12]  

.Lt_0_32514:	# 0x156
.Lt_0_32258:	# 0x156
    movi.n	a2,0                   	# [0]  
    rur.accx_0	a10                	# [1]  
    addi.n	a12,a7,-1              	# [2]  
    movi.n	a11,1                  	# [3]  
    ssl	a12                       	# [4]  
    sll	a11,a11                   	# [5]  
    ssr	a7                        	# [6]  
    add.n	a10,a10,a11             	# [7]  
    sra	a10,a10                   	# [8]  
    s8i	a10,a4,0                  	# [9]  id:854
    retw.n                        	# [10]  

.Lt_0_36098:	# 0x172
.Lt_0_20226:	# 0x172
    mov.n	a10,a2                  	# [0]  
    mov.n	a11,a3                  	# [1]  
    mov.n	a12,a4                  	# [2]  
    mov.n	a13,a5                  	# [3]  
    mov.n	a14,a6                  	# [4]  
    mov.n	a15,a7                  	# [5]  
    call8	dspi_dotprod_u8_ansi    	# [6]  dspi_dotprod_u8_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  

.LBB27_dspi_dotprod_off_u8_aes3:	# 0x185
    extui	a14,a5,0,1              	# [0]  
    beqz	a14,.Lt_0_36610          	# [1]  

    mov.n	a10,a2                  	# [0]  
    mov.n	a11,a3                  	# [1]  
    mov.n	a12,a4                  	# [2]  
    mov.n	a13,a5                  	# [3]  
    mov.n	a14,a6                  	# [4]  
    mov.n	a15,a7                  	# [5]  
    call8	dspi_dotprod_u8_ansi    	# [6]  dspi_dotprod_u8_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  

.LBB34_dspi_dotprod_off_u8_aes3:	# 0x19e
    ee.ld.128.usar.ip	q0,a15,16   	# [0]  id:760
    ee.ld.128.usar.ip	q2,a15,16   	# [1]  id:761
    ee.src.q.ld.ip	q3,a15,16,q0,q2 	# [3]  id:762
    beqz.n	a6,.Lt_0_24578         	# [4]  

    movi.n	a10,32                 	# [0]  
    l32i	a12,a1,64                	# [1]  gra_spill_temp_0
    movi.n	a11,-16                	# [2]  
    addi	a12,a12,-32              	# [3]  
    loopgtz	a6,.LBB163_dspi_dotprod_off_u8_aes3 	# [4]  

.LBB161_dspi_dotprod_off_u8_aes3:	# 0x1b6
    ee.vmulas.u8.accx.ld.ip	q1,a2,16,q0,q6 	# [0*II+0]  id:763
    ee.vmulas.u8.accx.ld.xp.qup	q1,a15,a12,q1,q0,q2,q3 	# [0*II+2]  id:764
    ee.vmulas.u8.accx.ld.ip	q0,a2,16,q2,q6 	# [0*II+3]  id:765
    ee.vmulas.u8.accx.ld.xp.qup	q2,a15,a11,q0,q2,q3,q1 	# [0*II+5]  id:766
    ee.vmulas.u8.accx.ld.ip	q1,a2,16,q3,q6 	# [0*II+6]  id:768
    ee.ld.128.usar.xp	q0,a15,a10  	# [0*II+7]  id:767
    ee.vmulas.u8.accx.ld.ip.qup	q3,a15,16,q1,q3,q0,q2 	# [0*II+9]  id:769

.LBB163_dspi_dotprod_off_u8_aes3:	# 0x1d1
    st.qr	q1,a1,48                	# [0]  q0
    j	.Lt_0_24578                 	# [1]  

.LBB43_dspi_dotprod_off_u8_aes3:	# 0x1d7
    srli	a3,a6,1                  	# [0]  
    l32i	a12,a1,64                	# [1]  gra_spill_temp_0
    ee.ld.128.usar.ip	q1,a15,16   	# [2]  id:772
    ee.ld.128.usar.ip	q2,a15,16   	# [3]  id:773
    addi	a12,a12,-16              	# [5]  
    ee.src.q.ld.xp	q3,a15,a12,q1,q2 	# [6]  id:774
    beqz.n	a3,.Lt_0_26626         	# [7]  

    ld.qr	q0,a1,48                	# [0]  q0
    movi.n	a10,32                 	# [1]  
    movi.n	a11,-16                	# [2]  
    loopnez	a3,.LBB186_dspi_dotprod_off_u8_aes3 	# [3]  

.LBB184_dspi_dotprod_off_u8_aes3:	# 0x1f5
    ee.vmulas.u8.accx.ld.xp.qup	q0,a15,a11,q0,q1,q2,q3 	# [0*II+0]  id:775
    ee.vmulas.u8.accx.ld.ip	q3,a2,16,q1,q6 	# [0*II+1]  id:776
    ee.ld.128.usar.xp	q1,a15,a10  	# [0*II+2]  id:777
    ee.vmulas.u8.accx.ld.xp.qup	q3,a15,a12,q3,q2,q1,q0 	# [0*II+4]  id:778
    ee.vmulas.u8.accx.ld.ip	q4,a2,16,q2,q6 	# [0*II+5]  id:779
    ee.vmulas.u8.accx.ld.xp.qup	q2,a15,a11,q4,q1,q0,q3 	# [0*II+7]  id:780
    ee.vmulas.u8.accx.ld.ip	q3,a2,16,q1,q6 	# [0*II+8]  id:781
    ee.ld.128.usar.xp	q1,a15,a10  	# [0*II+9]  id:782
    ee.vmulas.u8.accx.ld.xp.qup	q3,a15,a12,q3,q0,q1,q2 	# [0*II+11]  id:783
    ee.vmulas.u8.accx.ld.ip	q0,a2,16,q0,q6 	# [0*II+12]  id:784

.LBB186_dspi_dotprod_off_u8_aes3:	# 0x21b
    st.qr	q0,a1,48                	# [0]  q0
    j	.Lt_0_26626                 	# [1]  

.LBB50_dspi_dotprod_off_u8_aes3:	# 0x221
    srli	a3,a3,2                  	# [0]  
    movi.n	a13,-16                	# [1]  
    l32i	a11,a1,64                	# [2]  gra_spill_temp_0
    addi	a15,a15,16               	# [3]  
    addi	a11,a11,16               	# [4]  
    ee.ld.128.usar.xp	q2,a15,a13  	# [5]  id:785
    ee.ld.128.usar.xp	q1,a15,a11  	# [6]  id:786
    ee.src.q.ld.xp	q3,a15,a13,q1,q2 	# [8]  id:787
    ee.ld.128.usar.xp	q2,a15,a11  	# [9]  id:788
    beqz.n	a3,.Lt_0_28162         	# [10]  

    ld.qr	q0,a1,48                	# [0]  q0
    movi.n	a10,-16                	# [1]  
    loopnez	a3,.LBB209_dspi_dotprod_off_u8_aes3 	# [2]  

.LBB207_dspi_dotprod_off_u8_aes3:	# 0x245
    ee.vmulas.u8.accx.ld.xp.qup	q3,a15,a10,q0,q1,q2,q3 	# [0*II+0]  id:789
    ee.vmulas.u8.accx.ld.ip	q0,a2,16,q1,q6 	# [0*II+1]  id:790
    ee.ld.128.usar.xp	q1,a15,a11  	# [0*II+2]  id:791
    ee.vmulas.u8.accx.ld.xp.qup	q3,a15,a10,q0,q2,q1,q3 	# [0*II+4]  id:792
    ee.vmulas.u8.accx.ld.ip	q0,a2,16,q2,q6 	# [0*II+5]  id:793
    ee.ld.128.usar.xp	q4,a15,a11  	# [0*II+6]  id:794
    ee.vmulas.u8.accx.ld.xp.qup	q3,a15,a10,q0,q1,q4,q3 	# [0*II+8]  id:795
    ee.vmulas.u8.accx.ld.ip	q0,a2,16,q1,q6 	# [0*II+9]  id:796
    ee.ld.128.usar.xp	q1,a15,a11  	# [0*II+10]  id:797
    ee.vmulas.u8.accx.ld.xp.qup	q3,a15,a10,q0,q4,q1,q3 	# [0*II+12]  id:798
    ee.vmulas.u8.accx.ld.ip	q0,a2,16,q4,q6 	# [0*II+13]  id:799
    ee.ld.128.usar.xp	q2,a15,a11  	# [0*II+14]  id:800

.LBB209_dspi_dotprod_off_u8_aes3:	# 0x271
    st.qr	q0,a1,48                	# [0]  q0
    j	.Lt_0_28162                 	# [1]  

.LBB57_dspi_dotprod_off_u8_aes3:	# 0x277
    ee.ld.128.usar.ip	q1,a15,16   	# [0]  id:801
    ee.ld.128.usar.ip	q2,a15,16   	# [1]  id:802
    ee.src.q.ld.ip	q3,a15,16,q1,q2 	# [3]  id:803
    beqz.n	a3,.Lt_0_29698         	# [4]  

    ld.qr	q0,a1,48                	# [0]  q0
    movi.n	a10,32                 	# [1]  
    l32i	a12,a1,64                	# [2]  gra_spill_temp_0
    movi.n	a11,-16                	# [3]  
    sub	a12,a12,a5                	# [4]  
    addi	a12,a12,16               	# [5]  
    loopnez	a3,.LBB232_dspi_dotprod_off_u8_aes3 	# [6]  

.LBB230_dspi_dotprod_off_u8_aes3:	# 0x295
    ee.vmulas.u8.accx.ld.ip.qup	q0,a15,16,q0,q1,q2,q3 	# [0*II+0]  id:804
    ee.vmulas.u8.accx.ld.ip	q4,a2,16,q1,q6 	# [0*II+1]  id:805
    ee.vmulas.u8.accx.ld.xp.qup	q4,a15,a12,q4,q2,q3,q0 	# [0*II+3]  id:806
    ee.vmulas.u8.accx.ld.ip	q1,a2,16,q2,q6 	# [0*II+4]  id:807
    ee.vmulas.u8.accx.ld.xp.qup	q2,a15,a11,q1,q3,q0,q4 	# [0*II+6]  id:808
    ee.vmulas.u8.accx.ld.ip	q4,a2,16,q3,q6 	# [0*II+7]  id:809
    ee.ld.128.usar.xp	q1,a15,a10  	# [0*II+8]  id:810
    ee.vmulas.u8.accx.ld.ip.qup	q3,a15,16,q4,q0,q1,q2 	# [0*II+10]  id:811
    ee.vmulas.u8.accx.ld.ip	q0,a2,16,q0,q6 	# [0*II+11]  id:812

.LBB232_dspi_dotprod_off_u8_aes3:	# 0x2b8
    st.qr	q0,a1,48                	# [0]  q0
    j	.Lt_0_29698                 	# [1]  

.LBB64_dspi_dotprod_off_u8_aes3:	# 0x2be
    movi.n	a10,32                 	# [0]  
    movi.n	a11,-16                	# [1]  
    l32i	a12,a1,64                	# [2]  gra_spill_temp_0
    ee.ld.128.usar.ip	q1,a15,16   	# [3]  id:813
    ee.ld.128.usar.ip	q2,a15,16   	# [4]  id:814
    sub	a12,a12,a5                	# [6]  
    addi	a12,a12,16               	# [7]  
    ld.qr	q0,a1,48                	# [8]  q0
    ee.src.q.ld.ip	q3,a15,16,q1,q2 	# [9]  id:815
    mov.n	a8,a15                  	# [10]  
    loopnez	a3,.LBB254_dspi_dotprod_off_u8_aes3 	# [11]  

.LBB252_dspi_dotprod_off_u8_aes3:	# 0x2dc
    ee.vmulas.u8.accx.ld.ip.qup	q0,a8,16,q0,q1,q2,q3 	# [0*II+0]  id:816
    ee.vmulas.u8.accx.ld.ip	q4,a2,16,q1,q6 	# [0*II+1]  id:817
    ee.vmulas.u8.accx.ld.ip.qup	q4,a8,16,q4,q2,q3,q0 	# [0*II+3]  id:818
    ee.vmulas.u8.accx.ld.ip	q1,a2,16,q2,q6 	# [0*II+4]  id:819
    ee.vmulas.u8.accx.ld.ip.qup	q1,a8,16,q1,q3,q0,q4 	# [0*II+6]  id:820
    ee.vmulas.u8.accx.ld.ip	q5,a2,16,q3,q6 	# [0*II+7]  id:821
    ee.vmulas.u8.accx.ld.ip.qup	q5,a8,16,q5,q0,q4,q1 	# [0*II+9]  id:822
    ee.vmulas.u8.accx.ld.ip	q0,a2,16,q0,q6 	# [0*II+10]  id:823
    ee.vmulas.u8.accx.ld.ip.qup	q0,a8,16,q0,q4,q1,q5 	# [0*II+12]  id:824
    ee.vmulas.u8.accx.ld.ip	q4,a2,16,q4,q6 	# [0*II+13]  id:825
    ee.vmulas.u8.accx.ld.xp.qup	q4,a8,a12,q4,q1,q5,q0 	# [0*II+15]  id:826
    ee.vmulas.u8.accx.ld.ip	q1,a2,16,q1,q6 	# [0*II+16]  id:827
    ee.vmulas.u8.accx.ld.xp.qup	q2,a8,a11,q1,q5,q0,q4 	# [0*II+18]  id:828
    ee.vmulas.u8.accx.ld.ip	q4,a2,16,q5,q6 	# [0*II+19]  id:829
    ee.ld.128.usar.xp	q1,a8,a10   	# [0*II+20]  id:830
    ee.vmulas.u8.accx.ld.ip.qup	q3,a8,16,q4,q0,q1,q2 	# [0*II+22]  id:831
    ee.vmulas.u8.accx.ld.ip	q0,a2,16,q0,q6 	# [0*II+23]  id:832

.LBB254_dspi_dotprod_off_u8_aes3:	# 0x31f
    movi.n	a2,0                   	# [0]  
    movi.n	a11,1                  	# [1]  
    addi.n	a12,a7,-1              	# [2]  
    rur.accx_0	a10                	# [3]  
    ssl	a12                       	# [4]  
    sll	a11,a11                   	# [5]  
    ssr	a7                        	# [6]  
    add.n	a10,a10,a11             	# [7]  
    sra	a10,a10                   	# [8]  
    s8i	a10,a4,0                  	# [9]  id:854
    retw.n                        	# [10]  

#endif // dsps_dotprod_s16_aes3_enabled