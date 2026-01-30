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
#include "esp_log.h"
#include <string.h>
#include <malloc.h>


static const char *TAG = "fftr2_ansi";

float *dsps_fft_w_table_fc32;
int dsps_fft_w_table_size;
uint8_t dsps_fft2r_initialized = 0;
uint8_t dsps_fft2r_mem_allocated = 0;

uint16_t *dsps_fft2r_ram_rev_table = NULL;

#ifdef CONFIG_IDF_TARGET_ESP32S3
extern float *dsps_fft2r_w_table_fc32_1024;
#endif // CONFIG_IDF_TARGET_ESP32S3

unsigned short reverse(unsigned short x, unsigned short N, int order);

esp_err_t dsps_fft2r_init_fc32(float *fft_table_buff, int table_size)
{
    esp_err_t result = ESP_OK;
    if (dsps_fft2r_initialized != 0) {
        return result;
    }
    if (table_size > CONFIG_DSP_MAX_FFT_SIZE) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (table_size == 0) {
        return result;
    }
    if (fft_table_buff != NULL) {
        if (dsps_fft2r_mem_allocated) {
            return ESP_ERR_DSP_REINITIALIZED;
        }
        dsps_fft_w_table_fc32 = fft_table_buff;
        dsps_fft_w_table_size = table_size;
    } else {
        if (!dsps_fft2r_mem_allocated) {
#if CONFIG_IDF_TARGET_ESP32S3
            if (table_size <= 1024) {
                dsps_fft_w_table_fc32 = dsps_fft2r_w_table_fc32_1024;
            } else {
                dsps_fft_w_table_fc32 = (float *)memalign(16, sizeof(float) * table_size);
            }
#else
            dsps_fft_w_table_fc32 = (float *)malloc(table_size * sizeof(float));
#endif
            if (dsps_fft_w_table_fc32 == NULL) {
                return ESP_ERR_DSP_PARAM_OUTOFRANGE;
            }
        }
        dsps_fft_w_table_size = table_size;
        dsps_fft2r_mem_allocated = 1;

    }

    // FFT ram_rev table allocated
    int pow = dsp_power_of_two(table_size);
    if ((pow > 3) && (pow < 13)) {
        dsps_fft2r_ram_rev_table = (uint16_t *)malloc(2 * dsps_fft2r_rev_tables_fc32_size[pow - 4] * sizeof(uint16_t));
        if (dsps_fft2r_ram_rev_table == NULL) {
            return ESP_ERR_DSP_PARAM_OUTOFRANGE;
        }
        memcpy(dsps_fft2r_ram_rev_table, dsps_fft2r_rev_tables_fc32[pow - 4], 2 * dsps_fft2r_rev_tables_fc32_size[pow - 4] * sizeof(uint16_t));
        dsps_fft2r_rev_tables_fc32[pow - 4] = dsps_fft2r_ram_rev_table;
    }

    result = dsps_gen_w_r2_fc32(dsps_fft_w_table_fc32, dsps_fft_w_table_size);
    if (result != ESP_OK) {
        return result;
    }
    result = dsps_bit_rev_fc32_ansi(dsps_fft_w_table_fc32, dsps_fft_w_table_size >> 1);
    if (result != ESP_OK) {
        return result;
    }
    dsps_fft2r_initialized = 1;

    return ESP_OK;
}

void dsps_fft2r_deinit_fc32()
{
    if (dsps_fft2r_mem_allocated) {
#if CONFIG_IDF_TARGET_ESP32S3
        if (dsps_fft_w_table_fc32 != dsps_fft2r_w_table_fc32_1024) {
            free(dsps_fft_w_table_fc32);
        }
#else
        free(dsps_fft_w_table_fc32);
#endif
    }
    if (dsps_fft2r_ram_rev_table != NULL) {
        free(dsps_fft2r_ram_rev_table);
        dsps_fft2r_ram_rev_table = NULL;
    }
    // Re init bitrev table for next use
    dsps_fft2r_rev_tables_init_fc32();
    dsps_fft2r_mem_allocated = 0;
    dsps_fft2r_initialized = 0;
}

esp_err_t dsps_fft2r_fc32_ansi_(float *data, int N, float *w)
{
    if (!dsp_is_power_of_two(N)) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }
    if (!dsps_fft2r_initialized) {
        return ESP_ERR_DSP_UNINITIALIZED;
    }

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


unsigned short reverse(unsigned short x, unsigned short N, int order)
{
    unsigned short b = x;

    b = (b & 0xff00) >> 8 | (b & 0x00fF) << 8;
    b = (b & 0xf0F0) >> 4 | (b & 0x0f0F) << 4;
    b = (b & 0xCCCC) >> 2 | (b & 0x3333) << 2;
    b = (b & 0xAAAA) >> 1 | (b & 0x5555) << 1;
    return b >> (16 - order);
}

esp_err_t dsps_bit_rev_fc32_ansi(float *data, int N)
{
    if (!dsp_is_power_of_two(N)) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }

    esp_err_t result = ESP_OK;

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
    return result;
}

esp_err_t dsps_gen_w_r2_fc32(float *w, int N)
{
    if (!dsp_is_power_of_two(N)) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }

    esp_err_t result = ESP_OK;

    int i;
    float e = M_PI * 2.0 / N;

    for (i = 0; i < (N >> 1); i++) {
        w[2 * i] = cosf(i * e);
        w[2 * i + 1] = sinf(i * e);
    }

    return result;
}

