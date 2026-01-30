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
#include "dsps_fft4r.h"
#include "dsp_common.h"
#include "dsp_types.h"
#include <math.h>
#include "esp_attr.h"
#include "esp_log.h"
#include <string.h>
#include <malloc.h>

static const char *TAG = "fftr4 ansi";

float *dsps_fft4r_w_table_fc32;
int dsps_fft4r_w_table_size;
uint8_t dsps_fft4r_initialized = 0;
uint8_t dsps_fft4r_mem_allocated = 0;
//float* win2;
uint16_t *dsps_fft4r_ram_rev_table = NULL;

esp_err_t dsps_fft4r_init_fc32(float *fft_table_buff, int max_fft_size)
{
    esp_err_t result = ESP_OK;
    if (dsps_fft4r_initialized != 0) {
        return result;
    }
    if (max_fft_size > CONFIG_DSP_MAX_FFT_SIZE) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (max_fft_size == 0) {
        return result;
    }
    if (fft_table_buff != NULL) {
        if (dsps_fft4r_mem_allocated) {
            return ESP_ERR_DSP_REINITIALIZED;
        }
        dsps_fft4r_w_table_fc32 = fft_table_buff;
        dsps_fft4r_w_table_size = max_fft_size * 2;
    } else {
        if (!dsps_fft4r_mem_allocated) {
            dsps_fft4r_w_table_fc32 = (float *)malloc(max_fft_size * sizeof(float) * 4);
            if (NULL == dsps_fft4r_w_table_fc32) {
                return ESP_ERR_DSP_PARAM_OUTOFRANGE;
            }
        }
        dsps_fft4r_w_table_size = max_fft_size * 2;
        dsps_fft4r_mem_allocated = 1;
    }

    // FFT ram_rev table allocated
    int pow = dsp_power_of_two(max_fft_size) >> 1;
    if ((pow >= 2) && (pow <= 6)) {
        dsps_fft4r_ram_rev_table = (uint16_t *)malloc(2 * dsps_fft4r_rev_tables_fc32_size[pow - 2] * sizeof(uint16_t));
        if (NULL == dsps_fft4r_ram_rev_table) {
            return ESP_ERR_DSP_PARAM_OUTOFRANGE;
        }
        memcpy(dsps_fft4r_ram_rev_table, dsps_fft4r_rev_tables_fc32[pow - 2], 2 * dsps_fft4r_rev_tables_fc32_size[pow - 2] * sizeof(uint16_t));
        dsps_fft4r_rev_tables_fc32[pow - 2] = dsps_fft4r_ram_rev_table;
    }

    for (int i = 0; i < dsps_fft4r_w_table_size; i++) {
        float angle = 2 * M_PI * i / (float)dsps_fft4r_w_table_size;
        dsps_fft4r_w_table_fc32[2 * i + 0] = cosf(angle);
        dsps_fft4r_w_table_fc32[2 * i + 1] = sinf(angle);
    }

    dsps_fft4r_initialized = 1;

    return ESP_OK;
}

void dsps_fft4r_deinit_fc32()
{
    if (dsps_fft4r_mem_allocated) {
        free(dsps_fft4r_w_table_fc32);
    }
    if (dsps_fft4r_ram_rev_table != NULL) {
        free(dsps_fft4r_ram_rev_table);
        dsps_fft4r_ram_rev_table = NULL;
    }
    // Re init bitrev table for next use
    dsps_fft4r_rev_tables_init_fc32();

    dsps_fft4r_mem_allocated = 0;
    dsps_fft4r_initialized = 0;
}

