/*
 * SPDX-FileCopyrightText: 2018-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: 2024 f4lcOn @ Libera Chat IRC
 *
 * SPDX-License-Identifier: Apache-2.0
 */



	.section .text # placed in IRAM instead of FLASH .text
	.global dl_fft4r_fc32_ae32_
	.type   dl_fft4r_fc32_ae32_,@function

// The function implements the following C code:
// esp_err_t dl_fft4r_fc32_ansi_(float *data, int length, float *table, int table_size)
// {
//     if (0 == dl_fft4r_initialized) {
//         return ESP_ERR_DSP_UNINITIALIZED;
//     }
//
//     uint log2N = dl_power_of_two(length);
//     if ((log2N & 0x01) != 0) {
//         return ESP_ERR_DSP_INVALID_LENGTH;
//     }
//     uint log4N = log2N >> 1;
//
//     fc32_t bfly[4];
//     uint m = 2;
//     uint wind_step = table_size / length;
//     while (1) {  ///radix 4
//         if (log4N == 0) {
//             break;
//         }
//         length = length >> 2;
//         for (int j = 0; j < m; j += 2) { // j: which FFT of this step
//             int start_index = j * (length << 1); // n: n-point FFT
//
//             fc32_t *ptrc0 = (fc32_t *)data + start_index;
//             fc32_t *ptrc1 = ptrc0 + length;
//             fc32_t *ptrc2 = ptrc1 + length;
//             fc32_t *ptrc3 = ptrc2 + length;
//
//             fc32_t *winc0 = (fc32_t *)table;
//             fc32_t *winc1 = winc0;
//             fc32_t *winc2 = winc0;
//
//             for (int k = 0; k < length; k++) {
//                 fc32_t in0 = *ptrc0;
//                 fc32_t in2 = *ptrc2;
//                 fc32_t in1 = *ptrc1;
//                 fc32_t in3 = *ptrc3;
//
//                 bfly[0].re = in0.re + in2.re + in1.re + in3.re;
//                 bfly[0].im = in0.im + in2.im + in1.im + in3.im;
//
//                 bfly[1].re = in0.re - in2.re + in1.im - in3.im;
//                 bfly[1].im = in0.im - in2.im - in1.re + in3.re;
//
//                 bfly[2].re = in0.re + in2.re - in1.re - in3.re;
//                 bfly[2].im = in0.im + in2.im - in1.im - in3.im;
//
//                 bfly[3].re = in0.re - in2.re - in1.im + in3.im;
//                 bfly[3].im = in0.im - in2.im + in1.re - in3.re;
//
//                 *ptrc0 = bfly[0];
//                 ptrc1->re = bfly[1].re * winc0->re + bfly[1].im * winc0->im;
//                 ptrc1->im = bfly[1].im * winc0->re - bfly[1].re * winc0->im;
//                 ptrc2->re = bfly[2].re * winc1->re + bfly[2].im * winc1->im;
//                 ptrc2->im = bfly[2].im * winc1->re - bfly[2].re * winc1->im;
//                 ptrc3->re = bfly[3].re * winc2->re + bfly[3].im * winc2->im;
//                 ptrc3->im = bfly[3].im * winc2->re - bfly[3].re * winc2->im;
//
//                 winc0 += 1 * wind_step;
//                 winc1 += 2 * wind_step;
//                 winc2 += 3 * wind_step;
//
//                 ptrc0++;
//                 ptrc1++;
//                 ptrc2++;
//                 ptrc3++;
//             }
//         }
//         m = m << 2;
//         wind_step = wind_step << 2;
//         log4N--;
//     }
//     return ESP_OK;
// }

// esp_err_t dl_fft4r_fc32_ae32_(data, N, dl_fft4r_w_table_fc32, dl_fft4r_w_table_size)

//.ret_DSP_INVALID_LENGTH:
//	movi.n   a2, ESP_ERR_DSP_INVALID_LENGTH
//	retw.n

	.align  4
dl_fft4r_fc32_ae32_:

	entry   a1, 16        # no auto vars on stack

//	bltui   a3, 4, .ret_DSP_INVALID_LENGTH # if N < 4 : return(ESP_ERR_DSP_INVALID_LENGTH)

//	addi.n  a6, a3, -1
//	and     a6, a3, a6
//	bnez    a6, .ret_DSP_INVALID_LENGTH # if N not power of 2 : return(ESP_ERR_DSP_INVALID_LENGTH)

	nsau    a6, a3        # inline dl_power_of_two(N)
	movi.n  a7, 31
	xor     a6, a6, a7

