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
    .literal	.LC0_1_53, 458755

    # Program Unit: dspi_dotprod_s16_aes3
    .type	dspi_dotprod_s16_aes3, @function
    .align	 4
    .global	dspi_dotprod_s16_aes3
dspi_dotprod_s16_aes3:	# 0x4
.LBB1_dspi_dotprod_s16_aes3:	# 0x4
    entry	a1,64                   	#  
    l32i.n	a10,a2,4               	# [0]  id:678
    l32i.n	a11,a2,12              	# [1]  id:677
    mull	a8,a10,a5                	# [2]  
    blt	a11,a8,.LBB81_dspi_dotprod_s16_aes3 	# [4]  

    l32i.n	a12,a2,8               	# [0]  id:679
    l32i.n	a9,a2,16               	# [1]  id:680
    mull	a13,a12,a6               	# [2]  
    blt	a9,a13,.LBB81_dspi_dotprod_s16_aes3 	# [4]  

    l32i.n	a15,a3,4               	# [0]  id:682
    l32i.n	a14,a3,12              	# [1]  id:681
    mull	a13,a15,a5               	# [2]  
    blt	a14,a13,.LBB81_dspi_dotprod_s16_aes3 	# [4]  

    l32i.n	a8,a3,16               	# [0]  id:684
    l32i.n	a9,a3,8                	# [1]  id:683
    s32i.n	a9,a1,24               	# [2]  gra_spill_temp_2
    mull	a9,a9,a6                 	# [3]  
    blt	a8,a9,.LBB81_dspi_dotprod_s16_aes3 	# [5]  

    l32i.n	a8,a3,0                	# [0]  id:685
    s32i.n	a8,a1,20               	# [1]  gra_spill_temp_1
    bbsi	a8,0,.Lt_0_34050         	# [2]  

    bne	a14,a13,.Lt_0_34050       	# [0]  

    bnei	a15,1,.Lt_0_34050        	# [0]  

    l32i.n	a9,a1,24               	# [0]  gra_spill_temp_2
    beqi	a9,1,.Lt_0_18178         	# [2]  

.Lt_0_34050:	# 0x43
.Lt_0_18434:	# 0x43
    mov.n	a10,a2                  	# [0]  
    mov.n	a11,a3                  	# [1]  
    mov.n	a12,a4                  	# [2]  
    mov.n	a13,a5                  	# [3]  
    mov.n	a14,a6                  	# [4]  
    mov.n	a15,a7                  	# [5]  
    .type	dspi_dotprod_s16_ansi, @function
    call8	dspi_dotprod_s16_ansi   	# [6]  dspi_dotprod_s16_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  

.LBB81_dspi_dotprod_s16_aes3:	# 0x56
    l32r	a2,.LC0_1_53             	# [0]  
    retw.n                        	# [1]  

.Lt_0_18178:	# 0x5b
    addi.n	a13,a10,-1             	# [0]  
    bnez	a13,.Lt_0_34818          	# [1]  

    addi.n	a14,a12,-1             	# [0]  
    bnez	a14,.Lt_0_34818          	# [1]  

    extui	a15,a5,0,3              	# [0]  
    bnez.n	a15,.Lt_0_34818        	# [1]  

    blti	a6,4,.Lt_0_34818         	# [0]  

    movi.n	a8,32                  	# [0]  
    bge	a8,a5,.Lt_0_35330         	# [1]  

    extui	a9,a5,0,1               	# [0]  
    bnez	a9,.LBB28_dspi_dotprod_s16_aes3 	# [1]  

.Lt_0_35330:	# 0x78
.Lt_0_20226:	# 0x78
    mov.n	a3,a6                   	# [0]  
    addi	a10,a5,-24               	# [1]  
    mull	a13,a11,a12              	# [2]  
    l32i.n	a15,a1,20              	# [3]  gra_spill_temp_1
    l32i.n	a2,a2,0                	# [4]  id:686
    movi.n	a14,0                  	# [5]  
    wur.sar_byte	a14              	# [6]  
    wur.accx_0	a14                	# [8]  
    wur.accx_1	a14                	# [9]  
    ee.vld.128.ip	q0,a15,16       	# [10]  id:690
    slli	a13,a13,1                	# [11]  
    s32i.n	a13,a1,16              	# [12]  gra_spill_temp_0
    beqz	a10,.LBB32_dspi_dotprod_s16_aes3 	# [13]  

