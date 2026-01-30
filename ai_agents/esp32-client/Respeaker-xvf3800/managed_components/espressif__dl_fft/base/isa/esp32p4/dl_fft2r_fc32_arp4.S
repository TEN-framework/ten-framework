// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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


// This is matrix multipliction function for esp32p4 processor.
    .text
    .align  4
    .global dl_fft2r_fc32_arp4_
    .type   dl_fft2r_fc32_arp4_,@function

dl_fft2r_fc32_arp4_:
//esp_err_t dl_fft2r_fc32_arp4_(float *data, int N, float* dl_fft_w_table_fc32)

    add sp,sp,-16
#
    srli t6, a1, 1 // a6 = N2 = N/2
    li   t0, 1     // a7 - ie

.fft2r_l1:
        li t1, 0    // a8 - j
        li t4, 0    // a11 = ia = 0;

.fft2r_l2:          // loop for j, a8 - j

            slli    t3, t1, 3   // a10 = j<<3 // shift for cos ()   -- c = w[2 * j];
            add     t3, t3, a2  // a10 - pointer to cos
            flw     fa0, 0(t3)
            flw     fa1, 4(t3)

            esp.lp.setup    0, t6, .fft2r_l3    // .fft2r_l3 - label to the last executed instruction
                add      t5, t4, t6   // a12 = m = ia + N2

                slli     a4, t5, 3    // a14 - pointer for m*2
                slli     a3, t4, 3    // a13 - pointer for ia*2
                add      a4, a4, a0   // pointers to data arrays
                add      a3, a3, a0   //

                flw  fa4, 0(a4)
                flw  fa5, 4(a4)
                flw  fa2, 0(a3)
                flw  fa3, 4(a3)

                fmul.s          ft6, fa0, fa4       // re_temp =  c * data[2 * m]
                fmul.s          ft7, fa0, fa5       // im_temp =  c * data[2 * m + 1]
                fmadd.s         ft6, fa1, fa5, ft6  // re_temp += s * data[2 * m + 1];
                fnmsub.s        ft7, fa1, fa4, ft7  // im_temp -= s * data[2 * m];
                fsub.s          ft8, fa2, ft6       // = data[2 * ia] - re_temp;
                fsub.s          ft9, fa3, ft7       // = data[2 * ia + 1] - im_temp;

                fadd.s   ft10, fa2, ft6     // = data[2 * ia] + re_temp;
                fadd.s   ft11, fa3, ft7     // = data[2 * ia + 1] + im_temp;

                fsw      ft8, 0(a4)
                fsw      ft9, 4(a4)
                fsw      ft10, 0(a3)
                fsw      ft11, 4(a3)

.fft2r_l3:      add     t4, t4, 1       // ia++

            add     t4, t4, t6
            add     t1, t1, 1           // j++

            BNE  t1, t0, .fft2r_l2
            slli    t0, t0, 1  // ie = ie<<1
            srli    t6, t6, 1  // a6 = a6>>1
        BNEZ    t6, .fft2r_l1// Jump if > 0

#
        add sp,sp,16
        li  a0,0
        ret


// This is matrix multipliction function for esp32p4 processor.
    .text
    .align  4
    .global dl_ifft2r_fc32_arp4_
    .type   dl_ifft2r_fc32_arp4_,@function

dl_ifft2r_fc32_arp4_:
//esp_err_t dl_ifft2r_fc32_arp4_(float *data, int N, float* dl_fft_w_table_fc32)

    add sp,sp,-16
#
    srli t6, a1, 1 // a6 = N2 = N/2
    li   t0, 1     // a7 - ie

.ifft2r_l1:
        li t1, 0    // a8 - j
        li t4, 0    // a11 = ia = 0;

.ifft2r_l2:          // loop for j, a8 - j

            slli    t3, t1, 3   // a10 = j<<3 // shift for cos ()   -- c = w[2 * j];
            add     t3, t3, a2  // a10 - pointer to cos
            flw     fa0, 0(t3)
            flw     fa1, 4(t3)
            // CHANGE: Negate the imaginary part of twiddle factors (complex conjugate)
            fneg.s  fa1, fa1    // s = -s (since w^-1 = w*)

            esp.lp.setup    0, t6, .ifft2r_l3    // .fft2r_l3 - label to the last executed instruction
                add      t5, t4, t6   // a12 = m = ia + N2

                slli     a4, t5, 3    // a14 - pointer for m*2
                slli     a3, t4, 3    // a13 - pointer for ia*2
                add      a4, a4, a0   // pointers to data arrays
                add      a3, a3, a0   //

                flw  fa4, 0(a4)
                flw  fa5, 4(a4)
                flw  fa2, 0(a3)
                flw  fa3, 4(a3)

                fmul.s          ft6, fa0, fa4       // re_temp =  c * data[2 * m]
                fmul.s          ft7, fa0, fa5       // im_temp =  c * data[2 * m + 1]
                fmadd.s         ft6, fa1, fa5, ft6  // re_temp += s * data[2 * m + 1];
                fnmsub.s        ft7, fa1, fa4, ft7  // im_temp -= s * data[2 * m];
                fsub.s          ft8, fa2, ft6       // = data[2 * ia] - re_temp;
                fsub.s          ft9, fa3, ft7       // = data[2 * ia + 1] - im_temp;

                fadd.s   ft10, fa2, ft6     // = data[2 * ia] + re_temp;
                fadd.s   ft11, fa3, ft7     // = data[2 * ia + 1] + im_temp;

                fsw      ft8, 0(a4)
                fsw      ft9, 4(a4)
                fsw      ft10, 0(a3)
                fsw      ft11, 4(a3)

.ifft2r_l3:      add     t4, t4, 1       // ia++

            add     t4, t4, t6
            add     t1, t1, 1           // j++

            BNE  t1, t0, .ifft2r_l2
            slli    t0, t0, 1  // ie = ie<<1
            srli    t6, t6, 1  // a6 = a6>>1
        BNEZ    t6, .ifft2r_l1// Jump if > 0

#
        add sp,sp,16
        li  a0,0
        ret
