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
    .literal	.LC0_1_52, 458755

    .type	dspi_dotprod_u8_aes3, @function
    .align	 4
    .global	dspi_dotprod_u8_aes3
dspi_dotprod_u8_aes3:	# 0x4
.LBB1_dspi_dotprod_u8_aes3:	# 0x4
    entry	a1,48                   	#  
    l32i.n	a10,a2,4               	# [0]  id:669
    l32i.n	a11,a2,12              	# [1]  id:668
    mull	a8,a10,a5                	# [2]  
    blt	a11,a8,.LBB78_dspi_dotprod_u8_aes3 	# [4]  

    l32i.n	a12,a2,8               	# [0]  id:670
    l32i.n	a9,a2,16               	# [1]  id:671
    mull	a13,a12,a6               	# [2]  
    blt	a9,a13,.LBB78_dspi_dotprod_u8_aes3 	# [4]  

    l32i.n	a15,a3,4               	# [0]  id:673
    l32i.n	a14,a3,12              	# [1]  id:672
    mull	a13,a15,a5               	# [2]  
    blt	a14,a13,.LBB78_dspi_dotprod_u8_aes3 	# [4]  

    l32i.n	a8,a3,16               	# [0]  id:675
    l32i.n	a9,a3,8                	# [1]  id:674
    s32i.n	a9,a1,8                	# [2]  gra_spill_temp_2
    mull	a9,a9,a6                 	# [3]  
    blt	a8,a9,.LBB78_dspi_dotprod_u8_aes3 	# [5]  

    l32i.n	a8,a3,0                	# [0]  id:676
    s32i.n	a8,a1,4                	# [1]  gra_spill_temp_1
    bbsi	a8,0,.Lt_0_33026         	# [2]  

    bne	a14,a13,.Lt_0_33026       	# [0]  

    bnei	a15,1,.Lt_0_33026        	# [0]  

    l32i.n	a13,a1,8               	# [0]  gra_spill_temp_2
    beqi	a13,1,.Lt_0_17666        	# [2]  

.Lt_0_33026:	# 0x43
.Lt_0_17922:	# 0x43
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

.LBB78_dspi_dotprod_u8_aes3:	# 0x56
    l32r	a2,.LC0_1_52             	# [0]  
    retw.n                        	# [1]  

.Lt_0_17666:	# 0x5b
    addi.n	a14,a10,-1             	# [0]  
    bnez	a14,.Lt_0_33794          	# [1]  

    addi.n	a15,a12,-1             	# [0]  
    bnez	a15,.Lt_0_33794          	# [1]  

    extui	a8,a5,0,4               	# [0]  
    bnez.n	a8,.Lt_0_33794         	# [1]  

    blti	a6,4,.Lt_0_33794         	# [0]  

    movi.n	a9,64                  	# [0]  
    bge	a9,a5,.Lt_0_34306         	# [1]  

    extui	a10,a5,0,1              	# [0]  
    bnez	a10,.LBB28_dspi_dotprod_u8_aes3 	# [1]  

.Lt_0_34306:	# 0x78
.Lt_0_19714:	# 0x78
    mov.n	a3,a6                   	# [0]  
    addi	a13,a5,-48               	# [1]  
    movi.n	a14,0                  	# [2]  
    mull	a15,a11,a12              	# [3]  
    l32i.n	a2,a2,0                	# [4]  id:677
    s32i.n	a15,a1,0               	# [6]  gra_spill_temp_0
    wur.accx_0	a14                	# [7]  
    l32i.n	a15,a1,4               	# [8]  gra_spill_temp_1
    wur.accx_1	a14                	# [9]  
    ee.vld.128.ip	q0,a15,16       	# [10]  id:680
    beqz	a13,.LBB32_dspi_dotprod_u8_aes3 	# [11]  

.Lt_0_22786:	# 0x93
.Lt_0_22274:	# 0x93
    addi	a8,a5,-32                	# [0]  
    beqz	a8,.LBB38_dspi_dotprod_u8_aes3 	# [1]  

.Lt_0_24322:	# 0x99
.Lt_0_23810:	# 0x99
    addi	a9,a5,-16                	# [0]  
    beqz	a9,.LBB44_dspi_dotprod_u8_aes3 	# [1]  