.Lt_0_23298:	# 0x99
.Lt_0_22786:	# 0x99
    addi	a8,a5,-16                	# [0]  
    beqz	a8,.LBB38_dspi_dotprod_s16_aes3 	# [1]  

.Lt_0_24834:	# 0x9f
.Lt_0_24322:	# 0x9f
    addi	a9,a5,-8                 	# [0]  
    beqz	a9,.LBB44_dspi_dotprod_s16_aes3 	# [1]  

.Lt_0_26370:	# 0xa5
.Lt_0_25858:	# 0xa5
    addi	a10,a5,-32               	# [0]  
    beqz	a10,.LBB50_dspi_dotprod_s16_aes3 	# [1]  

.Lt_0_27906:	# 0xab
.Lt_0_27394:	# 0xab
    addi	a11,a5,-64               	# [0]  
    beqz	a11,.LBB56_dspi_dotprod_s16_aes3 	# [1]  

    movi.n	a12,64                 	# [0]  
    bge	a12,a5,.Lt_0_30722        	# [1]  

    movi.n	a12,0                  	# [0]  
    ee.ld.128.usar.ip	q1,a2,16    	# [1]  id:762
    ee.ld.128.usar.ip	q2,a2,16    	# [2]  id:763
    ee.src.q.ld.ip	q3,a2,16,q1,q2 	# [4]  id:764
    beqz.n	a3,.Lt_0_30722         	# [5]  

    slli	a8,a5,1                  	# [0]  
    l32i.n	a14,a1,16              	# [1]  gra_spill_temp_0
    addi	a13,a5,31                	# [2]  
    movgez	a13,a5,a5              	# [3]  
    srai	a13,a13,5                	# [4]  
    sub	a14,a14,a8                	# [5]  
    addi	a14,a14,16               	# [6]  
    addi.n	a13,a13,-1             	# [7]  

.Lt_0_31490:	# 0xd9
    addi.n	a12,a12,1              	# [0]  
    movi.n	a9,32                  	# [1]  
    beqz.n	a13,.Lt_0_31746        	# [2]  

    loopnez	a13,.LBB221_dspi_dotprod_s16_aes3 	# [0]  

.LBB219_dspi_dotprod_s16_aes3:	# 0xe2
    ee.vld.128.ip	q5,a15,16       	# [0*II+0]  id:766
    ee.vmulas.s16.accx.ld.ip.qup	q4,a2,16,q0,q1,q2,q3 	# [0*II+1]  id:765
    ee.vld.128.ip	q0,a15,16       	# [0*II+2]  id:768
    ee.vmulas.s16.accx.ld.ip.qup	q1,a2,16,q5,q2,q3,q4 	# [0*II+3]  id:767
    ee.vld.128.ip	q5,a15,16       	# [0*II+4]  id:770
    ee.vmulas.s16.accx.ld.ip.qup	q2,a2,16,q0,q3,q4,q1 	# [0*II+5]  id:769
    ee.vld.128.ip	q0,a15,16       	# [0*II+6]  id:772
    ee.vmulas.s16.accx.ld.ip.qup	q3,a2,16,q5,q4,q1,q2 	# [0*II+7]  id:771

.LBB221_dspi_dotprod_s16_aes3:	# 0xfe

.Lt_0_31746:	# 0xfe
    ee.vmulas.s16.accx.ld.ip.qup	q5,a2,16,q0,q1,q2,q3 	# [0]  id:773
    movi.n	a10,-16                	# [1]  
    ee.vld.128.ip	q0,a15,16       	# [2]  id:774
    ee.vld.128.ip	q6,a15,16       	# [3]  id:776
    ee.vmulas.s16.accx.ld.xp.qup	q7,a2,a14,q0,q2,q3,q5 	# [4]  id:775
    ee.vld.128.ip	q4,a15,16       	# [5]  id:779
    ee.vmulas.s16.accx.ld.xp.qup	q2,a2,a10,q6,q3,q5,q7 	# [6]  id:777
    ee.ld.128.usar.xp	q1,a2,a9    	# [7]  id:778
    ee.vld.128.ip	q0,a15,16       	# [8]  id:781
    ee.vmulas.s16.accx.ld.ip.qup	q3,a2,16,q4,q5,q1,q2 	# [9]  id:780
    bne	a12,a3,.Lt_0_31490        	# [10]  