//	bbsi    a6, 0, .ret_DSP_INVALID_LENGTH # if N not power of 4 : return(ESP_ERR_DSP_INVALID_LENGTH)

	srli    a7, a6, 1     # log4N = dl_power_of_two(N) >> 1;

	addi.n  a6, a6, -1
	ssr 		a6
	srl     a6, a5        # w_step = table_size >> (dl_power_of_two(N) - 1)

	movi.n  a5, 2         # m = 2

.stage:
	srli    a3, a3, 2     # N >>= 2

	movi.n  a8, 0         # j = 0

.group:
	mov.n   a9, a4        # w0 = w
	mov.n   a10, a4       # w1 = w
	mov.n   a11, a4       # w2 = w

	mul16u  a12, a8, a3
	slli    a12, a12, 1   # start_index = (j * N) << 1

	addx8   a12, a12, a2  # p0 = data + (start_index << 1)
	addx8   a13, a3, a12  # p1 = p0 + (N << 1)
	addx8   a14, a3, a13  # p2 = p1 + (N << 1)
	addx8   a15, a3, a14  # p3 = p2 + (N << 1)

	loopnez a3, .bf4_loop_end # for (uint k = 0; k < N; k++)
	lsi     f1, a12, 4    #  f1 = in0.im = *(p0 + 1)
	lsi     f3, a14, 4    #  f3 = in2.im = *(p2 + 1)
	lsi     f0, a12, 0    #  f0 = in0.re = *p0
	lsi     f2, a14, 0    #  f2 = in2.re = *p2
	add.s   f5, f1, f3    #  f5 = in0.im + in2.im
	sub.s   f7, f1, f3    #  f7 = in0.im - in2.im
	lsi     f1, a13, 4    #  f1 = in1.im = *(p1 + 1)
	lsi     f3, a15, 4    #  f3 = in3.im = *(p3 + 1)
	add.s   f4, f0, f2    #  f4 = in0.re + in2.re
	sub.s   f6, f0, f2    #  f6 = in0.re - in2.re
	add.s   f9, f1, f3    #  f9 = in1.im + in3.im
	sub.s   f11, f1, f3   # f11 = in1.im - in3.im
	lsi     f0, a13, 0    #  f0 = in1.re = *p1
	lsi     f2, a15, 0    #  f2 = in3.re = *p3
	lsi     f12, a9, 0    # f12 = w0->re
	lsi     f13, a10, 0   # f13 = w1->re
	lsi     f14, a11, 0   # f14 = w2->re
	add.s   f8, f0, f2    #  f8 = in1.re + in3.re
	sub.s   f10, f0, f2   # f10 = in1.re - in3.re
	sub.s   f1, f5, f9    #  f1 = bf2.im = in0.im + in2.im - in1.im - in3.im
	add.s   f5, f5, f9    #  f5 = bf0.im = in0.im + in2.im + in1.im + in3.im
	add.s   f2, f6, f11   #  f2 = bf1.re = in0.re - in2.re + in1.im - in3.im
	sub.s   f6, f6, f11   #  f6 = bf3.re = in0.re - in2.re - in1.im + in3.im
	sub.s   f0, f4, f8    #  f0 = bf2.re = in0.re + in2.re - in1.re - in3.re
	add.s   f4, f4, f8    #  f4 = bf0.re = in0.re + in2.re + in1.re + in3.re
	sub.s   f3, f7, f10   #  f3 = bf1.im = in0.im - in2.im - in1.re + in3.re
	add.s   f7, f7, f10   #  f7 = bf3.im = in0.im - in2.im + in1.re - in3.re
	ssi     f5, a12, 4    # *(p0 + 1) = f5 = bf0.im
	ssip    f4, a12, 8    # *p0 = f4 = bf0.re , p0 += 2
	mul.s   f5, f3, f12   #  f5 = bf1.im * w0->re
	mul.s   f4, f2, f12   #  f4 = bf1.re * w0->re
	mul.s   f9, f1, f13   #  f9 = bf2.im * w1->re
	mul.s   f8, f0, f13   #  f8 = bf2.re * w1->re
	mul.s   f11, f7, f14  # f11 = bf3.im * w2->re
	mul.s   f10, f6, f14  # f10 = bf3.re * w2->re
	lsi     f12, a9, 4    # f12 = w0->im
	lsi     f13, a10, 4   # f13 = w1->im
	lsi     f14, a11, 4   # f14 = w2->im
	addx4   a9, a6, a9    # w0 += m
	addx8   a10, a6, a10  # w1 += 2 * m
	addx4   a11, a6, a11
	addx8   a11, a6, a11  # w2 += 3 * m
	msub.s  f5, f2, f12   #  f5 = bf1.im * w0->re - bf1.re * w0->im
	madd.s  f4, f3, f12   #  f4 = bf1.re * w0->re + bf1.im * w0->im
	msub.s  f9, f0, f13   #  f9 = bf2.im * w1->re - bf2.re * w1->im
	madd.s  f8, f1, f13   #  f8 = bf2.re * w1->re + bf2.im * w1->im
	msub.s  f11, f6, f14  # f11 = bf3.im * w2->re - bf3.re * w2->im
	madd.s  f10, f7, f14  # f10 = bf3.re * w2->re + bf3.im * w2->im
	ssi     f5, a13, 4    # *(p1 + 1) = f5
	ssip    f4, a13, 8    # *p1 = f4, p1 += 2
	ssi     f9, a14, 4    # *(p2 + 1) = f9
	ssip    f8, a14, 8    # *p2 = f8, p2 += 2
	ssi     f11, a15, 4   # *(p3 + 1) = f11
	ssip    f10, a15, 8   # *p3 = f10, p3 += 2
