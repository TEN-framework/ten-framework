// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
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

#include "dsps_fft2r.h"
#include "dsp_common.h"
#include "dsp_types.h"
#include <math.h>
#include "esp_attr.h"
#include <malloc.h>
#include "dsp_tests.h"


int16_t *dsps_fft_w_table_sc16;
int dsps_fft_w_table_sc16_size;
uint8_t dsps_fft2r_sc16_initialized = 0;
uint8_t dsps_fft2r_sc16_mem_allocated = 0;

unsigned short reverse(unsigned short x, unsigned short N, int order);

static const int add_rount_mult = 0x7fff;
static const int mult_shift_const = 0x7fff; // Used to shift data << 15

static inline int16_t xtfixed_bf_1(int16_t a0, int16_t a1, int16_t a2, int16_t a3, int16_t a4, int result_shift)
{
    int result = a0 * mult_shift_const;
    result -= (int32_t)a1 * (int32_t)a2 + (int32_t)a3 * (int32_t)a4;
    result += add_rount_mult;
    result = result >> result_shift;
    return (int16_t)result;
}

static inline int16_t xtfixed_bf_2(int16_t a0, int16_t a1, int16_t a2, int16_t a3, int16_t a4, int result_shift)
{
    int result = a0 * mult_shift_const;
    result -= ((int32_t)a1 * (int32_t)a2 - (int32_t)a3 * (int32_t)a4);
    result += add_rount_mult;
    result = result >> result_shift;
    return (int16_t)result;
}

static inline int16_t xtfixed_bf_3(int16_t a0, int16_t a1, int16_t a2, int16_t a3, int16_t a4, int result_shift)
{
    int result = a0 * mult_shift_const;
    result += (int32_t)a1 * (int32_t)a2 + (int32_t)a3 * (int32_t)a4;
    result += add_rount_mult;
    result = result >> result_shift;
    return (int16_t)result;
}

static inline int16_t xtfixed_bf_4(int16_t a0, int16_t a1, int16_t a2, int16_t a3, int16_t a4, int result_shift)
{
    int result = a0 * mult_shift_const;
    result += (int32_t)a1 * (int32_t)a2 - (int32_t)a3 * (int32_t)a4;
    result += add_rount_mult;
    result = result >> result_shift;
    return (int16_t)result;
}

esp_err_t dsps_fft2r_init_sc16(int16_t *fft_table_buff, int table_size)
{
    esp_err_t result = ESP_OK;
    if (dsps_fft2r_sc16_initialized != 0) {
        return result;
    }
    if (table_size > CONFIG_DSP_MAX_FFT_SIZE) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (table_size == 0) {
        return result;
    }
    if (fft_table_buff != NULL) {
        if (dsps_fft2r_sc16_mem_allocated) {
            return ESP_ERR_DSP_REINITIALIZED;
        }
        dsps_fft_w_table_sc16 = fft_table_buff;
        dsps_fft_w_table_sc16_size = table_size;
    } else {
        if (!dsps_fft2r_sc16_mem_allocated) {
            dsps_fft_w_table_sc16 = (int16_t *)memalign(16, CONFIG_DSP_MAX_FFT_SIZE * sizeof(int16_t));
        }
        dsps_fft_w_table_sc16_size = CONFIG_DSP_MAX_FFT_SIZE;
        dsps_fft2r_sc16_mem_allocated = 1;
    }

    result = dsps_gen_w_r2_sc16(dsps_fft_w_table_sc16, dsps_fft_w_table_sc16_size);
    if (result != ESP_OK) {
        return result;
    }
    result = dsps_bit_rev_sc16_ansi(dsps_fft_w_table_sc16, dsps_fft_w_table_sc16_size >> 1);
    if (result != ESP_OK) {
        return result;
    }
    dsps_fft2r_sc16_initialized = 1;
    return ESP_OK;
}

void dsps_fft2r_deinit_sc16()
{
    if (dsps_fft2r_sc16_mem_allocated) {
        free(dsps_fft_w_table_sc16);
    }
    dsps_fft2r_sc16_mem_allocated = 0;
    dsps_fft2r_sc16_initialized = 0;
}

esp_err_t dsps_fft2r_sc16_ansi_(int16_t *data, int N, int16_t *sc_table)
{
    if (!dsp_is_power_of_two(N)) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }
    if (!dsps_fft2r_sc16_initialized) {
        return ESP_ERR_DSP_UNINITIALIZED;
    }

    esp_err_t result = ESP_OK;

    uint32_t *w = (uint32_t *)sc_table;
    uint32_t *in_data = (uint32_t *)data;

    int ie, ia, m;
    sc16_t cs;// c - re, s - im
    sc16_t m_data;
    sc16_t a_data;

    ie = 1;
    for (int N2 = N / 2; N2 > 0; N2 >>= 1) {
        ia = 0;
        for (int j = 0; j < ie; j++) {
            cs.data = w[j];
            //c = w[2 * j];
            //s = w[2 * j + 1];
            for (int i = 0; i < N2; i++) {
                m = ia + N2;
                m_data.data = in_data[m];
                a_data.data = in_data[ia];
                //data[2 * m] = data[2 * ia] - re_temp;
                //data[2 * m + 1] = data[2 * ia + 1] - im_temp;
                sc16_t m1;
                m1.re = xtfixed_bf_1(a_data.re, cs.re, m_data.re, cs.im, m_data.im, 16);//(a_data.re - temp.re + shift_const) >> 1;
                m1.im = xtfixed_bf_2(a_data.im, cs.re, m_data.im, cs.im, m_data.re, 16);//(a_data.im - temp.im + shift_const) >> 1;
                in_data[m] = m1.data;

                //data[2 * ia] = data[2 * ia] + re_temp;
                //data[2 * ia + 1] = data[2 * ia + 1] + im_temp;
                sc16_t m2;
                m2.re = xtfixed_bf_3(a_data.re, cs.re, m_data.re, cs.im, m_data.im, 16);//(a_data.re + temp.re + shift_const) >> 1;
                m2.im = xtfixed_bf_4(a_data.im, cs.re, m_data.im, cs.im, m_data.re, 16);//(a_data.im + temp.im + shift_const)>>1;
                in_data[ia] = m2.data;
                ia++;
            }
            ia += N2;
        }
        ie <<= 1;
    }
    return result;
}