.Lt_0_30722:	# 0x122
.Lt_0_30466:	# 0x122
    rur.accx_0	a9                 	# [0]  
    rur.accx_1	a10                	# [1]  
    blti	a7,1,.Lt_0_33282         	# [2]  

    movi.n	a2,0                   	# [0]  
    addi	a13,a7,-33               	# [1]  
    addi.n	a14,a7,-1              	# [2]  
    ssr	a14                       	# [3]  
    sra	a12,a10                   	# [4]  
    src	a11,a10,a9                	# [5]  
    movgez	a11,a12,a13            	# [6]  
    addi.n	a11,a11,1              	# [7]  
    srai	a11,a11,1                	# [8]  
    s16i	a11,a4,0                 	# [9]  id:787
    retw.n                        	# [10]  

.Lt_0_34818:	# 0x148
.Lt_0_19458:	# 0x148
    mov.n	a10,a2                  	# [0]  
    mov.n	a11,a3                  	# [1]  
    mov.n	a12,a4                  	# [2]  
    mov.n	a13,a5                  	# [3]  
    mov.n	a14,a6                  	# [4]  
    mov.n	a15,a7                  	# [5]  
    call8	dspi_dotprod_s16_ansi   	# [6]  dspi_dotprod_s16_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  

.LBB32_dspi_dotprod_s16_aes3:	# 0x15b
    ee.ld.128.usar.ip	q1,a2,16    	# [0]  id:691
    ee.ld.128.usar.ip	q2,a2,16    	# [1]  id:692
    ee.src.q.ld.ip	q3,a2,16,q1,q2 	# [3]  id:693
    beqz.n	a6,.Lt_0_23298         	# [4]  

    addi	a12,a13,-32              	# [0]  
    movi.n	a10,32                 	# [1]  
    movi.n	a11,-16                	# [2]  
    loopgtz	a6,.LBB107_dspi_dotprod_s16_aes3 	# [3]  

.LBB105_dspi_dotprod_s16_aes3:	# 0x170
    ee.vld.128.ip	q4,a15,16       	# [0*II+0]  id:695
    ee.vmulas.s16.accx.ld.xp.qup	q1,a2,a12,q0,q1,q2,q3 	# [0*II+1]  id:694
    ee.vld.128.ip	q5,a15,16       	# [0*II+2]  id:697
    ee.vmulas.s16.accx.ld.xp.qup	q2,a2,a11,q4,q2,q3,q1 	# [0*II+3]  id:696
    ee.ld.128.usar.xp	q1,a2,a10   	# [0*II+4]  id:698
    ee.vld.128.ip	q0,a15,16       	# [0*II+5]  id:700
    ee.vmulas.s16.accx.ld.ip.qup	q3,a2,16,q5,q3,q1,q2 	# [0*II+6]  id:699

.LBB107_dspi_dotprod_s16_aes3:	# 0x188
    j	.Lt_0_23298                 	# [0]  

.LBB38_dspi_dotprod_s16_aes3:	# 0x18b
    movi.n	a10,32                 	# [0]  
    movi.n	a11,-16                	# [1]  
    srli	a3,a6,1                  	# [2]  
    l32i.n	a12,a1,16              	# [3]  gra_spill_temp_0
    ee.ld.128.usar.ip	q1,a2,16    	# [4]  id:701
    ee.ld.128.usar.ip	q2,a2,16    	# [5]  id:702
    addi	a12,a12,-16              	# [7]  
    ee.src.q.ld.xp	q3,a2,a12,q1,q2 	# [8]  id:703
    loopnez	a3,.LBB130_dspi_dotprod_s16_aes3 	# [9]  

.LBB128_dspi_dotprod_s16_aes3:	# 0x1a3
    ee.vld.128.ip	q4,a15,16       	# [0*II+0]  id:705
    ee.vmulas.s16.accx.ld.xp.qup	q3,a2,a11,q0,q1,q2,q3 	# [0*II+1]  id:704
    ee.ld.128.usar.xp	q1,a2,a10   	# [0*II+2]  id:706
    ee.vld.128.ip	q0,a15,16       	# [0*II+3]  id:708
    ee.vmulas.s16.accx.ld.xp.qup	q4,a2,a12,q4,q2,q1,q3 	# [0*II+4]  id:707
    ee.vld.128.ip	q5,a15,16       	# [0*II+5]  id:710
    ee.vmulas.s16.accx.ld.xp.qup	q2,a2,a11,q0,q1,q3,q4 	# [0*II+6]  id:709
    ee.ld.128.usar.xp	q1,a2,a10   	# [0*II+7]  id:711
    ee.vld.128.ip	q0,a15,16       	# [0*II+8]  id:713
    ee.vmulas.s16.accx.ld.xp.qup	q3,a2,a12,q5,q3,q1,q2 	# [0*II+9]  id:712

