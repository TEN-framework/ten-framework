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

#include <string.h>
#include "unity.h"
#include "esp_dsp.h"
#include "dsp_platform.h"
#include "esp_log.h"
#include <malloc.h>

#include "dsps_view.h"
#include "dsps_fft2r.h"
#include "dsp_tests.h"

static const char *TAG = "dsps_fft2r_ansi_s16";

TEST_CASE("dsps_fft2r_sc16_ansi functionality", "[dsps]")
{
    int N = 1024;
    int16_t *data = (int16_t *)memalign(N, sizeof(int16_t) * N * 2);
    TEST_ASSERT_NOT_NULL(data);

    float *result_data = (float *)memalign(N, sizeof(float) * N * 2);
    TEST_ASSERT_NOT_NULL(result_data);

    int check_bin = 64;
    for (int i = 0 ; i < N ; i++) {
        data[i * 2 + 0] = (INT16_MAX) * sin(M_PI / N * check_bin * 2 * i) * 0.5 * (1 - cosf(i * 2 * M_PI / (float)(N - 1)));
        data[i * 2 + 1] = 0;
    }
    int16_t *fft_table_buff = (int16_t *)malloc((N + 2) * sizeof(int16_t));
    TEST_ASSERT_NOT_NULL(fft_table_buff);

    fft_table_buff[0] = 1234;
    fft_table_buff[N + 1] = 5678;
    esp_err_t ret = dsps_fft2r_init_sc16(&fft_table_buff[1], N);
    TEST_ESP_OK(ret);

    dsps_fft2r_sc16_ansi(data, N);
    unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_bit_rev_sc16_ansi(data, N);
    unsigned int end_b = dsp_get_cpu_cycle_count();


    for (int i = 0 ; i < N ; i++) {
        ESP_LOGD(TAG, "Data[%i] %i\n", i / 2, data[i]);
    }

    float min = 10000;
    float max = -10000;
    int max_pos = 0;
    for (int i = 0 ; i < (N * 2) ; i++) {
        result_data[i] = data[i];
        result_data[i] = result_data[i] / INT16_MAX;
    }

    for (int i = 0 ; i < N ; i++) {
        result_data[i] = 10 * log10f(0.0000000000001 + result_data[i * 2 + 0] * result_data[i * 2 + 0] + result_data[i * 2 + 1] * result_data[i * 2 + 1]);
        if (result_data[i] < min) {
            min = result_data[i];
        }
        if (result_data[i] > max) {
            max = result_data[i];
            max_pos = i;
        }
        ESP_LOGD(TAG, "FFT Data[%i] =%8.4f dB", i, result_data[i]);
    }
    dsps_view_spectrum(result_data, N, -100, 0);
    float round_pow = round(max * 5);

    ESP_LOGI(TAG, "max_bin=%i, check_bin=%i, round_pow=%f\n", max_pos, check_bin, round_pow);

    if (max_pos < N / 2) {
        TEST_ASSERT_EQUAL( check_bin, max_pos);
    } else {
        TEST_ASSERT_EQUAL( N - check_bin, max_pos);
    }
    TEST_ASSERT_EQUAL( -12 * 5, round_pow);
    ESP_LOGI(TAG, "Calculation error is less then 0.2 dB");
    ESP_LOGI(TAG, "cycles - %i", end_b - start_b);
    // Check if we not out of range
    ESP_LOGI(TAG, "fft_table_buff[0] = %i, fft_table_buff[N+1] = %i", fft_table_buff[0], fft_table_buff[N + 1]);
    TEST_ASSERT_EQUAL( fft_table_buff[0],  1234);
    TEST_ASSERT_EQUAL( fft_table_buff[N + 1],  5678);
    free(fft_table_buff);
    dsps_fft2r_deinit_sc16();
    free(data);
    free(result_data);
}

TEST_CASE("dsps_fft2r_sc16_ansi benchmark", "[dsps]")
{
    int16_t *data = (int16_t *)memalign(1024, sizeof(int16_t) * 1024 * 2);
    TEST_ASSERT_NOT_NULL(data);

    esp_err_t ret = dsps_fft2r_init_sc16(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    TEST_ESP_OK(ret);

    for (int i = 5 ; i < 10 ; i++) {
        int N_check = 2 << i;
        unsigned int start_b = dsp_get_cpu_cycle_count();
        dsps_fft2r_sc16_ansi(data, N_check);

        unsigned int end_b = dsp_get_cpu_cycle_count();
        float total_b = end_b - start_b;
        float cycles = total_b;
        ESP_LOGI(TAG, "Benchmark dsps_fft2r_sc16_ansi - %6i cycles for %6i points FFT.", (int)cycles, N_check);
        float min_exec = 3;
        float max_exec = 330000 * 3;
        TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);
    }
    dsps_fft2r_deinit_sc16();
    free(data);
}
