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

        .text
        .align  4
        .global dl_fft4r_fc32_arp4_
        .type   dl_fft4r_fc32_arp4_,@function

dl_fft4r_fc32_arp4_:
//esp_err_t dl_fft4r_fc32_arp4_(float *data, int N, float *table, int table_size)

// table_size - a3
// m - t0
// j - t1
    add sp,sp,-16
#
    srli t6, a1, 1 // t6 = log4N = N/2
    li   t0, 2     // t0 - m

    div  a3, a3, a1 // wind_step = table_size / N
    slli a3, a3, 3 // wind_step = complex step = 8 bytes

.fft2r_l1:
        li t1, 0    // t1 - j
        srli    a1, a1, 2 // a1 = length = length >> 2;
.fft2r_l2:          // loop for j, t1 - j
            slli    t2, a1, 4    // t2 = length << 1 << 3 (8 bytes for one complex sample)
            slli    t3, a1, 3    // t2 = length << 1 << 3 (8 bytes for one complex sample)
            // start_index = j * (length << 1); // n: n-point FFT
            mul     t2,t2,t1
            add     a4, a0, t2 // fc32_t *ptrc0
            add     a5, a4, t3 // fc32_t *ptrc1
            add     a6, a5, t3 // fc32_t *ptrc2
            add     a7, a6, t3 // fc32_t *ptrc3

            # flw        fa0, 0(a4)
            # fsw        fa0, 0(t3)
            # add t3, t3, 4
            mv      t2, a2  // winc0
            mv      t3, a2  // winc0
            mv      t4, a2  // winc0

            esp.lp.setup    0, a1, .fft2r_l3    // .fft2r_l3 - label to the last executed instruction

                    flw  fa0, 0(a4) // in0.re
                    flw  fa4, 0(a6) // in2.re
                    fadd.s      ft0, fa0, fa4   // in0.re + in2.re
                    flw  fa1, 4(a4) // in0.im
                    fsub.s      ft1, fa0, fa4   // in0.re - in2.re
                    flw  fa5, 4(a6) // in2.im
                    fadd.s      ft2, fa1, fa5   // in0.im + in2.im
                    flw  fa2, 0(a5) // in1.re
                    fsub.s      ft3, fa1, fa5   // in0.im - in2.im
                    flw  fa6, 0(a7) // in3.re
                    fadd.s      ft4, fa2, fa6   // in1.re + in3.re
                    flw  fa3, 4(a5) // in1.im
                    fsub.s      ft5, fa2, fa6   // in1.re - in3.re
                    flw  fa7, 4(a7) // in3.im
                    fadd.s      ft6, fa3, fa7   // in1.im + in3.im
                    fsub.s      ft7, fa3, fa7   // in1.im - in3.im

                    # bfly[0].re = ft0 + ft4;
                    fadd.s      fa0, ft0, ft4;
                    # bfly[0].im = ft2 + ft6;
                    fadd.s      fa1, ft2, ft6;
                    # bfly[1].re = ft1 + ft7;
                    fadd.s      fa2, ft1, ft7;
                    # bfly[1].im = ft3 - ft5;
                    fsub.s      fa3, ft3, ft5;
                    # bfly[2].re = ft0 - ft5;
                    fsub.s      fa4, ft0, ft4;
                    flw         ft0, 0(t2)     // winc0->re
                    # bfly[2].im = ft2 - ft7;
                    fsub.s      fa5, ft2, ft6;
                    flw  ft2, 0(t3)     // winc1->re
                    # bfly[3].re = ft1 - ft6;
                    fsub.s      fa6, ft1, ft7;
                    flw  ft1, 4(t2)     // winc0->im
                    # bfly[3].im = ft3 + ft5;
                    fadd.s      fa7, ft3, ft5;

                    // *ptrc0 = bfly[0];
                    fsw  fa0, 0(a4) // in0.re
                    fsw  fa1, 4(a4) // in0.im

                    flw  ft3, 4(t3)     // winc1->im

                    // ptrc1->re = bfly[1].re * winc0->re + bfly[1].im * winc0->im;
                    // ptrc1->im = bfly[1].im * winc0->re - bfly[1].re * winc0->im;
                    // ptrc2->re = bfly[2].re * winc1->re + bfly[2].im * winc1->im;
                    fmul.s  fa0, fa2, ft0
                    add t2, t2, a3 // winc0 += 1 * wind_step;
                    fmul.s  fa1, fa3, ft0
                    fmul.s  ft0, fa4, ft2
                    fmul.s  ft2, fa5, ft2

                    flw  ft4, 0(t4)     // winc2->re
                    flw  ft5, 4(t4)     // winc3->im

                    fmadd.s fa0, fa3, ft1, fa0
                    add t3, t3, a3 // winc1 += 2 * wind_step;
                    fnmsub.s fa1, fa2, ft1, fa1
                    add t3, t3, a3 //
                    fmul.s  fa2, fa6, ft4
                    fmul.s  fa3, fa7, ft4


                    add t4, t4, a3 // winc2 += 3 * wind_step;
                    fmadd.s ft0, fa5, ft3, ft0
                    add t4, t4, a3 //
                    fnmsub.s ft2, fa4, ft3, ft2

                    fmadd.s ft3, fa7, ft5, fa2
                    add t4, t4, a3 //
                    fnmsub.s fa3, fa6, ft5, fa3

                    fsw  fa0, 0(a5) // in1.re
                    add a4, a4, 8
                    fsw  fa1, 4(a5) // in1.im
                    add a5, a5, 8
                    fsw  ft0, 0(a6) // in2.re
                    // ptrc2->im = bfly[2].im * winc1->re - bfly[2].re * winc1->im;
                    fsw  ft2, 4(a6) // in2.re
                    // ptrc3->re = bfly[3].re * winc2->re + bfly[3].im * winc2->im;
                    add a6, a6, 8

                    fsw  ft3, 0(a7) // in2.re
                    // ptrc3->im = bfly[3].im * winc2->re - bfly[3].re * winc2->im;
                    fsw  fa3, 4(a7) // in2.re

                    add a7, a7, 8

                    // Temp solution

