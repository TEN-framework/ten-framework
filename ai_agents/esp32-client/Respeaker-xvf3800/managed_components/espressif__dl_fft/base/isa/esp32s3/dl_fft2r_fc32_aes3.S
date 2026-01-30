/*
 * SPDX-FileCopyrightText: 2018-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: 2024 f4lcOn @ Libera Chat IRC
 *
 * SPDX-License-Identifier: Apache-2.0
 */


	.text
	.align  4
	.global dl_fft2r_fc32_aes3_
	.type   dl_fft2r_fc32_aes3_,@function

// The function implements the following C code:
//esp_err_t dl_fft2r_fc32_ansi(float *data, int N)
//{
//    float *w = dl_fft_w_table_fc32;
//
//    int ie, ia, m;
//    float re_temp, im_temp;
//    float c, s;
//    int N2 = N;
//    ie = 1;
//    for (int N2 = N/2; N2 > 0; N2 >>= 1) {
//        ia = 0;
//        for (int j = 0; j < ie; j++) {
//            c = w[2 * j];
//            s = w[2 * j + 1];
//            for (int i = 0; i < N2; i++) {
//                m = ia + N2;
//                re_temp = c * data[2 * m] + s * data[2 * m + 1];
//                im_temp = c * data[2 * m + 1] - s * data[2 * m];
//                data[2 * m] = data[2 * ia] - re_temp;
//                data[2 * m + 1] = data[2 * ia + 1] - im_temp;
//                data[2 * ia] = data[2 * ia] + re_temp;
//                data[2 * ia + 1] = data[2 * ia + 1] + im_temp;
//                ia++;
//            }
//            ia += N2;
//        }
//        ie <<= 1;
//    }
//    return result;
//}


dl_fft2r_fc32_aes3_:
//esp_err_t dl_fft2r_fc32_ansi(float *data, int N, float* dl_fft_w_table_fc32)

	entry	a1, 16

// Array increment for floating point data should be 4
// data - a2
// N - a3
// dl_fft_w_table_fc32 - a4

// a6 - k, main loop counter; N2 - for (int N2 = N/2; N2 > 0; N2 >>= 1)
// a7 - ie
// a8 - j
// a10 - (j*2)<<2,  or a10 - j<<3
// f0 - c or w[2 * j]
// f1 - s or w[2 * j + 1]
// a11 - ia
// a12 - m
// a13 - ia pointer
// a14 - m pointer
// f6  - re_temp
// f7  - im_temp

    srli a6, a3, 1 // a6 = N2 = N/2
    movi.n a7, 1   // a7 - ie

.ifft2r_l1:
    movi.n a8, 0   // a8 - j
    movi.n a11,0   // a11 = ia = 0;

.ifft2r_l2:         // loop for j, a8 - j
        addx8    a10, a8, a4 // a8 - shift for cos () -- c = w[2 * j]; -- pointer to cos
        ee.ldf.64.ip f1, f0, a10, 0
        add.n    a12, a11, a6   // a12 = m = ia + N2
        addx8    a14, a12, a2   // a14 - pointer for m*2

        loopnez a6, .ifft2r_l3
            ee.ldf.64.ip f5, f4, a14, 0 // data[2 * m], data[2 * m + 1]
            mul.s    f6, f0, f4     // re_temp =  c * data[2 * m]
            mul.s    f7, f0, f5     // im_temp =  c * data[2 * m + 1]

            addx8    a13, a11, a2   // a13 - pointer for ia*2
            ee.ldf.64.ip f3, f2, a13, 0 // data[2 * ia], data[2 * ia + 1]

            madd.s   f6, f1, f5     // re_temp += s * data[2 * m + 1];
            msub.s   f7, f1, f4     // im_temp -= s * data[2 * m];

            addi     a11, a11, 1    // ia++
            add.n    a12, a11, a6   // a12 = m = ia + N2

            sub.s    f8, f2, f6     // = data[2 * ia] - re_temp;
            sub.s    f9, f3, f7     // = data[2 * ia + 1] - im_temp;
            add.s    f10, f2, f6    // = data[2 * ia] + re_temp;
            add.s    f11, f3, f7    // = data[2 * ia + 1] + im_temp;

            ee.stf.64.ip f9, f8, a14, 0
            addx8    a14, a12, a2   // a14 - pointer for m*2
            ee.stf.64.ip f11, f10, a13, 0