esp_err_t dsps_cplx2reC_fc32_ansi(float *data, int N)
{
    if (!dsp_is_power_of_two(N)) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }
    esp_err_t result = ESP_OK;

    int i;
    int n2 = N << 1;

    float rkl = 0;
    float rkh = 0;
    float rnl = 0;
    float rnh = 0;
    float ikl = 0;
    float ikh = 0;
    float inl = 0;
    float inh = 0;

    for (i = 0; i < (N / 4); i++) {
        rkl = data[i * 2 + 0 + 2];
        ikl = data[i * 2 + 1 + 2];
        rnl = data[n2 - i * 2 - 2];
        inl = data[n2 - i * 2 - 1];

        rkh = data[i * 2 + 0 + 2 + N];
        ikh = data[i * 2 + 1 + 2 + N];
        rnh = data[n2 - i * 2 - 2 - N];
        inh = data[n2 - i * 2 - 1 - N];

        data[i * 2 + 0 + 2] = rkl + rnl;
        data[i * 2 + 1 + 2] = ikl - inl;

        data[n2 - i * 2 - 1 - N] = inh - ikh;
        data[n2 - i * 2 - 2 - N] = rkh + rnh;

        data[i * 2 + 0 + 2 + N] = ikl + inl;
        data[i * 2 + 1 + 2 + N] = rnl - rkl;

        data[n2 - i * 2 - 1] = rkh - rnh;
        data[n2 - i * 2 - 2] = ikh + inh;
    }
    data[N] = data[1];
    data[1] = 0;
    data[N + 1] = 0;

    return result;
}

esp_err_t dsps_gen_bitrev2r_table(int N, int step, char *name_ext)
{
    if (!dsp_is_power_of_two(N)) {
        return ESP_ERR_DSP_INVALID_LENGTH;
    }

    int j, k;
    j = 0;
    int items_count = 0;
    ESP_LOGD(TAG, "const uint16_t bitrev2r_table_%i_%s[] = {        ", N, name_ext);
    for (int i = 1; i < (N - 1); i++) {
        k = N >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
        if (i < j) {
            ESP_LOGD(TAG, "%i, %i, ", i * step, j * step);
            items_count++;
            if ((items_count % 8) == 0) {
                ESP_LOGD(TAG, " ");
            }
        }
    }
    ESP_LOGD(TAG, "};");
    ESP_LOGD(TAG, "const uint16_t bitrev2r_table_%i_%s_size = %i;\n", N, name_ext, items_count);

    ESP_LOGD(TAG, "extern const uint16_t bitrev2r_table_%i_%s[];", N, name_ext);
    ESP_LOGD(TAG, "extern const uint16_t bitrev2r_table_%i_%s_size;\n", N, name_ext);
    return ESP_OK;
}

esp_err_t dsps_bit_rev2r_fc32(float *data, int N)
{
    uint16_t *table;
    uint16_t  table_size;
    switch (N) {
    case 16:
        table = (uint16_t *)dsps_fft2r_rev_tables_fc32[0];
        table_size = dsps_fft2r_rev_tables_fc32_size[0];
        break;
    case 32:
        table = (uint16_t *)dsps_fft2r_rev_tables_fc32[1];
        table_size = dsps_fft2r_rev_tables_fc32_size[1];
        break;
    case 64:
        table = (uint16_t *)dsps_fft2r_rev_tables_fc32[2];
        table_size = dsps_fft2r_rev_tables_fc32_size[2];
        break;
    case 128:
        table = (uint16_t *)dsps_fft2r_rev_tables_fc32[3];
        table_size = dsps_fft2r_rev_tables_fc32_size[3];
        break;
    case 256:
        table = (uint16_t *)dsps_fft2r_rev_tables_fc32[4];
        table_size = dsps_fft2r_rev_tables_fc32_size[4];
        break;
    case 512:
        table = (uint16_t *)dsps_fft2r_rev_tables_fc32[5];
        table_size = dsps_fft2r_rev_tables_fc32_size[5];
        break;
    case 1024:
        table = (uint16_t *)dsps_fft2r_rev_tables_fc32[6];
        table_size = dsps_fft2r_rev_tables_fc32_size[6];
        break;
    case 2048:
        table = (uint16_t *)dsps_fft2r_rev_tables_fc32[7];
        table_size = dsps_fft2r_rev_tables_fc32_size[7];
        break;
    case 4096:
        table = (uint16_t *)dsps_fft2r_rev_tables_fc32[8];
        table_size = dsps_fft2r_rev_tables_fc32_size[8];
        break;

    default:
        return dsps_bit_rev_fc32(data, N);
        break;
    }
    return dsps_bit_rev_lookup_fc32(data, table_size, table);
}

esp_err_t dsps_bit_rev_lookup_fc32_ansi(float *data, int reverse_size, uint16_t *reverse_tab)
{
    float r_temp, i_temp;
    for (int n = 0 ; n < reverse_size ; n++) {
        uint16_t i = reverse_tab[n * 2 + 0] >> 2;
        uint16_t j = reverse_tab[n * 2 + 1] >> 2;
        r_temp = data[j];
        data[j] = data[i];
        data[i] = r_temp;
        i_temp = data[j + 1];
        data[j + 1] = data[i + 1];
        data[i + 1] = i_temp;
    }
    return ESP_OK;
}