esp_err_t dsps_bit_rev4r_direct_fc32_ansi(float *data, int N)
{
    if (!dsp_is_power_of_two(N)) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }
    if (0 == dsps_fft4r_initialized) {
        return ESP_ERR_DSP_UNINITIALIZED;
    }
    esp_err_t result = ESP_OK;
    int log2N = dsp_power_of_two(N);
    int log4N = log2N >> 1;
    if ((log2N & 0x01) != 0) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }
    float r_temp, i_temp;
    for (int i = 0; i < N; i++) {
        int cnt;
        int xx;
        int bits2;
        xx = 0;
        cnt = log4N;
        int j = i;
        while (cnt > 0) {
            bits2 = j & 0x3;
            xx = (xx << 2) + bits2;
            j = j >> 2;
            cnt--;
        }
        if (i < xx) {
            r_temp = data[i * 2 + 0];
            i_temp = data[i * 2 + 1];
            data[i * 2 + 0] = data[xx * 2 + 0];
            data[i * 2 + 1] = data[xx * 2 + 1];
            data[xx * 2 + 0] = r_temp;
            data[xx * 2 + 1] = i_temp;
        }
    }
    return result;
}

esp_err_t dsps_fft4r_fc32_ansi_(float *data, int length, float *table, int table_size)
{
    if (0 == dsps_fft4r_initialized) {
        return ESP_ERR_DSP_UNINITIALIZED;
    }

    fc32_t bfly[4];
    int log2N = dsp_power_of_two(length);
    int log4N = log2N >> 1;
    if ((log2N & 0x01) != 0) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }

    int m = 2;
    int wind_step = table_size / length;
    while (1) {  ///radix 4
        if (log4N == 0) {
            break;
        }
        length = length >> 2;
        for (int j = 0; j < m; j += 2) { // j: which FFT of this step
            int start_index = j * (length << 1); // n: n-point FFT

            fc32_t *ptrc0 = (fc32_t *)data + start_index;
            fc32_t *ptrc1 = ptrc0 + length;
            fc32_t *ptrc2 = ptrc1 + length;
            fc32_t *ptrc3 = ptrc2 + length;

            fc32_t *winc0 = (fc32_t *)table;
            fc32_t *winc1 = winc0;
            fc32_t *winc2 = winc0;

            for (int k = 0; k < length; k++) {
                fc32_t in0 = *ptrc0;
                fc32_t in2 = *ptrc2;
                fc32_t in1 = *ptrc1;
                fc32_t in3 = *ptrc3;

                bfly[0].re = in0.re + in2.re + in1.re + in3.re;
                bfly[0].im = in0.im + in2.im + in1.im + in3.im;

                bfly[1].re = in0.re - in2.re + in1.im - in3.im;
                bfly[1].im = in0.im - in2.im - in1.re + in3.re;

                bfly[2].re = in0.re + in2.re - in1.re - in3.re;
                bfly[2].im = in0.im + in2.im - in1.im - in3.im;

                bfly[3].re = in0.re - in2.re - in1.im + in3.im;
                bfly[3].im = in0.im - in2.im + in1.re - in3.re;



                *ptrc0 = bfly[0];
                ptrc1->re = bfly[1].re * winc0->re + bfly[1].im * winc0->im;
                ptrc1->im = bfly[1].im * winc0->re - bfly[1].re * winc0->im;
                ptrc2->re = bfly[2].re * winc1->re + bfly[2].im * winc1->im;
                ptrc2->im = bfly[2].im * winc1->re - bfly[2].re * winc1->im;
                ptrc3->re = bfly[3].re * winc2->re + bfly[3].im * winc2->im;
                ptrc3->im = bfly[3].im * winc2->re - bfly[3].re * winc2->im;

                winc0 += 1 * wind_step;
                winc1 += 2 * wind_step;
                winc2 += 3 * wind_step;

                ptrc0++;
                ptrc1++;
                ptrc2++;
                ptrc3++;
            }
        }
        m = m << 2;
        wind_step = wind_step << 2;
        log4N--;
    }
    return ESP_OK;
}