.bf4_loop_end:

	addi.n  a8, a8, 2     # j += 2
	bgeu    a8, a5, .stage_next # if j >= m
	j       .group

.stage_next:
	slli    a5, a5, 2     # m <<= 2
	slli    a6, a6, 2     # w_step <<= 2
	addi.n  a7, a7, -1    # log4N--
	bnez    a7, .stage    # if log4N > 0

//	movi.n  a2, DSP_OK    # return(DSP_OK)
	retw



	.section .text # placed in IRAM instead of FLASH .text
	.global dl_ifft4r_fc32_ae32_
	.type   dl_ifft4r_fc32_ae32_,@function

// esp_err_t dl_ifft4r_fc32_ae32_(data, N, dl_fft4r_w_table_fc32, dl_fft4r_w_table_size)

//.ret_DSP_INVALID_LENGTH:
//	movi.n   a2, ESP_ERR_DSP_INVALID_LENGTH
//	retw.n

	.align  4
dl_ifft4r_fc32_ae32_:

	entry   a1, 16        # no auto vars on stack

//	bltui   a3, 4, .ret_DSP_INVALID_LENGTH # if N < 4 : return(ESP_ERR_DSP_INVALID_LENGTH)

//	addi.n  a6, a3, -1
//	and     a6, a3, a6
//	bnez    a6, .ret_DSP_INVALID_LENGTH # if N not power of 2 : return(ESP_ERR_DSP_INVALID_LENGTH)

	nsau    a6, a3        # inline dl_power_of_two(N)
	movi.n  a7, 31
	xor     a6, a6, a7

//	bbsi    a6, 0, .ret_DSP_INVALID_LENGTH # if N not power of 4 : return(ESP_ERR_DSP_INVALID_LENGTH)

	srli    a7, a6, 1     # log4N = dl_power_of_two(N) >> 1;

	addi.n  a6, a6, -1
	ssr 		a6
	srl     a6, a5        # w_step = table_size >> (dl_power_of_two(N) - 1)

	movi.n  a5, 2         # m = 2

.ifft_stage:
	srli    a3, a3, 2     # N >>= 2

	movi.n  a8, 0         # j = 0