.ifft2r_l3:
        add.n   a11, a11, a6

        addi.n  a8, a8, 1 // j++
        bne     a8, a7, .ifft2r_l2
    slli    a7, a7, 1  // ie = ie<<1
// main loop: for (int k = N/2; k > 0; k >>= 1)
    srli    a6, a6, 1     // a6 = a6>>1
    bnez    a6, .ifft2r_l1 // Jump if > 0

	retw




	.text
	.align  4
	.global dl_ifft2r_fc32_aes3_
	.type   dl_ifft2r_fc32_aes3_,@function


dl_ifft2r_fc32_aes3_:
//esp_err_t dl_fft2r_fc32_ansi(float *data, int N, float* dl_fft_w_table_fc32)

	entry	a1, 16

// Array increment for floating point data should be 4
// data - a2
// N - a3
// dl_fft_w_table_fc32 - a4

// a6 - k, main loop counter; N2 - for (int N2 = N/2; N2 > 0; N2 >>= 1)
// a7 - ie
// a8 - j
// a10 - (j*2)<<2,  or a10 - j<<3
// f0 - c or w[2 * j]
// f1 - s or w[2 * j + 1]
// a11 - ia
// a12 - m
// a13 - ia pointer
// a14 - m pointer
// f6  - re_temp
// f7  - im_temp

    srli a6, a3, 1 // a6 = N2 = N/2
    movi.n a7, 1   // a7 - ie

.fft2r_l1:
    movi.n a8, 0   // a8 - j
    movi.n a11,0   // a11 = ia = 0;

.fft2r_l2:         // loop for j, a8 - j
        addx8    a10, a8, a4 // a8 - shift for cos () -- c = w[2 * j]; -- pointer to cos
        ee.ldf.64.ip f1, f0, a10, 0

        // CHANGE: Negate the imaginary part of twiddle factors
        neg.s    f1, f1
        add.n    a12, a11, a6   // a12 = m = ia + N2
        addx8    a14, a12, a2   // a14 - pointer for m*2

        loopnez a6, .fft2r_l3
            ee.ldf.64.ip f5, f4, a14, 0 // data[2 * m], data[2 * m + 1]
            mul.s    f6, f0, f4     // re_temp =  c * data[2 * m]
            mul.s    f7, f0, f5     // im_temp =  c * data[2 * m + 1]

            addx8    a13, a11, a2   // a13 - pointer for ia*2
            ee.ldf.64.ip f3, f2, a13, 0 // data[2 * ia], data[2 * ia + 1]

            madd.s   f6, f1, f5     // re_temp += s * data[2 * m + 1];
            msub.s   f7, f1, f4     // im_temp -= s * data[2 * m];

            addi     a11, a11, 1    // ia++
            add.n    a12, a11, a6   // a12 = m = ia + N2

            sub.s    f8, f2, f6     // = data[2 * ia] - re_temp;
            sub.s    f9, f3, f7     // = data[2 * ia + 1] - im_temp;
            add.s    f10, f2, f6    // = data[2 * ia] + re_temp;
            add.s    f11, f3, f7    // = data[2 * ia + 1] + im_temp;

            ee.stf.64.ip f9, f8, a14, 0
            addx8    a14, a12, a2   // a14 - pointer for m*2
            ee.stf.64.ip f11, f10, a13, 0
.fft2r_l3:
        add.n   a11, a11, a6

        addi.n  a8, a8, 1 // j++
        bne     a8, a7, .fft2r_l2
    slli    a7, a7, 1  // ie = ie<<1
// main loop: for (int k = N/2; k > 0; k >>= 1)
    srli    a6, a6, 1     // a6 = a6>>1
    bnez    a6, .fft2r_l1 // Jump if > 0

	retw