.Lt_0_25858:	# 0x9f
.Lt_0_25346:	# 0x9f
    addi	a10,a5,-64               	# [0]  
    beqz	a10,.LBB50_dspi_dotprod_u8_aes3 	# [1]  

.Lt_0_27394:	# 0xa5
.Lt_0_26882:	# 0xa5
    addi	a11,a5,-128              	# [0]  
    beqz	a11,.LBB56_dspi_dotprod_u8_aes3 	# [1]  

    movi	a12,128                  	# [0]  
    bge	a12,a5,.Lt_0_30210        	# [1]  

    movi.n	a12,0                  	# [0]  
    ee.ld.128.usar.ip	q1,a2,16    	# [1]  id:752
    ee.ld.128.usar.ip	q2,a2,16    	# [2]  id:753
    ee.src.q.ld.ip	q3,a2,16,q1,q2 	# [4]  id:754
    beqz.n	a3,.Lt_0_30210         	# [5]  

    l32i.n	a14,a1,0               	# [0]  gra_spill_temp_0
    addi	a13,a5,31                	# [1]  
    movgez	a13,a5,a5              	# [2]  
    srai	a13,a13,5                	# [3]  
    sub	a14,a14,a5                	# [4]  
    addi	a14,a14,16               	# [5]  
    addi.n	a13,a13,-1             	# [6]  

.Lt_0_30978:	# 0xd1
    addi.n	a12,a12,1              	# [0]  
    movi.n	a8,32                  	# [1]  
    movi.n	a9,-16                 	# [2]  
    beqz.n	a13,.Lt_0_31234        	# [3]  

    loopnez	a13,.LBB218_dspi_dotprod_u8_aes3 	# [0]  

.LBB216_dspi_dotprod_u8_aes3:	# 0xdc
    ee.vld.128.ip	q5,a15,16       	# [0*II+0]  id:756
    ee.vmulas.u8.accx.ld.ip.qup	q4,a2,16,q0,q1,q2,q3 	# [0*II+1]  id:755
    ee.vld.128.ip	q0,a15,16       	# [0*II+2]  id:758
    ee.vmulas.u8.accx.ld.ip.qup	q1,a2,16,q5,q2,q3,q4 	# [0*II+3]  id:757
    ee.vld.128.ip	q5,a15,16       	# [0*II+4]  id:760
    ee.vmulas.u8.accx.ld.ip.qup	q2,a2,16,q0,q3,q4,q1 	# [0*II+5]  id:759
    ee.vld.128.ip	q0,a15,16       	# [0*II+6]  id:762
    ee.vmulas.u8.accx.ld.ip.qup	q3,a2,16,q5,q4,q1,q2 	# [0*II+7]  id:761

.LBB218_dspi_dotprod_u8_aes3:	# 0xf8

.Lt_0_31234:	# 0xf8
    ee.vmulas.u8.accx.ld.ip.qup	q5,a2,16,q0,q1,q2,q3 	# [0]  id:763
    ee.vld.128.ip	q0,a15,16       	# [1]  id:764
    ee.vld.128.ip	q6,a15,16       	# [2]  id:766
    ee.vmulas.u8.accx.ld.xp.qup	q7,a2,a14,q0,q2,q3,q5 	# [3]  id:765
    ee.vld.128.ip	q4,a15,16       	# [4]  id:769
    ee.vmulas.u8.accx.ld.xp.qup	q2,a2,a9,q6,q3,q5,q7 	# [5]  id:767
    ee.ld.128.usar.xp	q1,a2,a8    	# [6]  id:768
    ee.vld.128.ip	q0,a15,16       	# [7]  id:771
    ee.vmulas.u8.accx.ld.ip.qup	q3,a2,16,q4,q5,q1,q2 	# [8]  id:770
    bne	a12,a3,.Lt_0_30978        	# [9]  

.Lt_0_30210:	# 0x11a
.Lt_0_29954:	# 0x11a
    movi.n	a2,0                   	# [0]  
    rur.accx_0	a10                	# [1]  
    addi.n	a12,a7,-1              	# [2]  
    movi.n	a11,1                  	# [3]  
    ssl	a12                       	# [4]  
    sll	a11,a11                   	# [5]  
    ssr	a7                        	# [6]  
    add.n	a10,a10,a11             	# [7]  
    srl	a10,a10                   	# [8]  
    s8i	a10,a4,0                  	# [9]  id:773
    retw.n                        	# [10]  