.LBB130_dspi_dotprod_s16_aes3:	# 0x1c5
    j	.Lt_0_24834                 	# [0]  

.LBB44_dspi_dotprod_s16_aes3:	# 0x1c8
    srli	a3,a3,2                  	# [0]  
    movi.n	a10,-16                	# [1]  
    l32i.n	a11,a1,16              	# [2]  gra_spill_temp_0
    addi	a8,a2,16                 	# [3]  
    addi	a11,a11,16               	# [4]  
    ee.ld.128.usar.xp	q2,a8,a10   	# [5]  id:714
    ee.ld.128.usar.xp	q1,a8,a11   	# [6]  id:715
    ee.src.q.ld.xp	q3,a8,a10,q1,q2 	# [8]  id:716
    ee.ld.128.usar.xp	q2,a8,a11   	# [9]  id:717
    loopnez	a3,.LBB153_dspi_dotprod_s16_aes3 	# [10]  

.LBB151_dspi_dotprod_s16_aes3:	# 0x1e4
    ee.vld.128.ip	q4,a15,16       	# [0*II+0]  id:719
    ee.vmulas.s16.accx.ld.xp.qup	q3,a8,a10,q0,q1,q2,q3 	# [0*II+1]  id:718
    ee.ld.128.usar.xp	q1,a8,a11   	# [0*II+2]  id:720
    ee.vld.128.ip	q0,a15,16       	# [0*II+3]  id:722
    ee.vmulas.s16.accx.ld.xp.qup	q4,a8,a10,q4,q2,q1,q3 	# [0*II+4]  id:721
    ee.ld.128.usar.xp	q3,a8,a11   	# [0*II+5]  id:723
    ee.vld.128.ip	q5,a15,16       	# [0*II+6]  id:725
    ee.vmulas.s16.accx.ld.xp.qup	q4,a8,a10,q0,q1,q3,q4 	# [0*II+7]  id:724
    ee.ld.128.usar.xp	q1,a8,a11   	# [0*II+8]  id:726
    ee.vld.128.ip	q0,a15,16       	# [0*II+9]  id:728
    ee.vmulas.s16.accx.ld.xp.qup	q3,a8,a10,q5,q3,q1,q4 	# [0*II+10]  id:727
    ee.ld.128.usar.xp	q2,a8,a11   	# [0*II+11]  id:729

.LBB153_dspi_dotprod_s16_aes3:	# 0x20c
    mov.n	a2,a8                   	# [0]  
    j	.Lt_0_26370                 	# [1]  

.LBB50_dspi_dotprod_s16_aes3:	# 0x211
    movi.n	a10,32                 	# [0]  
    movi.n	a11,-16                	# [1]  
    slli	a13,a5,1                 	# [2]  
    l32i.n	a12,a1,16              	# [3]  gra_spill_temp_0
    ee.ld.128.usar.ip	q1,a2,16    	# [4]  id:730
    ee.ld.128.usar.ip	q2,a2,16    	# [5]  id:731
    sub	a12,a12,a13               	# [6]  
    ee.src.q.ld.ip	q3,a2,16,q1,q2 	# [8]  id:732
    addi	a12,a12,16               	# [9]  
    loopnez	a3,.LBB176_dspi_dotprod_s16_aes3 	# [10]  

.LBB174_dspi_dotprod_s16_aes3:	# 0x22c
    ee.vld.128.ip	q5,a15,16       	# [0*II+0]  id:734
    ee.vmulas.s16.accx.ld.ip.qup	q4,a2,16,q0,q1,q2,q3 	# [0*II+1]  id:733
    ee.vld.128.ip	q1,a15,16       	# [0*II+2]  id:736
    ee.vmulas.s16.accx.ld.xp.qup	q0,a2,a12,q5,q2,q3,q4 	# [0*II+3]  id:735
    ee.vld.128.ip	q5,a15,16       	# [0*II+4]  id:739
    ee.vmulas.s16.accx.ld.xp.qup	q2,a2,a11,q1,q3,q4,q0 	# [0*II+5]  id:737
    ee.ld.128.usar.xp	q1,a2,a10   	# [0*II+6]  id:738
    ee.vld.128.ip	q0,a15,16       	# [0*II+7]  id:741
    ee.vmulas.s16.accx.ld.ip.qup	q3,a2,16,q5,q4,q1,q2 	# [0*II+8]  id:740

