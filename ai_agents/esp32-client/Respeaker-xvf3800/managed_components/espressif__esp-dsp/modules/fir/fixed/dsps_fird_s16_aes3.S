/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsps_fir_platform.h"
 
// This is FIR filter for ESP32s3 processor.
	.text
	.align  4
	.global dsps_fird_s16_aes3
	.type   dsps_fird_s16_aes3,@function
// The function implements the following C code:
// int32_t dsps_fird_s16_aes3(fir_s16_t *fir, const int16_t *input, int16_t *output, int32_t len)

#if (dsps_fird_s16_aes3_enabled == 1)

dsps_fird_s16_aes3: 
// Input params					      Variables
//
// fir      - a2                                N			- a7
// input    - a3				            coeffs	      - a8
// output   - a4				            delay		      - a9
// len      - a5				            rounding          - a10
//								final shift	      - a11       (div_24 constant)
//								fir_pos     	- a12
//								decim		      - a13

      // load inputs
      entry	      a1, 64
      l16si	      a7, a2, 8                                             // a7  N
      l16si 	a13, a2, 12                                           // a13  decim
      l32i.n  	a8, a2, 0                                             // a8  coeffs
      l32i.n  	a9, a2, 4                                             // a9  delay
      l16si	      a12, a2, 10                                           // a12 fir_pos
      l16si	      a6, a2, 14                                            // a6 d_pos

      // check decimation and delay line length
      movi        a15, 0xF                                              // modulo 16 mask
      bnone       a13, a15, _length_16_check                            // jump if decim is divisible by 16
      srli        a14, a15, 1                                           // modulo 8 mask
      bnone       a13, a14, _decim_8_dpos_check                         // jump to start_pos check if decim is divisible by 8
      srli        a15, a14, 1                                           // modulo 4 mask
      bnone       a13, a15, _decim_4_dpos_check                         // jump to start_pos check if decim is divisible by 4
      srli        a14, a15, 1                                           // modulo 2 mask
      bnone       a13, a14, _decim_2_dpos_check                         // jump to start_pos check if decim is divisible by 2
      j           _other_decim                                          // jump to other decimations

      // check start_pos and delay line length for the largest decim as decim_16
      _length_16_check:
      l16si	      a11, a2,   16                                         // get shift value   
      addi        a14, a11, -15                                         // apply 16-bit final shift
      neg         a11, a14                                              // negate final_shift
      bltz        a11, _decim_8_dpos_check                              // jump if the final shift is to right
      bany        a7,  a15, _decim_8_dpos_check                         // jump if fir_len (N) is not divisible by 16, but divisible by 8
      beqz.n      a6, _decim_16_len_16                                  // jump if start_pos is 0
      bnone       a6, a15, _decim_16_len_16                             // jump to _decim_16_len_16 if start_pos is divisible by 16
      j           _decim_8_dpos_check

      // check start_pos for the largest decim as decim_8
      _decim_8_dpos_check:
      movi        a14, 0x7                                              // modulo 8 mask
      beqz.n      a6, _decim_8                                          // jump to decim_8 if start_pos is 0
      bnone       a6, a14, _decim_8                                     // jump to decim_8 if start_pos is divisible by 8
      srli        a15, a14, 1                                           // modulo 4 mask
      bnone       a6, a15, _decim_4                                     // jump to decim_4 if start_pos is divisible by 4
      srli        a14, a15, 1                                           // modulo 2 mask
      bnone       a6, a14, _decim_2                                     // jump to decim_2 if start_pos is divisible by 2
      j           _other_decim                                          // jump to other decim, if start_pos is odd number

      // check start_pos for the largest decim as decim_4
      _decim_4_dpos_check:
      beqz.n      a6, _decim_4                                          // jump to decim_4 if start_pos is 0
      bnone       a6, a15, _decim_4                                     // jump to decim_4 if start_pos is divisible by 4
      srli        a14, a15, 1                                           // modulo 2 mask
      bnone       a6, a14, _decim_2                                     // jump to decim_2 if start_pos is divisible by 2
      j           _other_decim                                          // jump to other decim, if start_pos is odd number

      // check start_pos for the largest decim as decim_2
      _decim_2_dpos_check:
      beqz.n      a6, _decim_2                                          // jump to decim_2 if start_pos is 0
      bnone       a6, a14, _decim_2                                     // jump to decim_2 if start_pos is divisible by 2
      j           _other_decim                                          // jump to other_decim if srart_pos is odd number


      // decimation and fir length divisible by 16, only right final_shift
      _decim_16_len_16:

      s32i.n      a5, a1, 0                                             // save len/decim to a1, as return value
      srli        a13, a13, 4                                           // fir->decim /= 16 (set length of delay line filling loop)                                              

      // Prepare final shift value
      l32i.n      a10, a2, 20                                           // get address of rounding array to a10
      l16si	      a15, a2, 16                                           // get shift value (array) 
      addi.n	a11, a15, -15                                          // final_shift +15   
      neg	      a11, a11

      // first delay line load ((decim - d_pos) / 16 times) when d_pos is not 0
      beqz        a6, main_loop_decim_16
      slli        a14, a13, 4                                           // decim * 16
      sub         a15, a14, a6                                          // a15 = decim - d_pos
      srli        a15, a15, 4                                           // a15 / 16

      loopnez a15, ._loop_d_pos_decim_16

            blt	a12, a7, reset_fir_d_pos_decim_16                     //if(fir->pos >= fir->N){                                       
                  movi.n	a12, 0                                    // fir->pos = 0
                  l32i.n	a9, a2, 4                                 // reset delay line to the beginning
            reset_fir_d_pos_decim_16: 

            ee.vld.128.ip	    q7, a3, 16                            // load from input (a3) to q7, increase a3 pointer
            ee.vst.128.ip         q7, a9, 16                            // save to delay_line (a9) from q7, increase a9 pointer  
            addi.n	          a12, a12, 16                          // fir->pos++ 
            
            ee.vld.128.ip	    q7, a3, 16                            // load from input (a3) to q7, increase a3 pointer
            ee.vst.128.ip         q7, a9, 16                            // save to delay_line (a9) from q7, increase a9 pointer  
      ._loop_d_pos_decim_16:
      
      j ._loop_fill_delay_decim_16                                      // skip the first iteration of the delay line filling routine


      main_loop_decim_16:

            // fill the delay line by the amount of fir->dec
            loopnez a13, ._loop_fill_delay_decim_16

                  blt	a12, a7, reset_fir_pos_decim_16                 //if(fir->pos >= fir->N){                                       
                        movi.n	a12, 0                              // fir->pos = 0
                        l32i.n	a9, a2, 4                           // reset delay line to the beginning
                  reset_fir_pos_decim_16:   

                  ee.vld.128.ip	    q7, a3, 16                      // load from input (a3) to q7, increase a3 pointer
                  ee.vst.128.ip         q7, a9, 16                      // save to delay_line (a9) from q7, increase a9 pointer  
                  addi.n	          a12, a12, 16                    // fir->pos++ 

                  ee.vld.128.ip	    q7, a3, 16                      // load from input (a3) to q7, increase a3 pointer
                  ee.vst.128.ip         q7, a9, 16                      // save to delay_line (a9) from q7, increase a9 pointer     
            ._loop_fill_delay_decim_16:
                  


            ee.ld.accx.ip     a10, 0                                    // load rounding value to accx
            sub	            a15, a7, a12                              // loop_len = fir->N - fir->pos
            ee.vld.128.ip	q0, a8, 16                                // Preload
            srli              a15, a15, 4                               // loop_len >> 4 (loop_len / 16)
            ee.vld.128.ip	q1, a9, 16                                // Preload

            // Circular buffer loop
            loopnez a15, ._loop_end_1st_circular_buff_decim_16
                  ee.vld.128.ip	              q2, a8, 16
                  ee.vmulas.s16.accx.ld.ip	  q3, a9, 16, q0, q1
                  ee.vld.128.ip	              q0, a8, 16
                  ee.vmulas.s16.accx.ld.ip	  q1, a9, 16, q2, q3
            ._loop_end_1st_circular_buff_decim_16:

            l32i.n	      a9, a2, 4                                 // reset delay to the beginning
            srli              a15, a12, 4                               // loop_len >> 4 (fir->pos / 16)
            ee.vld.128.ip	q1, a9, 16                                // Preload

            // Circular buffer loop
            loopnez a15, ._loop_end_2nd_circular_buff_decim_16
                  ee.vld.128.ip	              q2, a8, 16
                  ee.vmulas.s16.accx.ld.ip	  q3, a9, 16, q0, q1
                  ee.vld.128.ip	              q0, a8, 16
                  ee.vmulas.s16.accx.ld.ip	  q1, a9, 16, q2, q3
            ._loop_end_2nd_circular_buff_decim_16:
                                                               
            ee.srs.accx       a15, a11, 0                               // shift accx register by final_shift amount (a11), save the lower 32bits to a15
            l32i.n	      a8, a2, 0                                 // reset coeffs to the beginning
            s16i	            a15, a4, 0                                // save the final acc value to the output                   

            l32i.n	      a9, a2, 4                                 // reset delay to the beginning
            addi.n            a5, a5, -1                                // decrement length
            addi.n	      a4, a4, 2                                 // increase pointer p_output++
            addx2             a9, a12, a9                               // p_delay[fir->pos] - (two times the fir->pos)
            bnez.n            a5, main_loop_decim_16

      l32i.n  a2, a1, 0                                                 // load saved return value from a1 to a2
	retw.n

      // DECIMATION 2 
      _decim_2:

      s32i.n      a5, a1, 0                                                         // save calculated return value to a1                                                          
      l32i.n      a10, a2, 20                                                       // get address of rounding array to a10
                                               
      // Prepare final shift value              
      l16si	      a15, a2,   16                                                     // get shift value   
      addi.n      a15, a15, -15                                                     // final_shift -15
      ssl         a15                                                               // set SAR register to left shift (even if not used)
      neg         a11, a15
      s32i        a11, a1, 4                                                        // save final_shift value to a1           

      // Set delay line fill loop count
      srli        a13, a13, 1                                                       // decim = decim / 2  

      // divide by 24 constant            
      movi        a11, 178956971      

       // first delay line load ((decim - d_pos) / 2 times) when d_pos is not 0
      beqz        a6, main_loop_decim_2                                             // branch if d_pos = 0
      slli        a14, a13, 1                                                       // a14 = dec * 2
      sub         a15, a14, a6                                                      // a15 = decim - d_pos
      srli        a15, a15, 1

      loopnez     a15, ._loop_d_pos_decim_2
            
            blt	a12, a7, reset_fir_d_pos_decim_2                                  //if(fir->pos >= fir->N){                                               
                  movi.n	      a12, 0                                          // fir->pos = 0
                  l32i.n	      a9, a2, 4                                       // reset delay line to the beginning
            reset_fir_d_pos_decim_2: 
            
            l32i.n      a15, a3, 0                                                  // load 32 bits from input a3 to a15
            addi.n	a12, a12, 2                                                 // fir->pos++
            s32i.n      a15, a9, 0                                                  // save 32 bits from a15 to delay line a9
            addi.n      a3, a3, 4                                                   // Increase pointer of the input array by 4
            addi.n      a9, a9, 4                                                   // Increase pointer of the delay line by 4
      ._loop_d_pos_decim_2:

      j ._loop_fill_delay_decim_2                                                   // skip the first iteration of the delay line filling routine          

      main_loop_decim_2:

            // Fill the delay line (only decim 2)
            loopnez a13, ._loop_fill_delay_decim_2

                  blt	a12, a7, reset_fir_pos_decim_2                              //if(fir->pos >= fir->N){                                               
                        movi.n	      a12, 0                                    // fir->pos = 0
                        l32i.n	      a9, a2, 4                                 // reset delay line to the beginning
                  reset_fir_pos_decim_2: 

                  l32i.n      a15, a3, 0                                            // load 32 bits from input a3 to a15
                  addi.n	a12, a12, 2                                           // fir->pos++
                  s32i.n      a15, a9, 0                                            // save 32 bits from a15 to delay line a9
                  addi.n      a3, a3, 4                                             // Increase pointer of the input array by 4
                  addi.n      a9, a9, 4                                             // Increase pointer of the delay line by 4
            ._loop_fill_delay_decim_2:             

            ee.ld.accx.ip           a10, 0                                          // load rounding value to accx

            sub	                  a15, a7, a12                                    // a15 = loop_len = fir->N - fir->pos
            ee.ld.128.usar.ip       q0, a9, 16                                      // Preload from delay
            muluh                   a14, a15, a11                                   // a14 = loop1_len = loop_len / 24
            ee.ld.128.usar.ip       q1, a9, 16                   

            movi.n                  a6, 24                                          // Move 24 to a6
            ee.vld.128.ip           q3, a8, 16                                      // preload from coeffs
            mul16s                  a6, a6, a14                                     // loop1_len * 24
            ee.src.q.ld.ip          q2, a9, 16, q0, q1                              // preload and shift from delay
            sub                     a6, a15, a6                                     // loop remiainder = a6 = loop_len - loop1_len *24
            
            loopnez a14, ._loop_end_1st_circular_buff_decim_2
                  ee.vld.128.ip                       q4, a8, 16                    // Load from coeffs   
                  ee.vmulas.s16.accx.ld.ip.qup        q0, a9, 16, q0, q3, q1, q2    // Load from delay
                  ee.vld.128.ip                       q5, a8, 16                    // Load from coeffs
                  ee.vmulas.s16.accx.ld.ip.qup        q1, a9, 16, q1, q4, q2, q0    // Load from delay
                  ee.vld.128.ip                       q3, a8, 16                    // Load from coeffs 
                  ee.vmulas.s16.accx.ld.ip.qup        q2, a9, 16, q2, q5, q0, q1    // Load from delay     
            ._loop_end_1st_circular_buff_decim_2:

            beqi        a6, 16, _decim_2_1st_equal_to_16                            // jump if the remainder is equal to 16
            bgei        a6, 16, _decim_2_1st_more_equal_to_16                       // jump if the remainder is greater or equal to 16
            beqi        a6, 8,  _decim_2_1st_equal_to_8                             // jump if the remainder is equal to 8
            bgei        a6, 8,  _decim_2_1st_more_equal_to_8                        // jump if the remainder is greater or equal to 8
            beqz        a6,     _decim_2_1st_equal_to_0                             // jump if the remainder is equal to 0
            bgez        a6,     _decim_2_1st_more_equal_to_0                        // jump if the remainder is greater or equal to 0

            _decim_2_1st_equal_to_16:
                  l32i.n      a9, a2, 4                                             // reset delay to the beginning

                  ee.vld.128.ip                 q4 ,a8, 16
                  ee.vmulas.s16.accx.ld.ip.qup  q0 ,a9, 16, q0, q3, q1, q2
                  ee.vmulas.s16.accx            q1, q4
                  j                             _1st_circular_buff_end_decim_2

            _decim_2_1st_more_equal_to_16:
                  ee.vld.128.ip                     q4, a8, 16
                  ee.vmulas.s16.accx.ld.ip.qup      q0, a9, 16, q0, q3, q1, q2

                  l32i.n      a9, a2, 4                                             // reset delay to the beginning
                  ee.vld.128.ip                     q5, a8, 16
                  
                  addx2       a6, a7, a9                                            // move delay pointer to the end, save the pointer to a6
                  ee.vmulas.s16.accx                q1, q4                          // MAC
                  
                  addi        a15, a6, -16                                          // move pointer back by 16, save the pointer to a15

                  ee.vld.128.ip           q2, a15, 16                               // load 8 words, not modifying the SAR_BYTE, load from a15 (end of the array)
                  ee.vld.128.ip           q0 ,a9, 16                                // load 8 words, not modifying the SAR_BYTE, load from a9  (beginning of the array)
                  ee.src.q.ld.ip          q1, a9, 16, q2, q0                        // shift by the amount of SAR_BYTE
                  ee.vmulas.s16.accx      q2, q5
                  blti                    a12, 8, _2nd_circular_buff_end_decim_2    // skip the second circular buffer if fir->pos is lower than 8 
                  j                       _1st_circular_buff_end_decim_2

            _decim_2_1st_equal_to_8:
                  ee.vmulas.s16.accx      q0, q3
                  j                       _1st_circular_buff_end_decim_2

            _decim_2_1st_more_equal_to_8:

                  l32i.n      a9, a2, 4                                             // reset delay to the beginning
                  ee.vld.128.ip       q4 , a8, 16

                  addx2       a6, a7, a9                                            // move delay pointer to the end, save the pointer to a6
                  ee.vmulas.s16.accx  q0, q3
                  
                  addi        a15, a6, -16                                          // move pointer back by 16, save the pointer to a15

                  ee.vld.128.ip           q1, a15, 16                               // load 8 words, not modifying the SAR_BYTE, load from a15 (end of the array)
                  ee.vld.128.ip           q2, a9, 16                                // load 8 words, not modifying the SAR_BYTE, load from a9  (beginning of the array)
                  ee.src.q.ld.ip          q0, a9, 16, q1, q2                        // shift by the amount of SAR_BYTE
                  ee.vmulas.s16.accx      q1, q4
                  blti                    a12, 8, _2nd_circular_buff_end_decim_2    // skip the second circular buffer if fir->pos is lower than 8 
                  j                       _1st_circular_buff_end_decim_2

            _decim_2_1st_more_equal_to_0:
                  l32i.n  	a9, a2, 4                                             // reset delay to the beginning
                  addx2       a6, a7, a9                                            // move delay pointer to the end, save the pointer to a6
                  addi        a15, a6, -16                                          // move pointer back by 16, save the pointer to a15

                  ee.vld.128.ip           q0, a15, 16                               // load 8 words, not modifying the SAR_BYTE, load from a15 (end of the array)
                  ee.vld.128.ip           q1, a9, 16                                // load 8 words, not modifying the SAR_BYTE, load from a9  (beginning of the array)
                  ee.src.q.ld.ip          q2, a9, 16, q0, q1                        // shift by amount of SAR_BYTE
                  ee.vmulas.s16.accx      q0, q3
                  blti                    a12, 8, _2nd_circular_buff_end_decim_2    // skip the second circular buffer if fir->pos is lower than 8 
                  j                       _1st_circular_buff_end_decim_2

            _decim_2_1st_equal_to_0:
                  addi  a8, a8, -16                                                 // move coeffs pointer back by 16

            _1st_circular_buff_end_decim_2:

            // SECOND PART OF CIRCULAR BUFFER
            l32i.n	      a9, a2, 4                                             // reset delay to the beginning
            muluh             a14, a12, a11                                         // a14 = loop2_len = fir->pos / 24

            movi.n            a6, 24                                                // Move 24 to a6
            ee.vld.128.ip     q0, a9, 16                                            // Preload
            ee.vld.128.ip     q1, a9, 16
            ee.src.q.ld.ip    q2, a9, 16, q0, q1

            mul16s            a15, a6, a14                                          // loop1_len * 24
            ee.vld.128.ip     q3, a8, 16                                            // Preload
            sub               a6, a12, a15                                          // loop remiainder = a6 = fir->pos - loop1_len *24

            loopnez a14, ._loop_end_2nd_circular_buff_decim_2
                  ee.vld.128.ip                       q4, a8, 16                    // Load from coeffs   
                  ee.vmulas.s16.accx.ld.ip.qup        q0, a9, 16, q0, q3, q1, q2    // Load from delay
                  ee.vld.128.ip                       q5, a8, 16                    // Load from coeffs
                  ee.vmulas.s16.accx.ld.ip.qup        q1, a9, 16, q1, q4, q2, q0    // Load from delay
                  ee.vld.128.ip                       q3, a8, 16                    // Load from coeffs 
                  ee.vmulas.s16.accx.ld.ip.qup        q2, a9, 16, q2, q5, q0, q1    // Load from delay                                         
            ._loop_end_2nd_circular_buff_decim_2:
            
            bgei        a6, 16, _decim_2_2nd_more_equal_to_16                       // jump if the remainder is greater or equal to 16
            bgei        a6, 8,  _decim_2_2nd_more_equal_to_8                        // jump if the remainder is greater or equal to 8
            bgez        a6,     _2nd_circular_buff_end_decim_2                      // jump if the remainder is greater or equal to 0

            _decim_2_2nd_more_equal_to_16:
                  ee.vld.128.ip                     q4, a8, 16
                  ee.vmulas.s16.accx.ld.ip.qup      q0, a9, 16, q0, q3, q1, q2
                  ee.vmulas.s16.accx                q1, q4
                  j                                 _2nd_circular_buff_end_decim_2

            _decim_2_2nd_more_equal_to_8:
                  ee.vmulas.s16.accx                q0, q3

            _2nd_circular_buff_end_decim_2:

            l32i.n            a6, a1, 4                                             // load final shift value to a6    
            l32i.n	      a8, a2, 0                                             // reset coeffs to the beginning

            bgez              a6, _shift_right_decim_2
            rur.accx_0        a9                                                    // acc low
            rur.accx_1        a14                                                   // acc high
            addi.n            a5, a5, -1                                            // decrease counter
            src               a15, a9, a14                                          // funnel shift left, save 32bits to a15
            j                 _shift_left_decim_2

            _shift_right_decim_2:
            ee.srs.accx       a15, a6, 0                                            // shift accx register by final_shift amount (a6), save the lower 32bits to a15
            addi.n            a5, a5, -1                                            // decrease counter

            _shift_left_decim_2:
            s16i	            a15, a4, 0                                            // save the final acc value to the output                   
            l32i.n	      a9, a2, 4                                             // reset delay to the beginning
            addi.n	      a4, a4, 2                                             // increase pointer p_output++ 
            addx2             a9, a12, a9                                           // p_delay[fir->pos] - (two times the fir->pos)
            bnez.n            a5, main_loop_decim_2

      l32i.n  a2, a1, 0                                                             // load saved return value from a1 to a2
      retw.n


      // DECIMATION 4 
      _decim_4:

      s32i.n      a5, a1, 0                                                         // save calculated return value to a1
      l32i.n      a10, a2, 20                                                       // get address of rounding array to a10

      // Prepare final shift value        
      l16si	      a15, a2,   16                                                     // get shift value   
      addi.n      a15, a15, -15                                                     // final_shift -15
      ssl         a15                                                               // set SAR register to left shift (even if not used)
      neg         a11, a15
      s32i        a11, a1, 4                                                        // save final_shift value to a1           

      // Set delay line fill loop count
      srli        a13, a13, 2                                                       // decim = decim / 4   

      // divide by 24 constant            
      movi        a11, 178956971      

      // first delay line load ((decim - d_pos) / 4 times) when d_pos is not 0
      beqz        a6, main_loop_decim_4
      slli        a14, a13, 2                                                       // decim * 4
      sub         a15, a14, a6                                                      // a15 = decim - d_pos
      srli        a15, a15, 2

      loopnez     a15, ._loop_d_pos_decim_4

            blt	a12, a7, reset_fir_d_pos_decim_4                                  //if(fir->pos >= fir->N){                                                    
                  movi.n	      a12, 0                                          // fir->pos = 0
                  l32i.n	      a9, a2, 4                                       // reset delay line to the beginning
            reset_fir_d_pos_decim_4:  
            
            ee.vld.l.64.ip    q0, a3, 8                                             // load 64bits from input (a3) to lower half of q0
            ee.vst.l.64.ip    q0, a9, 8                                             // store 64bits from lower half of q0 to delay line a9
            addi.n            a12, a12, 4                                           // fir->pos++
      ._loop_d_pos_decim_4:

      j ._loop_fill_delay_decim_4                                                   // skip the first iteration of the delay line filling routine      

      main_loop_decim_4:

            // Fill the delay line (only decim 4)
            loopnez a13, ._loop_fill_delay_decim_4

                  blt	a12, a7, reset_fir_pos_decim_4                              //if(fir->pos >= fir->N){                                                    
                        movi.n	      a12, 0                                    // fir->pos = 0
                        l32i.n	      a9, a2, 4                                 // reset delay line to the beginning
                  reset_fir_pos_decim_4:  

                  ee.vld.l.64.ip    q0, a3, 8                                       // load 64bits from input (a3) to lower half of q0
                  ee.vst.l.64.ip    q0, a9, 8                                       // store 64bits from lower half of q0 to delay line a9
                  addi.n            a12, a12, 4                                     // fir->pos++
            ._loop_fill_delay_decim_4:            

            ee.ld.accx.ip           a10, 0                                          // load rounding value to accx

            sub	                  a15, a7, a12                                    // a15 = loop_len = fir->N - fir->pos
            ee.ld.128.usar.ip       q0, a9, 16                                      // Preload from delay
            muluh                   a14, a15, a11                                   // a14 = loop1_len = loop_len / 24
            ee.ld.128.usar.ip       q1, a9, 16                   

            movi.n                  a6, 24                                          // Move 24 to a6
            ee.vld.128.ip           q3, a8, 16                                      // preload from coeffs
            mul16s                  a6, a6, a14                                     // loop1_len * 24
            ee.src.q.ld.ip          q2, a9, 16, q0, q1                              // preload and shift from delay
            sub                     a6, a15, a6                                     // loop remiainder = a6 = loop_len - loop1_len *24
            
            loopnez a14, ._loop_end_1st_circular_buff_decim_4
                  ee.vld.128.ip                       q4, a8, 16                    // Load from coeffs   
                  ee.vmulas.s16.accx.ld.ip.qup        q0, a9, 16, q0, q3, q1, q2    // Load from delay
                  ee.vld.128.ip                       q5, a8, 16                    // Load from coeffs
                  ee.vmulas.s16.accx.ld.ip.qup        q1, a9, 16, q1, q4, q2, q0    // Load from delay
                  ee.vld.128.ip                       q3, a8, 16                    // Load from coeffs 
                  ee.vmulas.s16.accx.ld.ip.qup        q2, a9, 16, q2, q5, q0, q1    // Load from delay     
            ._loop_end_1st_circular_buff_decim_4:

            beqi        a6, 16, _decim_4_1st_equal_to_16                            // jump if the remainder is equal to 16
            bgei        a6, 16, _decim_4_1st_more_equal_to_16                       // jump if the remainder is greater or equal to 16
            beqi        a6, 8,  _decim_4_1st_equal_to_8                             // jump if the remainder is equal to 8
            bgei        a6, 8,  _decim_4_1st_more_equal_to_8                        // jump if the remainder is greater or equal to 8
            beqz        a6,     _decim_4_1st_equal_to_0                             // jump if the remainder is equal to 0
            bgez        a6,     _decim_4_1st_more_equal_to_0                        // jump if the remainder is greater or equal to 0

            _decim_4_1st_equal_to_16:
                  l32i.n      a9, a2, 4                                             // reset delay to the beginning

                  ee.vld.128.ip                 q4 ,a8, 16
                  ee.vmulas.s16.accx.ld.ip.qup  q0 ,a9, 16, q0, q3, q1, q2
                  ee.vmulas.s16.accx            q1, q4
                  j                             _1st_circular_buff_end_decim_4

            _decim_4_1st_more_equal_to_16:
                  ee.vld.128.ip                     q4, a8, 16
                  ee.vmulas.s16.accx.ld.ip.qup      q0, a9, 16, q0, q3, q1, q2

                  l32i.n      a9, a2, 4                                             // reset delay to the beginning
                  ee.vld.128.ip                     q5, a8, 16
                  
                  addx2       a6, a7, a9                                            // move delay pointer to the end, save the pointer to a6
                  ee.vmulas.s16.accx                q1, q4                          // MAC
                  
                  addi        a15, a6, -16                                          // move pointer back by 16, save the pointer to a15

                  ee.vld.128.ip           q2, a15, 16                               // load 8 words, not modifying the SAR_BYTE, load from a15 (end of the array)
                  ee.vld.128.ip           q0 ,a9, 16                                // load 8 words, not modifying the SAR_BYTE, load from a9  (beginning of the array)
                  ee.src.q.ld.ip          q1, a9, 16, q2, q0                        // shift by the amount of SAR_BYTE
                  ee.vmulas.s16.accx      q2, q5
                  blti                    a12, 8, _2nd_circular_buff_end_decim_4    // skip the second circular buffer if fir->pos is lower than 8 
                  j                       _1st_circular_buff_end_decim_4

            _decim_4_1st_equal_to_8:
                  ee.vmulas.s16.accx      q0, q3
                  j                       _1st_circular_buff_end_decim_4

            _decim_4_1st_more_equal_to_8:

                  l32i.n      a9, a2, 4                                             // reset delay to the beginning
                  ee.vld.128.ip       q4 , a8, 16

                  addx2       a6, a7, a9                                            // move delay pointer to the end, save the pointer to a6
                  ee.vmulas.s16.accx  q0, q3
                  
                  addi        a15, a6, -16                                          // move pointer back by 16, save the pointer to a15

                  ee.vld.128.ip           q1, a15, 16                               // load 8 words, not modifying the SAR_BYTE, load from a15 (end of the array)
                  ee.vld.128.ip           q2, a9, 16                                // load 8 words, not modifying the SAR_BYTE, load from a9  (beginning of the array)
                  ee.src.q.ld.ip          q0, a9, 16, q1, q2                        // shift by the amount of SAR_BYTE
                  ee.vmulas.s16.accx      q1, q4
                  blti                    a12, 8, _2nd_circular_buff_end_decim_4    // skip the second circular buffer if fir->pos is lower than 8 
                  j                       _1st_circular_buff_end_decim_4

            _decim_4_1st_more_equal_to_0:
                  l32i.n  	a9, a2, 4                                             // reset delay to the beginning
                  addx2       a6, a7, a9                                            // move delay pointer to the end, save pointer to a6
                  addi        a15, a6, -16                                          // move pointer back by 16, save the pointer to a15

                  ee.vld.128.ip           q0, a15, 16                               // load 8 words, not modifying the SAR_BYTE, load from a15 (end of the array)
                  ee.vld.128.ip           q1, a9, 16                                // load 8 words, not modifying the SAR_BYTE, load from a9  (beginning of the array)
                  ee.src.q.ld.ip          q2, a9, 16, q0, q1                        // shift by amount of SAR_BYTE
                  ee.vmulas.s16.accx      q0, q3
                  blti                    a12, 8, _2nd_circular_buff_end_decim_4    // skip the second circular buffer if fir->pos is lower than 8 
                  j                       _1st_circular_buff_end_decim_4

            _decim_4_1st_equal_to_0:
                  addi  a8, a8, -16                                                 // move coeffs pointer back by 16

            _1st_circular_buff_end_decim_4:

            // SECOND PART OF CIRCULAR BUFFER
            l32i.n	      a9, a2, 4                                             // reset delay to the beginning
            muluh             a14, a12, a11                                         // a14 = loop2_len = fir->pos / 24

            movi.n            a6, 24                                                // Move 24 to a6
            ee.vld.128.ip     q0, a9, 16                                            // Preload
            ee.vld.128.ip     q1, a9, 16
            ee.src.q.ld.ip    q2, a9, 16, q0, q1

            mul16s            a15, a6, a14                                          // loop1_len * 24
            ee.vld.128.ip     q3, a8, 16                                            // Preload
            sub               a6, a12, a15                                          // loop remiainder = a6 = fir->pos - loop1_len *24

            loopnez a14, ._loop_end_2nd_circular_buff_decim_4
                  ee.vld.128.ip                       q4, a8, 16                    // Load from coeffs   
                  ee.vmulas.s16.accx.ld.ip.qup        q0, a9, 16, q0, q3, q1, q2    // Load from delay
                  ee.vld.128.ip                       q5, a8, 16                    // Load from coeffs
                  ee.vmulas.s16.accx.ld.ip.qup        q1, a9, 16, q1, q4, q2, q0    // Load from delay
                  ee.vld.128.ip                       q3, a8, 16                    // Load from coeffs 
                  ee.vmulas.s16.accx.ld.ip.qup        q2, a9, 16, q2, q5, q0, q1    // Load from delay                                         
            ._loop_end_2nd_circular_buff_decim_4:
            
            bgei        a6, 16, _decim_4_2nd_more_equal_to_16                       // jump if the remainder is greater or equal to 16
            bgei        a6, 8,  _decim_4_2nd_more_equal_to_8                        // jump if the remainder is greater or equal to 8
            bgez        a6,     _2nd_circular_buff_end_decim_4                      // jump if the remainder is greater or equal to 0

            _decim_4_2nd_more_equal_to_16:
                  ee.vld.128.ip                     q4, a8, 16
                  ee.vmulas.s16.accx.ld.ip.qup      q0, a9, 16, q0, q3, q1, q2
                  ee.vmulas.s16.accx                q1, q4
                  j                                 _2nd_circular_buff_end_decim_4

            _decim_4_2nd_more_equal_to_8:
                  ee.vmulas.s16.accx                q0, q3

            _2nd_circular_buff_end_decim_4:

            l32i.n            a6, a1, 4                                             // load final shift value to a6
            l32i.n	      a8, a2, 0                                             // reset coeffs to the beginning
            
            bgez              a6, _shift_right_decim_4
            rur.accx_0        a9                                                    // acc low
            rur.accx_1        a14                                                   // acc high
            addi.n            a5, a5, -1                                            // decrease counter
            src               a15, a9, a14                                          // funnel shift left, save 32bits to a15
            j                 _shift_left_decim_4

            _shift_right_decim_4:
            ee.srs.accx       a15, a6, 0                                            // shift accx register by final_shift amount (a6), save the lower 32bits to a15
            addi.n            a5, a5, -1                                            // decrease counter

            _shift_left_decim_4:
            s16i	            a15, a4, 0                                            // save the final acc value to the output                   
            l32i.n	      a9, a2, 4                                             // reset delay to the beginning
            addi.n	      a4, a4, 2                                             // increase pointer p_output++ 
            addx2             a9, a12, a9                                           // p_delay[fir->pos] - (two times the fir->pos)
            bnez.n            a5, main_loop_decim_4

      l32i.n  a2, a1, 0                                                             // load saved return value from a1 to a2
      retw.n


      // DECIMATION 8 
      _decim_8:
      
      s32i.n      a5, a1, 0                                                         // save len/decim to a1, as return value
      l32i.n      a10, a2, 20                                                       // get address of rounding array to a10

      // Prepare final shift value        
      l16si	      a15, a2,   16                                                     // get shift value   
      addi.n      a15, a15, -15                                                     // final_shift -15
      ssl         a15                                                               // set SAR register to left shift (even if not used)
      neg         a11, a15
      s32i        a11, a1, 4                                                        // save final_shift value to a1           

      // Set delay line fill loop count
      srli        a13, a13, 3                                                       // decim = decim / 8 

      // divide by 24 constant            
      movi        a11, 178956971      

      // first delay line load ((decim - d_pos) / 8 times) when d_pos is not 0
      beqz        a6, main_loop_decim_8
      slli        a14, a13, 3                                                       // decim * 8
      sub         a15, a14, a6                                                      // a15 = decim - d_pos
      srli        a15, a15, 3                                                       // a15 / 8

      loopnez a15, ._loop_d_pos_decim_8

            blt	a12, a7, reset_fir_d_pos_decim_8                                  //if(fir->pos >= fir->N){                                   
                  movi.n	      a12, 0                                          // fir->pos = 0
                  l32i.n	      a9, a2, 4                                       // reset delay line to the beginning
            reset_fir_d_pos_decim_8:
            
            ee.vld.128.ip	    q0, a3, 16                                        // load 64bits from input (a3) to lower half of q0
            ee.vst.128.ip	    q0, a9, 16                                        // store 64bits from lower half of q0 to delay line a9
            addi.n                a12, a12, 8                                       // fir->pos++
      ._loop_d_pos_decim_8:

      j ._loop_fill_delay_decim_8                                                   // skip the first iteration of the delay line filling routine

      main_loop_decim_8:

            // Fill the delay line (only decim 8)
            loopnez a13, ._loop_fill_delay_decim_8

                  blt	a12, a7, reset_fir_pos_decim_8                              //if(fir->pos >= fir->N){                                   
                        movi.n	      a12, 0                                    // fir->pos = 0
                        l32i.n	      a9, a2, 4                                 // reset delay line to the beginning
                  reset_fir_pos_decim_8:

                  ee.vld.128.ip	    q0, a3, 16                                  // load 64bits from input (a3) to lower half of q0
                  ee.vst.128.ip	    q0, a9, 16                                  // store 64bits from lower half of q0 to delay line a9
                  addi.n                a12, a12, 8                                 // fir->pos++
                  
            ._loop_fill_delay_decim_8:         

            ee.ld.accx.ip           a10, 0                                          // load rounding value to accx

            sub	                  a15, a7, a12                                    // a15 = loop_len = fir->N - fir->pos
            ee.ld.128.usar.ip       q0, a9, 16                                      // Preload from delay
            muluh                   a14, a15, a11                                   // a14 = loop1_len = loop_len / 24
            ee.ld.128.usar.ip       q1, a9, 16                   

            movi.n                  a6, 24                                          // Move 24 to a6
            ee.vld.128.ip           q3, a8, 16                                      // preload from coeffs
            mul16s                  a6, a6, a14                                     // loop1_len * 24
            ee.src.q.ld.ip          q2, a9, 16, q0, q1                              // preload and shift from delay
            sub                     a6, a15, a6                                     // loop remiainder = a6 = loop_len - loop1_len *24
            
            loopnez a14, ._loop_end_1st_circular_buff_decim_8
                  ee.vld.128.ip                       q4, a8, 16                    // Load from coeffs   
                  ee.vmulas.s16.accx.ld.ip.qup        q0, a9, 16, q0, q3, q1, q2    // Load from delay
                  ee.vld.128.ip                       q5, a8, 16                    // Load from coeffs
                  ee.vmulas.s16.accx.ld.ip.qup        q1, a9, 16, q1, q4, q2, q0    // Load from delay
                  ee.vld.128.ip                       q3, a8, 16                    // Load from coeffs 
                  ee.vmulas.s16.accx.ld.ip.qup        q2, a9, 16, q2, q5, q0, q1    // Load from delay     
            ._loop_end_1st_circular_buff_decim_8:

            beqi        a6, 16, _decim_8_1st_equal_to_16                            // jump if the remainder is equal to 16
            bgei        a6, 16, _decim_8_1st_more_equal_to_16                       // jump if the remainder is greater or equal to 16
            beqi        a6, 8,  _decim_8_1st_equal_to_8                             // jump if the remainder is equal to 8
            bgei        a6, 8,  _decim_8_1st_more_equal_to_8                        // jump if the remainder is greater or equal to 8
            beqz        a6,     _decim_8_1st_equal_to_0                             // jump if the remainder is equal to 0
            bgez        a6,     _decim_8_1st_more_equal_to_0                        // jump if the remainder is greater or equal to 0

            _decim_8_1st_equal_to_16:
                  l32i.n      a9, a2, 4                                             // reset delay to the beginning

                  ee.vld.128.ip                 q4 ,a8, 16
                  ee.vmulas.s16.accx.ld.ip.qup  q0 ,a9, 16, q0, q3, q1, q2
                  ee.vmulas.s16.accx            q1, q4
                  j                             _1st_circular_buff_end_decim_8

            _decim_8_1st_more_equal_to_16:
                  ee.vld.128.ip                     q4, a8, 16
                  ee.vmulas.s16.accx.ld.ip.qup      q0, a9, 16, q0, q3, q1, q2

                  l32i.n      a9, a2, 4                                             // reset delay to the beginning
                  ee.vld.128.ip                     q5, a8, 16
                  
                  addx2       a6, a7, a9                                            // move delay pointer to the end, save the pointer to a6
                  ee.vmulas.s16.accx                q1, q4                          // MAC
                  
                  addi        a15, a6, -16                                          // move pointer back by 16, save the pointer to a15

                  ee.vld.128.ip           q2, a15, 16                               // load 8 words, not modifying the SAR_BYTE, load from a15 (end of the array)
                  ee.vld.128.ip           q0 ,a9, 16                                // load 8 words, not modifying the SAR_BYTE, load from a9  (beginning of the array)
                  ee.src.q.ld.ip          q1, a9, 16, q2, q0                        // shift by the amount of SAR_BYTE
                  ee.vmulas.s16.accx      q2, q5
                  blti                    a12, 8, _2nd_circular_buff_end_decim_8    // skip the second circular buffer if fir->pos is lower than 8 
                  j                       _1st_circular_buff_end_decim_8

            _decim_8_1st_equal_to_8:
                  ee.vmulas.s16.accx      q0, q3
                  j                       _1st_circular_buff_end_decim_8

            _decim_8_1st_more_equal_to_8:

                  l32i.n      a9, a2, 4                                             // reset delay to the beginning
                  ee.vld.128.ip       q4 , a8, 16

                  addx2       a6, a7, a9                                            // move delay pointer to the end, save the pointer to a6
                  ee.vmulas.s16.accx  q0, q3
                  
                  addi        a15, a6, -16                                          // move pointer back by 16, save the pointer to a15

                  ee.vld.128.ip           q1, a15, 16                               // load 8 words, not modifying the SAR_BYTE, load from a15 (end of the array)
                  ee.vld.128.ip           q2, a9, 16                                // load 8 words, not modifying the SAR_BYTE, load from a9  (beginning of the array)
                  ee.src.q.ld.ip          q0, a9, 16, q1, q2                        // shift by the amount of SAR_BYTE
                  ee.vmulas.s16.accx      q1, q4
                  blti                    a12, 8, _2nd_circular_buff_end_decim_8    // skip the second circular buffer if fir->pos is lower than 8 
                  j                       _1st_circular_buff_end_decim_8

            _decim_8_1st_more_equal_to_0:
                  l32i.n  	a9, a2, 4                                             // reset delay to the beginning
                  addx2       a6, a7, a9                                            // move delay pointer to the end, save pointer to a6
                  addi        a15, a6, -16                                          // move pointer back by 16, save the pointer to a15

                  ee.vld.128.ip           q0, a15, 16                               // load 8 words, not modifying the SAR_BYTE, load from a15 (end of the array)
                  ee.vld.128.ip           q1, a9, 16                                // load 8 words, not modifying the SAR_BYTE, load from a9  (beginning of the array)
                  ee.src.q.ld.ip          q2, a9, 16, q0, q1                        // shift by amount of SAR_BYTE
                  ee.vmulas.s16.accx      q0, q3
                  blti                    a12, 8, _2nd_circular_buff_end_decim_8    // skip the second circular buffer if fir->pos is lower than 8 
                  j                       _1st_circular_buff_end_decim_8

            _decim_8_1st_equal_to_0:
                  addi  a8, a8, -16                                                 // move coeffs pointer back by 16

            _1st_circular_buff_end_decim_8:

            // SECOND PART OF CIRCULAR BUFFER
            rur.accx_0        a15
            l32i.n	      a9, a2, 4                                             // reset delay to the beginning
            muluh             a14, a12, a11                                         // a14 = loop2_len = fir->pos / 24

            movi.n            a6, 24                                                // Move 24 to a6
            ee.vld.128.ip     q0, a9, 16                                            // Preload
            ee.vld.128.ip     q1, a9, 16
            ee.src.q.ld.ip    q2, a9, 16, q0, q1

            mul16s            a15, a6, a14                                          // loop1_len * 24
            ee.vld.128.ip     q3, a8, 16                                            // Preload
            sub               a6, a12, a15                                          // loop remiainder = a6 = fir->pos - loop1_len *24

            loopnez a14, ._loop_end_2nd_circular_buff_decim_8
                  ee.vld.128.ip                       q4, a8, 16                    // Load from coeffs   
                  ee.vmulas.s16.accx.ld.ip.qup        q0, a9, 16, q0, q3, q1, q2    // Load from delay
                  ee.vld.128.ip                       q5, a8, 16                    // Load from coeffs
                  ee.vmulas.s16.accx.ld.ip.qup        q1, a9, 16, q1, q4, q2, q0    // Load from delay
                  ee.vld.128.ip                       q3, a8, 16                    // Load from coeffs 
                  ee.vmulas.s16.accx.ld.ip.qup        q2, a9, 16, q2, q5, q0, q1    // Load from delay                                         
            ._loop_end_2nd_circular_buff_decim_8:
            
            bgei        a6, 16, _decim_8_2nd_more_equal_to_16                       // jump if the remainder is greater or equal to 16
            bgei        a6, 8,  _decim_8_2nd_more_equal_to_8                        // jump if the remainder is greater or equal to 8
            bgez        a6,     _2nd_circular_buff_end_decim_8                      // jump if the remainder is greater or equal to 0

            _decim_8_2nd_more_equal_to_16:
                  ee.vld.128.ip                     q4, a8, 16
                  ee.vmulas.s16.accx.ld.ip.qup      q0, a9, 16, q0, q3, q1, q2
                  ee.vmulas.s16.accx                q1, q4
                  j                                 _2nd_circular_buff_end_decim_8

            _decim_8_2nd_more_equal_to_8:
                  ee.vmulas.s16.accx                q0, q3

            _2nd_circular_buff_end_decim_8:

            l32i.n            a6, a1, 4                                             // load final shift value to a6
            l32i.n	      a8, a2, 0                                             // reset coeffs to the beginning

            bgez              a6, _shift_right_decim_8
            rur.accx_0        a9                                                    // acc low
            rur.accx_1        a14                                                   // acc high
            addi.n            a5,  a5, -1                                           // decrease counter
            src               a15, a9, a14                                          // funnel shift left, save 32bits to a15
            j                 _shift_left_decim_8

            _shift_right_decim_8:
            ee.srs.accx       a15, a6, 0                                            // shift accx register by final_shift amount (a6), save the lower 32bits to a15
            addi.n            a5, a5, -1                                            // decrease counter

            _shift_left_decim_8:
            s16i	            a15, a4, 0                                            // save the final acc value to the output                   
            l32i.n	      a9, a2, 4                                             // reset delay to the beginning
            addi.n	      a4, a4, 2                                             // increase pointer p_output++ 
            addx2             a9, a12, a9                                           // p_delay[fir->pos] - (two times the fir->pos)
            bnez.n            a5, main_loop_decim_8

      l32i.n  a2, a1, 0                                                             // load saved return value from a1 to a2
      retw.n


      // OTHER DECIMATIONS
      _other_decim:

      s32i.n      a5, a1, 0                                                         // save calculated return value to a1
      l32i.n      a10, a2, 20                                                       // get address of rounding array to a10

      // Prepare final shift value              
      l16si	      a15, a2,   16                                                     // get shift value   
      addi.n      a15, a15, -15                                                     // final_shift -15
      ssl         a15                                                               // set SAR register to left shift (even if not used)
      neg         a11, a15
      s32i        a11, a1, 4                                                        // save final_shift value to a1           

      // divide by 24 constant            
      movi        a11, 178956971 

      // first delay line load (decim - d_pos times) when d_pos is not 0
      beqz        a6, main_loop_other_decim
      sub         a15, a13, a6                                                      // a15 = decim - d_pos

      loopnez     a15, ._loop_d_pos_other_decim

            blt	a12, a7, reset_fir_d_pos_other_decim                              //if(fir->pos >= fir->N){                                                 
                  movi.n	      a12, 0                                          // fir->pos = 0
                  l32i.n	      a9, a2, 4                                       // reset delay line to the beginning
            reset_fir_d_pos_other_decim:  

            l16si       a15, a3, 0                                                  // load 16 bits from input a3 to a15
            addi.n	a12, a12, 1                                                 // fir->pos++
            s16i        a15, a9, 0                                                  // save 16 bits from a15 to delay line a9
            addi.n      a3, a3, 2                                                   // Increase pointer of the input array by 2
            addi.n      a9, a9, 2                                                   // Increase pointer of the delay line by 2
      ._loop_d_pos_other_decim:

      j ._loop_fill_delay_other_decim                                               // skip the first iteration of the delay line filling routine

      main_loop_other_decim:

            // Fill the delay line (other decims)
            loopnez a13, ._loop_fill_delay_other_decim

                  blt	a12, a7, reset_fir_pos_other_decim                          //if(fir->pos >= fir->N){                                                 
                        movi.n	      a12, 0                                    // fir->pos = 0
                        l32i.n	      a9, a2, 4                                 // reset delay line to the beginning
                  reset_fir_pos_other_decim:  

                  l16si       a15, a3, 0                                            // load 16 bits from input a3 to a15
                  addi.n	a12, a12, 1                                           // fir->pos++
                  s16i        a15, a9, 0                                            // save 16 bits from a15 to delay line a9
                  addi.n      a3, a3, 2                                             // Increase pointer of the input array by 2
                  addi.n      a9, a9, 2                                             // Increase pointer of the delay line by 2
            ._loop_fill_delay_other_decim:            

            ee.ld.accx.ip           a10, 0                                          // load rounding value to accx

            sub	                  a15, a7, a12                                    // a15 = loop_len = fir->N - fir->pos
            ee.ld.128.usar.ip       q0, a9, 16                                      // Preload from delay
            muluh                   a14, a15, a11                                   // a14 = loop1_len = loop_len / 24
            ee.ld.128.usar.ip       q1, a9, 16                   

            movi.n                  a6, 24                                          // Move 24 to a6
            ee.vld.128.ip           q3, a8, 16                                      // preload from coeffs
            mul16s                  a6, a6, a14                                     // loop1_len * 24
            ee.src.q.ld.ip          q2, a9, 16, q0, q1                              // preload and shift from delay
            sub                     a6, a15, a6                                     // loop remiainder = a6 = loop_len - loop1_len *24
            
            loopnez a14, ._loop_end_1st_circular_buff_other_decim
                  ee.vld.128.ip                       q4, a8, 16                    // Load from coeffs   
                  ee.vmulas.s16.accx.ld.ip.qup        q0, a9, 16, q0, q3, q1, q2    // Load from delay
                  ee.vld.128.ip                       q5, a8, 16                    // Load from coeffs
                  ee.vmulas.s16.accx.ld.ip.qup        q1, a9, 16, q1, q4, q2, q0    // Load from delay
                  ee.vld.128.ip                       q3, a8, 16                    // Load from coeffs 
                  ee.vmulas.s16.accx.ld.ip.qup        q2, a9, 16, q2, q5, q0, q1    // Load from delay     
            ._loop_end_1st_circular_buff_other_decim:

            beqi        a6, 16, _other_decim_1st_equal_to_16                        // jump if the remainder is equal to 16
            bgei        a6, 16, _other_decim_1st_more_equal_to_16                   // jump if the remainder is greater or equal to 16
            beqi        a6, 8,  _other_decim_1st_equal_to_8                         // jump if the remainder is equal to 8
            bgei        a6, 8,  _other_decim_1st_more_equal_to_8                    // jump if the remainder is greater or equal to 8
            beqz        a6,     _other_decim_1st_equal_to_0                         // jump if the remainder is equal to 0
            bgez        a6,     _other_decim_1st_more_equal_to_0                    // jump if the remainder is greater or equal to 0

            _other_decim_1st_equal_to_16:
                  l32i.n      a9, a2, 4                                             // reset delay to the beginning

                  ee.vld.128.ip                 q4 ,a8, 16
                  ee.vmulas.s16.accx.ld.ip.qup  q0 ,a9, 16, q0, q3, q1, q2
                  ee.vmulas.s16.accx            q1, q4
                  j                             _1st_circular_buff_end_other_decim

            _other_decim_1st_more_equal_to_16:
                  ee.vld.128.ip                     q4, a8, 16
                  ee.vmulas.s16.accx.ld.ip.qup      q0, a9, 16, q0, q3, q1, q2

                  l32i.n      a9, a2, 4                                             // reset delay to the beginning
                  ee.vld.128.ip                     q5, a8, 16
                  
                  addx2       a6, a7, a9                                            // move delay pointer to the end, save the pointer to a6
                  ee.vmulas.s16.accx                q1, q4                          // MAC
                  
                  addi        a15, a6, -16                                          // move pointer back by 16, save the pointer to a15

                  ee.vld.128.ip           q2, a15, 16                               // load 8 words, not modifying the SAR_BYTE, load from a15 (end of the array)
                  ee.vld.128.ip           q0 ,a9, 16                                // load 8 words, not modifying the SAR_BYTE, load from a9  (beginning of the array)
                  ee.src.q.ld.ip          q1, a9, 16, q2, q0                        // shift by the amount of SAR_BYTE
                  ee.vmulas.s16.accx      q2, q5
                  blti                    a12, 8, _2nd_circular_buff_end_other_decim    // skip the second circular buffer if fir->pos is lower than 8 
                  j                       _1st_circular_buff_end_other_decim

            _other_decim_1st_equal_to_8:
                  ee.vmulas.s16.accx      q0, q3
                  j                       _1st_circular_buff_end_other_decim

            _other_decim_1st_more_equal_to_8:

                  l32i.n      a9, a2, 4                                             // reset delay to the beginning
                  ee.vld.128.ip       q4 , a8, 16

                  addx2       a6, a7, a9                                            // move delay pointer to the end, save the pointer to a6
                  ee.vmulas.s16.accx  q0, q3
                  
                  addi        a15, a6, -16                                          // move pointer back by 16, save the pointer to a15

                  ee.vld.128.ip           q1, a15, 16                               // load 8 words, not modifying the SAR_BYTE, load from a15 (end of the array)
                  ee.vld.128.ip           q2, a9, 16                                // load 8 words, not modifying the SAR_BYTE, load from a9  (beginning of the array)
                  ee.src.q.ld.ip          q0, a9, 16, q1, q2                        // shift by the amount of SAR_BYTE
                  ee.vmulas.s16.accx      q1, q4
                  blti                    a12, 8, _2nd_circular_buff_end_other_decim    // skip the second circular buffer if fir->pos is lower than 8 
                  j                       _1st_circular_buff_end_other_decim

            _other_decim_1st_more_equal_to_0:
                  l32i.n  	a9, a2, 4                                             // reset delay to the beginning
                  addx2       a6, a7, a9                                            // move delay pointer to the end, save pointer to a6
                  addi        a15, a6, -16                                          // move pointer back by 16, save the pointer to a15

                  ee.vld.128.ip           q0, a15, 16                               // load 8 words, not modifying the SAR_BYTE, load from a15 (end of the array)
                  ee.vld.128.ip           q1, a9, 16                                // load 8 words, not modifying the SAR_BYTE, load from a9  (beginning of the array)
                  ee.src.q.ld.ip          q2, a9, 16, q0, q1                        // shift by amount of SAR_BYTE
                  ee.vmulas.s16.accx      q0, q3
                  blti                    a12, 8, _2nd_circular_buff_end_other_decim    // skip the second circular buffer if fir->pos is lower than 8 
                  j                       _1st_circular_buff_end_other_decim

            _other_decim_1st_equal_to_0:
                  addi  a8, a8, -16                                                 // move coeffs pointer back by 16

            _1st_circular_buff_end_other_decim:

            // SECOND PART OF CIRCULAR BUFFER
            l32i.n	      a9, a2, 4                                             // reset delay to the beginning
            muluh             a14, a12, a11                                         // a14 = loop2_len = fir->pos / 24

            movi.n            a6, 24                                                // Move 24 to a6
            ee.vld.128.ip     q0, a9, 16                                            // Preload
            ee.vld.128.ip     q1, a9, 16
            ee.src.q.ld.ip    q2, a9, 16, q0, q1

            mul16s            a15, a6, a14                                          // loop1_len * 24
            ee.vld.128.ip     q3, a8, 16                                            // Preload
            sub               a6, a12, a15                                          // loop remiainder = a6 = fir->pos - loop1_len *24

            loopnez a14, ._loop_end_2nd_circular_buff_other_decim
                  ee.vld.128.ip                       q4, a8, 16                    // Load from coeffs   
                  ee.vmulas.s16.accx.ld.ip.qup        q0, a9, 16, q0, q3, q1, q2    // Load from delay
                  ee.vld.128.ip                       q5, a8, 16                    // Load from coeffs
                  ee.vmulas.s16.accx.ld.ip.qup        q1, a9, 16, q1, q4, q2, q0    // Load from delay
                  ee.vld.128.ip                       q3, a8, 16                    // Load from coeffs 
                  ee.vmulas.s16.accx.ld.ip.qup        q2, a9, 16, q2, q5, q0, q1    // Load from delay                                         
            ._loop_end_2nd_circular_buff_other_decim:
            
            bgei        a6, 16, _other_decim_2nd_more_equal_to_16                   // jump if the remainder is greater or equal to 16
            bgei        a6, 8,  _other_decim_2nd_more_equal_to_8                    // jump if the remainder is greater or equal to 8
            bgez        a6,     _2nd_circular_buff_end_other_decim                  // jump if the remainder is greater or equal to 0

            _other_decim_2nd_more_equal_to_16:
                  ee.vld.128.ip                     q4, a8, 16
                  ee.vmulas.s16.accx.ld.ip.qup      q0, a9, 16, q0, q3, q1, q2
                  ee.vmulas.s16.accx                q1, q4
                  j                                 _2nd_circular_buff_end_other_decim

            _other_decim_2nd_more_equal_to_8:
                  ee.vmulas.s16.accx                q0, q3

            _2nd_circular_buff_end_other_decim:

            l32i.n            a6, a1, 4                                             // load final shift value to a6
            l32i.n	      a8, a2, 0                                             // reset coeffs to the beginning
            
            bgez              a6, _shift_right_other_decim
            rur.accx_0        a9                                                    // acc low
            rur.accx_1        a14                                                   // acc high
            addi.n            a5, a5, -1                                            // decrease counter
            src               a15, a9, a14                                          // funnel shift left, save 32bits to a15
            j                 _shift_left_other_decim

            _shift_right_other_decim:
            ee.srs.accx       a15, a6, 0                                            // shift accx register by final_shift amount (a6), save the lower 32bits to a15
            addi.n            a5, a5, -1                                            // decrease counter

            _shift_left_other_decim:
            s16i	            a15, a4, 0                                            // save the final acc value to the output                   
            l32i.n	      a9, a2, 4                                             // reset delay to the beginning
            addi.n	      a4, a4, 2                                             // increase pointer p_output++ 
            addx2             a9, a12, a9                                           // p_delay[fir->pos] - (two times the fir->pos)
            bnez.n            a5, main_loop_other_decim

      l32i.n  a2, a1, 0                                                             // load saved return value from a1 to a2
      retw.n


#endif // dsps_fird_s16_aes3_enabled