.Lt_0_33794:	# 0x136
.Lt_0_18946:	# 0x136
    mov.n	a10,a2                  	# [0]  
    mov.n	a11,a3                  	# [1]  
    mov.n	a12,a4                  	# [2]  
    mov.n	a13,a5                  	# [3]  
    mov.n	a14,a6                  	# [4]  
    mov.n	a15,a7                  	# [5]  
    call8	dspi_dotprod_u8_ansi    	# [6]  dspi_dotprod_u8_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  

.LBB32_dspi_dotprod_u8_aes3:	# 0x149
    ee.ld.128.usar.ip	q1,a2,16    	# [0]  id:681
    ee.ld.128.usar.ip	q2,a2,16    	# [1]  id:682
    ee.src.q.ld.ip	q3,a2,16,q1,q2 	# [3]  id:683
    beqz.n	a6,.Lt_0_22786         	# [4]  

    movi.n	a10,32                 	# [0]  
    l32i.n	a12,a1,0               	# [1]  gra_spill_temp_0
    movi.n	a11,-16                	# [2]  
    addi	a12,a12,-32              	# [3]  
    loopgtz	a6,.LBB104_dspi_dotprod_u8_aes3 	# [4]  

.LBB102_dspi_dotprod_u8_aes3:	# 0x160
    ee.vld.128.ip	q4,a15,16       	# [0*II+0]  id:685
    ee.vmulas.u8.accx.ld.xp.qup	q1,a2,a12,q0,q1,q2,q3 	# [0*II+1]  id:684
    ee.vld.128.ip	q5,a15,16       	# [0*II+2]  id:687
    ee.vmulas.u8.accx.ld.xp.qup	q2,a2,a11,q4,q2,q3,q1 	# [0*II+3]  id:686
    ee.ld.128.usar.xp	q1,a2,a10   	# [0*II+4]  id:688
    ee.vld.128.ip	q0,a15,16       	# [0*II+5]  id:690
    ee.vmulas.u8.accx.ld.ip.qup	q3,a2,16,q5,q3,q1,q2 	# [0*II+6]  id:689

.LBB104_dspi_dotprod_u8_aes3:	# 0x178
    j	.Lt_0_22786                 	# [0]  

.LBB38_dspi_dotprod_u8_aes3:	# 0x17b
    movi.n	a10,32                 	# [0]  
    movi.n	a11,-16                	# [1]  
    srli	a3,a6,1                  	# [2]  
    l32i.n	a12,a1,0               	# [3]  gra_spill_temp_0
    ee.ld.128.usar.ip	q1,a2,16    	# [4]  id:691
    ee.ld.128.usar.ip	q2,a2,16    	# [5]  id:692
    addi	a12,a12,-16              	# [7]  
    ee.src.q.ld.xp	q3,a2,a12,q1,q2 	# [8]  id:693
    loopnez	a3,.LBB127_dspi_dotprod_u8_aes3 	# [9]  

.LBB125_dspi_dotprod_u8_aes3:	# 0x193
    ee.vld.128.ip	q4,a15,16       	# [0*II+0]  id:695
    ee.vmulas.u8.accx.ld.xp.qup	q3,a2,a11,q0,q1,q2,q3 	# [0*II+1]  id:694
    ee.ld.128.usar.xp	q1,a2,a10   	# [0*II+2]  id:696
    ee.vld.128.ip	q0,a15,16       	# [0*II+3]  id:698
    ee.vmulas.u8.accx.ld.xp.qup	q4,a2,a12,q4,q2,q1,q3 	# [0*II+4]  id:697
    ee.vld.128.ip	q5,a15,16       	# [0*II+5]  id:700
    ee.vmulas.u8.accx.ld.xp.qup	q2,a2,a11,q0,q1,q3,q4 	# [0*II+6]  id:699
    ee.ld.128.usar.xp	q1,a2,a10   	# [0*II+7]  id:701
    ee.vld.128.ip	q0,a15,16       	# [0*II+8]  id:703
    ee.vmulas.u8.accx.ld.xp.qup	q3,a2,a12,q5,q3,q1,q2 	# [0*II+9]  id:702