static inline unsigned short reverse_sc16(unsigned short x, unsigned short N, int order)
{
    unsigned short b = x;

    b = (b & 0xff00) >> 8 | (b & 0x00fF) << 8;
    b = (b & 0xf0F0) >> 4 | (b & 0x0f0F) << 4;
    b = (b & 0xCCCC) >> 2 | (b & 0x3333) << 2;
    b = (b & 0xAAAA) >> 1 | (b & 0x5555) << 1;
    return b >> (16 - order);
}

esp_err_t dsps_bit_rev_sc16_ansi(int16_t *data, int N)
{
    if (!dsp_is_power_of_two(N)) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }
    esp_err_t result = ESP_OK;

    int j, k;
    uint32_t temp;
    uint32_t *in_data = (uint32_t *)data;
    j = 0;
    for (int i = 1; i < (N - 1); i++) {
        k = N >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
        if (i < j) {
            temp = in_data[j];
            in_data[j] = in_data[i];
            in_data[i] = temp;
        }
    }
    return result;
}

esp_err_t dsps_gen_w_r2_sc16(int16_t *w, int N)
{
    if (!dsp_is_power_of_two(N)) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }

    esp_err_t result = ESP_OK;

    int i;
    float e = M_PI * 2.0 / N;

    for (i = 0; i < (N >> 1); i++) {
        w[2 * i] = (int16_t)(INT16_MAX * cosf(i * e));
        w[2 * i + 1] = (int16_t)(INT16_MAX * sinf(i * e));
    }

    return result;
}

esp_err_t dsps_cplx2reC_sc16(int16_t *data, int N)
{
    if (!dsp_is_power_of_two(N)) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }
    esp_err_t result = ESP_OK;

    int i;
    int n2 = N << (1); // we will operate with int32 indexes
    uint32_t *in_data = (uint32_t *)data;

    sc16_t kl;
    sc16_t kh;
    sc16_t nl;
    sc16_t nh;

    for (i = 0; i < (N / 4); i++) {
        kl.data = in_data[i  + 1];
        nl.data = in_data[N - i - 1];
        kh.data = in_data[i + 1 + N / 2];
        nh.data = in_data[N - i - 1 - N / 2];

        data[i * 2 + 0 + 2] = kl.re + nl.re;
        data[i * 2 + 1 + 2] = kl.im - nl.im;

        data[n2 - i * 2 - 1 - N] = kh.re + nh.re;
        data[n2 - i * 2 - 2 - N] = kh.im - nh.im;

        data[i * 2 + 0 + 2 + N] = kl.im + nl.im;
        data[i * 2 + 1 + 2 + N] = kl.re - nl.re;

        data[n2 - i * 2 - 1] = kh.im + nh.im;
        data[n2 - i * 2 - 2] = kh.re - nh.re;
    }
    data[N] = data[1];
    data[1] = 0;
    data[N + 1] = 0;

    return result;
}

esp_err_t dsps_cplx2real_sc16_ansi(int16_t *data, int N)
{

    int order = dsp_power_of_two(N);
    sc16_t *table = (sc16_t *)dsps_fft_w_table_sc16;
    sc16_t *result = (sc16_t *)data;
    // Original formula...
    // result[0].re = result[0].re + result[0].im;
    // result[N].re = result[0].re - result[0].im;
    // result[0].im = 0;
    // result[N].im = 0;
    // Optimized one:
    int16_t tmp_re = result[0].re;
    result[0].re = (tmp_re + result[0].im) >> 1;
    result[0].im = (tmp_re - result[0].im) >> 1;

    sc16_t f1k, f2k;
    for (int k = 1; k <= N / 2 ; ++k ) {
        sc16_t fpk = result[k];
        sc16_t fpnk;
        fpnk.re = result[N - k].re;
        fpnk.im = result[N - k].im;
        f1k.re = fpk.re + fpnk.re;
        f1k.im = fpk.im - fpnk.im;
        f2k.re = fpk.re - fpnk.re;
        f2k.im = fpk.im + fpnk.im;

        int table_index = reverse(k, N, order);

        // float c = -dsps_fft_w_table_fc32[table_index*2+1];
        // float s = -dsps_fft_w_table_fc32[table_index*2+0];
        sc16_t w = table[table_index];

        sc16_t tw;
        {
            int re = (w.re * f2k.im - w.im * f2k.re) >> 15;
            int im = (+w.re * f2k.re + w.im * f2k.im) >> 15;
            tw.re = re;
            tw.im = im;
        }

        result[k].re = (f1k.re + tw.re) >> 2;
        result[k].im = (f1k.im - tw.im) >> 2;
        result[N - k].re = (f1k.re - tw.re) >> 2;
        result[N - k].im = -(f1k.im + tw.im) >> 2;
    }
    return ESP_OK;
}