esp_err_t dsps_cplx2real_fc32_ansi_(float *data, int N, float *table, int table_size)
{
    if (0 == dsps_fft4r_initialized) {
        return ESP_ERR_DSP_UNINITIALIZED;
    }
    int wind_step = table_size / (N);
    fc32_t *result = (fc32_t *)data;
    // Original formula...
    // result[0].re = result[0].re + result[0].im;
    // result[N].re = result[0].re - result[0].im;
    // result[0].im = 0;
    // result[N].im = 0;
    // Optimized one:
    float tmp_re = result[0].re;
    result[0].re = tmp_re + result[0].im;
    result[0].im = tmp_re - result[0].im;

    fc32_t f1k, f2k;
    for (int k = 1; k <= N / 2 ; k++ ) {
        fc32_t fpk = result[k];
        fc32_t fpnk = result[N - k];
        f1k.re = fpk.re + fpnk.re;
        f1k.im = fpk.im - fpnk.im;
        f2k.re = fpk.re - fpnk.re;
        f2k.im = fpk.im + fpnk.im;

        float c = -table[k * wind_step + 1];
        float s = -table[k * wind_step + 0];
        fc32_t tw;
        tw.re = c * f2k.re - s * f2k.im;
        tw.im = s * f2k.re + c * f2k.im;

        result[k].re = 0.5 * (f1k.re + tw.re);
        result[k].im = 0.5 * (f1k.im + tw.im);
        result[N - k].re = 0.5 * (f1k.re - tw.re);
        result[N - k].im = 0.5 * (tw.im  - f1k.im);
    }
    return ESP_OK;
}

esp_err_t dsps_gen_bitrev4r_table(int N, int step, char *name_ext)
{
    if (!dsp_is_power_of_two(N)) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }

    int items_count = 0;
    ESP_LOGD(TAG, "const uint16_t bitrev4r_table_%i_%s[] = {        ", N, name_ext);
    int log2N = dsp_power_of_two(N);
    int log4N = log2N >> 1;

    for (int i = 1; i < N - 1; i++) {
        int cnt;
        int xx;
        int bits2;
        xx = 0;
        cnt = log4N;
        int j = i;
        while (cnt > 0) {
            bits2 = j & 0x3;
            xx = (xx << 2) + bits2;
            j = j >> 2;
            cnt--;
        }
        if (i < xx) {
            ESP_LOGD(TAG, "%i, %i, ", i * step, xx * step);
            items_count++;
            if ((items_count % 8) == 0) {
                ESP_LOGD(TAG, "        ");
            }
        }
    }

    ESP_LOGD(TAG, "};");
    ESP_LOGD(TAG, "const uint16_t bitrev4r_table_%i_%s_size = %i;\n", N, name_ext, items_count);

    ESP_LOGD(TAG, "extern const uint16_t bitrev4r_table_%i_%s[];", N, name_ext);
    ESP_LOGD(TAG, "extern const uint16_t bitrev4r_table_%i_%s_size;\n", N, name_ext);
    return ESP_OK;
}

esp_err_t dsps_bit_rev4r_fc32(float *data, int N)
{
    uint16_t *table;
    uint16_t  table_size;
    switch (N) {
    case 16:
        table = (uint16_t *)dsps_fft4r_rev_tables_fc32[0];
        table_size = dsps_fft4r_rev_tables_fc32_size[0];
        break;
    case 64:
        table = (uint16_t *)dsps_fft4r_rev_tables_fc32[1];
        table_size = dsps_fft4r_rev_tables_fc32_size[1];
        break;
    case 256:
        table = (uint16_t *)dsps_fft4r_rev_tables_fc32[2];
        table_size = dsps_fft4r_rev_tables_fc32_size[2];
        break;
    case 1024:
        table = (uint16_t *)dsps_fft4r_rev_tables_fc32[3];
        table_size = dsps_fft4r_rev_tables_fc32_size[3];
        break;
    case 4096:
        table = (uint16_t *)dsps_fft4r_rev_tables_fc32[4];
        table_size = dsps_fft4r_rev_tables_fc32_size[4];
        break;

    default:
        return dsps_bit_rev4r_direct_fc32_ansi(data, N);
        break;
    }

    return dsps_bit_rev_lookup_fc32(data, table_size, table);
}