.fft2r_l3:      nop
            add     t1, t1, 2           // j+=2
            BNE  t1, t0, .fft2r_l2

            slli    t0, t0, 2  // t0 = m = m<<2
            srli    t6, t6, 2  // t6 = log4N >>= 2
            slli    a3, a3, 2  // wind_step = wind_step << 2;
        BNEZ    t6, .fft2r_l1// Jump if > 0

#
        add     sp,sp,16
        li  a0,0
        ret


        .text
        .align  4
        .global dl_ifft4r_fc32_arp4_
        .type   dl_ifft4r_fc32_arp4_,@function

dl_ifft4r_fc32_arp4_:
//esp_err_t dl_ifft4r_fc32_arp4_(float *data, int N, float *table, int table_size)

// table_size - a3
// m - t0
// j - t1
    add sp,sp,-16
#
    srli t6, a1, 1 // t6 = log4N = N/2
    li   t0, 2     // t0 - m

    div  a3, a3, a1 // wind_step = table_size / N
    slli a3, a3, 3 // wind_step = complex step = 8 bytes

.ifft2r_l1:
        li t1, 0    // t1 - j
        srli    a1, a1, 2 // a1 = length = length >> 2;
.ifft2r_l2:          // loop for j, t1 - j
            slli    t2, a1, 4    // t2 = length << 1 << 3 (8 bytes for one complex sample)
            slli    t3, a1, 3    // t2 = length << 1 << 3 (8 bytes for one complex sample)
            // start_index = j * (length << 1); // n: n-point FFT
            mul     t2,t2,t1
            add     a4, a0, t2 // fc32_t *ptrc0
            add     a5, a4, t3 // fc32_t *ptrc1
            add     a6, a5, t3 // fc32_t *ptrc2
            add     a7, a6, t3 // fc32_t *ptrc3

            # flw        fa0, 0(a4)
            # fsw        fa0, 0(t3)
            # add t3, t3, 4
            mv      t2, a2  // winc0
            mv      t3, a2  // winc0
            mv      t4, a2  // winc0

            esp.lp.setup    0, a1, .ifft2r_l3    // .ifft2r_l3 - label to the last executed instruction

                    flw  fa0, 0(a4) // in0.re
                    flw  fa4, 0(a6) // in2.re
                    fadd.s      ft0, fa0, fa4   // in0.re + in2.re
                    flw  fa1, 4(a4) // in0.im
                    fsub.s      ft1, fa0, fa4   // in0.re - in2.re
                    flw  fa5, 4(a6) // in2.im
                    fadd.s      ft2, fa1, fa5   // in0.im + in2.im
                    flw  fa2, 0(a5) // in1.re
                    fsub.s      ft3, fa1, fa5   // in0.im - in2.im
                    flw  fa6, 0(a7) // in3.re
                    fadd.s      ft4, fa2, fa6   // in1.re + in3.re
                    flw  fa3, 4(a5) // in1.im
                    fsub.s      ft5, fa2, fa6   // in1.re - in3.re
                    flw  fa7, 4(a7) // in3.im
                    fadd.s      ft6, fa3, fa7   // in1.im + in3.im
                    fsub.s      ft7, fa3, fa7   // in1.im - in3.im

                    # bfly[0].re = ft0 + ft4;
                    fadd.s      fa0, ft0, ft4;
                    # bfly[0].im = ft2 + ft6;
                    fadd.s      fa1, ft2, ft6;
                    # bfly[1].re = ft1 - ft7;
                    fsub.s      fa2, ft1, ft7;
                    # bfly[1].im = ft3 + ft5;
                    fadd.s      fa3, ft3, ft5;
                    # bfly[2].re = ft0 - ft5;
                    fsub.s      fa4, ft0, ft4;
                    flw         ft0, 0(t2)     // winc0->re
                    # bfly[2].im = ft2 - ft7;
                    fsub.s      fa5, ft2, ft6;
                    flw  ft2, 0(t3)     // winc1->re
                    # bfly[3].re = ft1 + ft6;
                    fadd.s      fa6, ft1, ft7;
                    flw  ft1, 4(t2)     // winc0->im
                    # bfly[3].im = ft3 - ft5;
                    fsub.s      fa7, ft3, ft5;

                    // *ptrc0 = bfly[0];
                    fsw  fa0, 0(a4) // in0.re
                    fsw  fa1, 4(a4) // in0.im

                    flw  ft3, 4(t3)     // winc1->im

                    // ptrc1->re = bfly[1].re * winc0->re + bfly[1].im * winc0->im;
                    // ptrc1->im = bfly[1].im * winc0->re - bfly[1].re * winc0->im;
                    // ptrc2->re = bfly[2].re * winc1->re + bfly[2].im * winc1->im;
                    fmul.s  fa0, fa2, ft0
                    add t2, t2, a3 // winc0 += 1 * wind_step;
                    fmul.s  fa1, fa3, ft0
                    fmul.s  ft0, fa4, ft2
                    fmul.s  ft2, fa5, ft2

                    flw  ft4, 0(t4)     // winc2->re
                    flw  ft5, 4(t4)     // winc3->im

                    fnmsub.s fa0, fa3, ft1, fa0
                    add t3, t3, a3 // winc1 += 2 * wind_step;
                    fmadd.s fa1, fa2, ft1, fa1
                    add t3, t3, a3 //
                    fmul.s  fa2, fa6, ft4
                    fmul.s  fa3, fa7, ft4


                    add t4, t4, a3 // winc2 += 3 * wind_step;
                    fnmsub.s ft0, fa5, ft3, ft0
                    add t4, t4, a3 //
                    fmadd.s ft2, fa4, ft3, ft2

                    fnmsub.s ft3, fa7, ft5, fa2
                    add t4, t4, a3 //
                    fmadd.s fa3, fa6, ft5, fa3

                    fsw  fa0, 0(a5) // in1.re
                    add a4, a4, 8
                    fsw  fa1, 4(a5) // in1.im
                    add a5, a5, 8
                    fsw  ft0, 0(a6) // in2.re
                    // ptrc2->im = bfly[2].im * winc1->re - bfly[2].re * winc1->im;
                    fsw  ft2, 4(a6) // in2.re
                    // ptrc3->re = bfly[3].re * winc2->re + bfly[3].im * winc2->im;
                    add a6, a6, 8

                    fsw  ft3, 0(a7) // in2.re
                    // ptrc3->im = bfly[3].im * winc2->re - bfly[3].re * winc2->im;
                    fsw  fa3, 4(a7) // in2.re

                    add a7, a7, 8

                    // Temp solution

.ifft2r_l3:      nop
            add     t1, t1, 2           // j+=2
            BNE  t1, t0, .ifft2r_l2

            slli    t0, t0, 2  // t0 = m = m<<2
            srli    t6, t6, 2  // t6 = log4N >>= 2
            slli    a3, a3, 2  // wind_step = wind_step << 2;
        BNEZ    t6, .ifft2r_l1// Jump if > 0

#
        add     sp,sp,16
        li  a0,0
        ret
