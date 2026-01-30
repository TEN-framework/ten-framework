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

#include "dl_fft_base.h"

// unsigned short reverse(unsigned short x, unsigned short N, int order);

esp_err_t dl_fft2r_fc32_ansi(float *data, int N, float *w)
{
    esp_err_t result = ESP_OK;

    int ie, ia, m;
    float re_temp, im_temp;
    float c, s;
    ie = 1;
    for (int N2 = N / 2; N2 > 0; N2 >>= 1) {
        ia = 0;
        for (int j = 0; j < ie; j++) {
            c = w[2 * j];
            s = w[2 * j + 1];
            for (int i = 0; i < N2; i++) {
                m = ia + N2;
                re_temp = c * data[2 * m] + s * data[2 * m + 1];
                im_temp = c * data[2 * m + 1] - s * data[2 * m];
                data[2 * m] = data[2 * ia] - re_temp;
                data[2 * m + 1] = data[2 * ia + 1] - im_temp;
                data[2 * ia] = data[2 * ia] + re_temp;
                data[2 * ia + 1] = data[2 * ia + 1] + im_temp;
                ia++;
            }
            ia += N2;
        }
        ie <<= 1;
    }
    return result;
}

esp_err_t dl_ifft2r_fc32_ansi(float *data, int N, float *w)
{
    esp_err_t result = ESP_OK;

    int ie, ia, m;
    float re_temp, im_temp;
    float c, s;
    ie = 1;
    for (int N2 = N / 2; N2 > 0; N2 >>= 1) {
        ia = 0;
        for (int j = 0; j < ie; j++) {
            c = w[2 * j];
            s = -w[2 * j + 1];
            for (int i = 0; i < N2; i++) {
                m = ia + N2;
                re_temp = c * data[2 * m] + s * data[2 * m + 1];
                im_temp = c * data[2 * m + 1] - s * data[2 * m];
                data[2 * m] = data[2 * ia] - re_temp;
                data[2 * m + 1] = data[2 * ia + 1] - im_temp;
                data[2 * ia] = data[2 * ia] + re_temp;
                data[2 * ia + 1] = data[2 * ia + 1] + im_temp;
                ia++;
            }
            ia += N2;
        }
        ie <<= 1;
    }
    return result;
}

esp_err_t dl_bitrev2r_fc32_ansi(float *data, int N, uint16_t *bitrev_table, int bitrev_size)
{
    esp_err_t result = ESP_OK;

    if (bitrev_table) {
        float r_temp, i_temp;
        for (int n = 0; n < bitrev_size; n++) {
            uint16_t i = bitrev_table[n * 2];
            uint16_t j = bitrev_table[n * 2 + 1];
            r_temp = data[j];
            data[j] = data[i];
            data[i] = r_temp;
            i_temp = data[j + 1];
            data[j + 1] = data[i + 1];
            data[i + 1] = i_temp;
        }
    } else {
        int j, k;
        float r_temp, i_temp;
        j = 0;
        for (int i = 1; i < (N - 1); i++) {
            k = N >> 1;
            while (k <= j) {
                j -= k;
                k >>= 1;
            }
            j += k;
            if (i < j) {
                r_temp = data[j * 2];
                data[j * 2] = data[i * 2];
                data[i * 2] = r_temp;
                i_temp = data[j * 2 + 1];
                data[j * 2 + 1] = data[i * 2 + 1];
                data[i * 2 + 1] = i_temp;
            }
        }
    }

    return result;
}