.LBB127_dspi_dotprod_u8_aes3:	# 0x1b5
    j	.Lt_0_24322                 	# [0]  

.LBB44_dspi_dotprod_u8_aes3:	# 0x1b8
    srli	a3,a3,2                  	# [0]  
    movi.n	a10,-16                	# [1]  
    l32i.n	a11,a1,0               	# [2]  gra_spill_temp_0
    addi	a8,a2,16                 	# [3]  
    addi	a11,a11,16               	# [4]  
    ee.ld.128.usar.xp	q2,a8,a10   	# [5]  id:704
    ee.ld.128.usar.xp	q1,a8,a11   	# [6]  id:705
    ee.src.q.ld.xp	q3,a8,a10,q1,q2 	# [8]  id:706
    ee.ld.128.usar.xp	q2,a8,a11   	# [9]  id:707
    loopnez	a3,.LBB150_dspi_dotprod_u8_aes3 	# [10]  

.LBB148_dspi_dotprod_u8_aes3:	# 0x1d4
    ee.vld.128.ip	q4,a15,16       	# [0*II+0]  id:709
    ee.vmulas.u8.accx.ld.xp.qup	q3,a8,a10,q0,q1,q2,q3 	# [0*II+1]  id:708
    ee.ld.128.usar.xp	q1,a8,a11   	# [0*II+2]  id:710
    ee.vld.128.ip	q0,a15,16       	# [0*II+3]  id:712
    ee.vmulas.u8.accx.ld.xp.qup	q4,a8,a10,q4,q2,q1,q3 	# [0*II+4]  id:711
    ee.ld.128.usar.xp	q3,a8,a11   	# [0*II+5]  id:713
    ee.vld.128.ip	q5,a15,16       	# [0*II+6]  id:715
    ee.vmulas.u8.accx.ld.xp.qup	q4,a8,a10,q0,q1,q3,q4 	# [0*II+7]  id:714
    ee.ld.128.usar.xp	q1,a8,a11   	# [0*II+8]  id:716
    ee.vld.128.ip	q0,a15,16       	# [0*II+9]  id:718
    ee.vmulas.u8.accx.ld.xp.qup	q3,a8,a10,q5,q3,q1,q4 	# [0*II+10]  id:717
    ee.ld.128.usar.xp	q2,a8,a11   	# [0*II+11]  id:719

.LBB150_dspi_dotprod_u8_aes3:	# 0x1fc
    mov.n	a2,a8                   	# [0]  
    j	.Lt_0_25858                 	# [1]  

.LBB50_dspi_dotprod_u8_aes3:	# 0x201
    movi.n	a10,32                 	# [0]  
    movi.n	a11,-16                	# [1]  
    l32i.n	a12,a1,0               	# [2]  gra_spill_temp_0
    ee.ld.128.usar.ip	q1,a2,16    	# [3]  id:720
    ee.ld.128.usar.ip	q2,a2,16    	# [4]  id:721
    sub	a12,a12,a5                	# [5]  
    ee.src.q.ld.ip	q3,a2,16,q1,q2 	# [7]  id:722
    addi	a12,a12,16               	# [8]  
    loopnez	a3,.LBB173_dspi_dotprod_u8_aes3 	# [9]  

.LBB171_dspi_dotprod_u8_aes3:	# 0x219
    ee.vld.128.ip	q5,a15,16       	# [0*II+0]  id:724
    ee.vmulas.u8.accx.ld.ip.qup	q4,a2,16,q0,q1,q2,q3 	# [0*II+1]  id:723
    ee.vld.128.ip	q1,a15,16       	# [0*II+2]  id:726
    ee.vmulas.u8.accx.ld.xp.qup	q0,a2,a12,q5,q2,q3,q4 	# [0*II+3]  id:725
    ee.vld.128.ip	q5,a15,16       	# [0*II+4]  id:729
    ee.vmulas.u8.accx.ld.xp.qup	q2,a2,a11,q1,q3,q4,q0 	# [0*II+5]  id:727
    ee.ld.128.usar.xp	q1,a2,a10   	# [0*II+6]  id:728
    ee.vld.128.ip	q0,a15,16       	# [0*II+7]  id:731
    ee.vmulas.u8.accx.ld.ip.qup	q3,a2,16,q5,q4,q1,q2 	# [0*II+8]  id:730