.ifft_group:
	mov.n   a9, a4        # w0 = w
	mov.n   a10, a4       # w1 = w
	mov.n   a11, a4       # w2 = w

	mul16u  a12, a8, a3
	slli    a12, a12, 1   # start_index = (j * N) << 1

	addx8   a12, a12, a2  # p0 = data + (start_index << 1)
	addx8   a13, a3, a12  # p1 = p0 + (N << 1)
	addx8   a14, a3, a13  # p2 = p1 + (N << 1)
	addx8   a15, a3, a14  # p3 = p2 + (N << 1)

	loopnez a3, .inv_bf4_loop_end # for (uint k = 0; k < N; k++)
	lsi     f1, a12, 4    #  f1 = in0.im = *(p0 + 1)
	lsi     f3, a14, 4    #  f3 = in2.im = *(p2 + 1)
	lsi     f0, a12, 0    #  f0 = in0.re = *p0
	lsi     f2, a14, 0    #  f2 = in2.re = *p2
	add.s   f5, f1, f3    #  f5 = in0.im + in2.im
	sub.s   f7, f1, f3    #  f7 = in0.im - in2.im
	lsi     f1, a13, 4    #  f1 = in1.im = *(p1 + 1)
	lsi     f3, a15, 4    #  f3 = in3.im = *(p3 + 1)
	add.s   f4, f0, f2    #  f4 = in0.re + in2.re
	sub.s   f6, f0, f2    #  f6 = in0.re - in2.re
	add.s   f9, f1, f3    #  f9 = in1.im + in3.im
	sub.s   f11, f1, f3   # f11 = in1.im - in3.im
	lsi     f0, a13, 0    #  f0 = in1.re = *p1
	lsi     f2, a15, 0    #  f2 = in3.re = *p3
	lsi     f12, a9, 0    # f12 = w0->re
	lsi     f13, a10, 0   # f13 = w1->re
	lsi     f14, a11, 0   # f14 = w2->re
	add.s   f8, f0, f2    #  f8 = in1.re + in3.re
	sub.s   f10, f0, f2   # f10 = in1.re - in3.re
	sub.s   f1, f5, f9    #  f1 = bf2.im = in0.im + in2.im - in1.im - in3.im
	add.s   f5, f5, f9    #  f5 = bf0.im = in0.im + in2.im + in1.im + in3.im
	sub.s   f2, f6, f11   #  f2 = bf1.re = in0.re - in2.re + in1.im - in3.im
	add.s   f6, f6, f11   #  f6 = bf3.re = in0.re - in2.re - in1.im + in3.im
	sub.s   f0, f4, f8    #  f0 = bf2.re = in0.re + in2.re - in1.re - in3.re
	add.s   f4, f4, f8    #  f4 = bf0.re = in0.re + in2.re + in1.re + in3.re
	add.s   f3, f7, f10   #  f3 = bf1.im = in0.im - in2.im - in1.re + in3.re
	sub.s   f7, f7, f10   #  f7 = bf3.im = in0.im - in2.im + in1.re - in3.re
	ssi     f5, a12, 4    # *(p0 + 1) = f5 = bf0.im
	ssip    f4, a12, 8    # *p0 = f4 = bf0.re , p0 += 2
	mul.s   f5, f3, f12   #  f5 = bf1.im * w0->re
	mul.s   f4, f2, f12   #  f4 = bf1.re * w0->re
	mul.s   f9, f1, f13   #  f9 = bf2.im * w1->re
	mul.s   f8, f0, f13   #  f8 = bf2.re * w1->re
	mul.s   f11, f7, f14  # f11 = bf3.im * w2->re
	mul.s   f10, f6, f14  # f10 = bf3.re * w2->re
	lsi     f12, a9, 4    # f12 = w0->im
	lsi     f13, a10, 4   # f13 = w1->im
	lsi     f14, a11, 4   # f14 = w2->im
	addx4   a9, a6, a9    # w0 += m
	addx8   a10, a6, a10  # w1 += 2 * m
	addx4   a11, a6, a11
	addx8   a11, a6, a11  # w2 += 3 * m
	madd.s  f5, f2, f12   #  f5 = bf1.im * w0->re - bf1.re * w0->im
	msub.s  f4, f3, f12   #  f4 = bf1.re * w0->re + bf1.im * w0->im
	madd.s  f9, f0, f13   #  f9 = bf2.im * w1->re - bf2.re * w1->im
	msub.s  f8, f1, f13   #  f8 = bf2.re * w1->re + bf2.im * w1->im
	madd.s  f11, f6, f14  # f11 = bf3.im * w2->re - bf3.re * w2->im
	msub.s  f10, f7, f14  # f10 = bf3.re * w2->re + bf3.im * w2->im
	ssi     f5, a13, 4    # *(p1 + 1) = f5
	ssip    f4, a13, 8    # *p1 = f4, p1 += 2
	ssi     f9, a14, 4    # *(p2 + 1) = f9
	ssip    f8, a14, 8    # *p2 = f8, p2 += 2
	ssi     f11, a15, 4   # *(p3 + 1) = f11
	ssip    f10, a15, 8   # *p3 = f10, p3 += 2
.inv_bf4_loop_end:

	addi.n  a8, a8, 2     # j += 2
	bgeu    a8, a5, .ifft_stage_next # if j >= m
	j       .ifft_group

.ifft_stage_next:
	slli    a5, a5, 2     # m <<= 2
	slli    a6, a6, 2     # w_step <<= 2
	addi.n  a7, a7, -1    # log4N--
	bnez    a7, .ifft_stage    # if log4N > 0

//	movi.n  a2, DSP_OK    # return(DSP_OK)
	retw