.LBB176_dspi_dotprod_s16_aes3:	# 0x24b
    j	.Lt_0_27906                 	# [0]  

.LBB56_dspi_dotprod_s16_aes3:	# 0x24e
    movi.n	a10,32                 	# [0]  
    movi.n	a11,-16                	# [1]  
    slli	a13,a5,1                 	# [2]  
    l32i.n	a12,a1,16              	# [3]  gra_spill_temp_0
    ee.ld.128.usar.ip	q1,a2,16    	# [4]  id:742
    ee.ld.128.usar.ip	q2,a2,16    	# [5]  id:743
    sub	a12,a12,a13               	# [7]  
    addi	a12,a12,16               	# [8]  
    ee.src.q.ld.ip	q3,a2,16,q1,q2 	# [9]  id:744
    loopnez	a3,.LBB198_dspi_dotprod_s16_aes3 	# [10]  

.LBB196_dspi_dotprod_s16_aes3:	# 0x269
    ee.vld.128.ip	q4,a15,16       	# [0*II+0]  id:746
    ee.vmulas.s16.accx.ld.ip.qup	q1,a2,16,q0,q1,q2,q3 	# [0*II+1]  id:745
    ee.vld.128.ip	q0,a15,16       	# [0*II+2]  id:748
    ee.vmulas.s16.accx.ld.ip.qup	q4,a2,16,q4,q2,q3,q1 	# [0*II+3]  id:747
    ee.vld.128.ip	q5,a15,16       	# [0*II+4]  id:750
    ee.vmulas.s16.accx.ld.ip.qup	q0,a2,16,q0,q3,q1,q4 	# [0*II+5]  id:749
    ee.vld.128.ip	q6,a15,16       	# [0*II+6]  id:752
    ee.vmulas.s16.accx.ld.ip.qup	q1,a2,16,q5,q1,q4,q0 	# [0*II+7]  id:751
    ee.vld.128.ip	q5,a15,16       	# [0*II+8]  id:754
    ee.vmulas.s16.accx.ld.ip.qup	q4,a2,16,q6,q4,q0,q1 	# [0*II+9]  id:753
    ee.vld.128.ip	q6,a15,16       	# [0*II+10]  id:756
    ee.vmulas.s16.accx.ld.xp.qup	q0,a2,a12,q5,q0,q1,q4 	# [0*II+11]  id:755
    ee.vld.128.ip	q5,a15,16       	# [0*II+12]  id:759
    ee.vmulas.s16.accx.ld.xp.qup	q2,a2,a11,q6,q1,q4,q0 	# [0*II+13]  id:757
    ee.ld.128.usar.xp	q1,a2,a10   	# [0*II+14]  id:758
    ee.vld.128.ip	q0,a15,16       	# [0*II+15]  id:761
    ee.vmulas.s16.accx.ld.ip.qup	q3,a2,16,q5,q4,q1,q2 	# [0*II+16]  id:760

.LBB198_dspi_dotprod_s16_aes3:	# 0x2a4
    j	.Lt_0_30722                 	# [0]  

.Lt_0_33282:	# 0x2a7
    movi.n	a2,0                   	# [0]  
    sext	a14,a9,15                	# [1]  
    s16i	a14,a4,0                 	# [2]  id:788
    retw.n                        	# [3]  

.LBB28_dspi_dotprod_s16_aes3:	# 0x2b1
    mov.n	a15,a7                  	# [0]  
    mov.n	a14,a6                  	# [1]  
    mov.n	a13,a5                  	# [2]  
    mov.n	a12,a4                  	# [3]  
    mov.n	a11,a3                  	# [4]  
    mov.n	a10,a2                  	# [5]  
    call8	dspi_dotprod_s16_ansi   	# [6]  dspi_dotprod_s16_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  


#endif // dsps_dotprod_s16_aes3_enabled