.LBB173_dspi_dotprod_u8_aes3:	# 0x238
    j	.Lt_0_27394                 	# [0]  

.LBB56_dspi_dotprod_u8_aes3:	# 0x23b
    movi.n	a10,32                 	# [0]  
    movi.n	a11,-16                	# [1]  
    l32i.n	a12,a1,0               	# [2]  gra_spill_temp_0
    ee.ld.128.usar.ip	q1,a2,16    	# [3]  id:732
    ee.ld.128.usar.ip	q2,a2,16    	# [4]  id:733
    sub	a12,a12,a5                	# [6]  
    addi	a12,a12,16               	# [7]  
    ee.src.q.ld.ip	q3,a2,16,q1,q2 	# [8]  id:734
    loopnez	a3,.LBB195_dspi_dotprod_u8_aes3 	# [9]  

.LBB193_dspi_dotprod_u8_aes3:	# 0x253
    ee.vld.128.ip	q4,a15,16       	# [0*II+0]  id:736
    ee.vmulas.u8.accx.ld.ip.qup	q1,a2,16,q0,q1,q2,q3 	# [0*II+1]  id:735
    ee.vld.128.ip	q0,a15,16       	# [0*II+2]  id:738
    ee.vmulas.u8.accx.ld.ip.qup	q4,a2,16,q4,q2,q3,q1 	# [0*II+3]  id:737
    ee.vld.128.ip	q5,a15,16       	# [0*II+4]  id:740
    ee.vmulas.u8.accx.ld.ip.qup	q0,a2,16,q0,q3,q1,q4 	# [0*II+5]  id:739
    ee.vld.128.ip	q6,a15,16       	# [0*II+6]  id:742
    ee.vmulas.u8.accx.ld.ip.qup	q1,a2,16,q5,q1,q4,q0 	# [0*II+7]  id:741
    ee.vld.128.ip	q5,a15,16       	# [0*II+8]  id:744
    ee.vmulas.u8.accx.ld.ip.qup	q4,a2,16,q6,q4,q0,q1 	# [0*II+9]  id:743
    ee.vld.128.ip	q6,a15,16       	# [0*II+10]  id:746
    ee.vmulas.u8.accx.ld.xp.qup	q0,a2,a12,q5,q0,q1,q4 	# [0*II+11]  id:745
    ee.vld.128.ip	q5,a15,16       	# [0*II+12]  id:749
    ee.vmulas.u8.accx.ld.xp.qup	q2,a2,a11,q6,q1,q4,q0 	# [0*II+13]  id:747
    ee.ld.128.usar.xp	q1,a2,a10   	# [0*II+14]  id:748
    ee.vld.128.ip	q0,a15,16       	# [0*II+15]  id:751
    ee.vmulas.u8.accx.ld.ip.qup	q3,a2,16,q5,q4,q1,q2 	# [0*II+16]  id:750

.LBB195_dspi_dotprod_u8_aes3:	# 0x28e
    movi.n	a2,0                   	# [0]  
    movi.n	a11,1                  	# [1]  
    addi.n	a12,a7,-1              	# [2]  
    rur.accx_0	a10                	# [3]  
    ssl	a12                       	# [4]  
    sll	a11,a11                   	# [5]  
    ssr	a7                        	# [6]  
    add.n	a10,a10,a11             	# [7]  
    srl	a10,a10                   	# [8]  
    s8i	a10,a4,0                  	# [9]  id:773
    retw.n                        	# [10]  

.LBB28_dspi_dotprod_u8_aes3:	# 0x2aa
    mov.n	a15,a7                  	# [0]  
    mov.n	a14,a6                  	# [1]  
    mov.n	a13,a5                  	# [2]  
    mov.n	a12,a4                  	# [3]  
    mov.n	a11,a3                  	# [4]  
    mov.n	a10,a2                  	# [5]  
    call8	dspi_dotprod_u8_ansi    	# [6]  dspi_dotprod_u8_ansi

    mov.n	a2,a10                  	# [0]  
    retw.n                        	# [1]  


#endif // dsps_dotprod_s16_aes3_enabled