esp_err_t dl_rfft_post_proc_fc32_ansi(float *data, int N, float *table)
{
    dl_fc32_t *result = (dl_fc32_t *)data;
    // Original formula...
    // result[0].re = result[0].re + result[0].im;
    // result[N].re = result[0].re - result[0].im;
    // result[0].im = 0;
    // result[N].im = 0;
    // Optimized one:
    float tmp_re = result[0].re;
    result[0].re = tmp_re + result[0].im;
    result[0].im = tmp_re - result[0].im;

    dl_fc32_t f1k, f2k;
    for (int k = 1; k <= N / 2; k++) {
        dl_fc32_t fpk = result[k];
        dl_fc32_t fpnk = result[N - k];
        f1k.re = fpk.re + fpnk.re;
        f1k.im = fpk.im - fpnk.im;
        f2k.re = fpk.re - fpnk.re;
        f2k.im = fpk.im + fpnk.im;

        float c = -table[k * 2 - 1];
        float s = -table[k * 2 - 2];
        dl_fc32_t tw;
        tw.re = c * f2k.re - s * f2k.im;
        tw.im = s * f2k.re + c * f2k.im;

        result[k].re = 0.5 * (f1k.re + tw.re);
        result[k].im = 0.5 * (f1k.im + tw.im);
        result[N - k].re = 0.5 * (f1k.re - tw.re);
        result[N - k].im = 0.5 * (tw.im - f1k.im);
    }
    return ESP_OK;
}

esp_err_t dl_rfft_pre_proc_fc32_ansi(float *data, int N, float *table)
{
    dl_fc32_t *result = (dl_fc32_t *)data;
    float tmp_re = result[0].re;
    result[0].re = (tmp_re + result[0].im) * 0.5;
    result[0].im = (tmp_re - result[0].im) * 0.5;

    dl_fc32_t f1k, f2k;
    for (int k = 1; k <= N / 2; k++) {
        dl_fc32_t fpk = result[k];
        dl_fc32_t fpnk = result[N - k];
        f1k.re = fpk.re + fpnk.re;
        f1k.im = fpk.im - fpnk.im;
        f2k.re = fpk.re - fpnk.re;
        f2k.im = fpk.im + fpnk.im;

        float c = -table[k * 2 - 1];
        float s = table[k * 2 - 2];
        dl_fc32_t tw;
        tw.re = c * f2k.re - s * f2k.im;
        tw.im = s * f2k.re + c * f2k.im;

        result[k].re = 0.5 * (f1k.re + tw.re);
        result[k].im = 0.5 * (f1k.im + tw.im);
        result[N - k].re = 0.5 * (f1k.re - tw.re);
        result[N - k].im = 0.5 * (tw.im - f1k.im);
    }
    return ESP_OK;
}

float *dl_gen_rfft_table_f32(int fft_point, uint32_t caps)
{
    float *fft_table = (float *)heap_caps_aligned_alloc(16, fft_point * sizeof(float), caps);

    if (fft_table) {
        for (int i = 1; i <= fft_point >> 1; i++) {
            float angle = 2 * M_PI * i * 1.0 / fft_point;
            fft_table[2 * i - 2] = cosf(angle);
            fft_table[2 * i - 1] = sinf(angle);
        }
    }

    return fft_table;
}

uint16_t *dl_gen_bitrev2r_table(int N, uint32_t caps, int *bitrev_size)
{
    int count = 0, idx = 0;
    int j = 0, k;
    for (int i = 1; i < (N - 1); i++) {
        k = N >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
        if (i < j) {
            count++;
        }
    }
    if (count * 2 > UINT16_MAX) {
        return NULL;
    }
    bitrev_size[0] = count;
    uint16_t *bitrev_table = (uint16_t *)heap_caps_malloc(2 * count * sizeof(uint16_t), caps);

    if (bitrev_table) {
        j = 0;
        for (int i = 1; i < (N - 1); i++) {
            k = N >> 1;
            while (k <= j) {
                j -= k;
                k >>= 1;
            }
            j += k;
            if (i < j) {
                bitrev_table[idx * 2] = j * 2;
                bitrev_table[idx * 2 + 1] = i * 2;
                idx++;
            }
        }
    }

    return bitrev_table;
}

float *dl_gen_fftr2_table_f32(int fft_point, uint32_t caps)
{
    float *fft_table = (float *)heap_caps_aligned_alloc(16, fft_point * sizeof(float), caps);

    if (fft_table) {
        float e = M_PI * 2.0 / fft_point;

        for (int i = 0; i < (fft_point >> 1); i++) {
            fft_table[2 * i] = cosf(i * e);
            fft_table[2 * i + 1] = sinf(i * e);
        }

        dl_bitrev2r_fc32_ansi(fft_table, fft_point >> 1, NULL, 0);
    }

    return fft_table;
}
