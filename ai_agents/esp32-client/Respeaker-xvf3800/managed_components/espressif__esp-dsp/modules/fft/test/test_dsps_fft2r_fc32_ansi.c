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

static const char *TAG = "dsps_fft2r_ansi";

TEST_CASE("dsps_fft2r_fc32_ansi functionality", "[dsps]")
{
    float *data = (float *)malloc(2 * 4096 * sizeof(float));
    float *check_data = (float *)malloc(2 * 4096 * sizeof(float));

    int N = 1024;
    int check_bin = 32;
    for (int i = 0 ; i < N ; i++) {
        data[i * 2 + 0] = 2 * sinf(M_PI / N * check_bin * 2 * i) / (N / 2);
        data[i * 2 + 1] = 0;
    }

    float *fft_table_buff = (float *)malloc((N + 2) * sizeof(float));
    fft_table_buff[0] = 1234;
    fft_table_buff[N + 1] = 5678;

    esp_err_t ret = dsps_fft2r_init_fc32(&fft_table_buff[1], N);
    TEST_ESP_OK(ret);


    dsps_fft2r_fc32_ansi(data, N);
    unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_bit_rev_fc32_ansi(data, N);
    unsigned int end_b = dsp_get_cpu_cycle_count();

    float min = 10000;
    float max = -10000;
    int max_pos = 0;
    for (int i = 0 ; i < N ; i++) {
        data[i] = 10 * log10f(data[i * 2 + 0] * data[i * 2 + 0] + data[i * 2 + 1] * data[i * 2 + 1]);
        if (data[i] < min) {
            min = data[i];
        }
        if (data[i] > max) {
            max = data[i];
            max_pos = i;
        }
        ESP_LOGD(TAG, "FFT Data[%i] =%8.4f dB", i, data[i]);
    }
    dsps_view_spectrum(data, 256, -160, 40);

    TEST_ASSERT_EQUAL( check_bin, max_pos);
    float round_pow = round(max * 10);
    TEST_ASSERT_EQUAL( 6 * 10, round_pow);
    ESP_LOGI(TAG, "Calculation error is less then 0.1 dB");
    ESP_LOGI(TAG, "cycles - %i", end_b - start_b);

    ESP_LOGI(TAG, "fft_table_buff[0] = %f, fft_table_buff[N+1] = %f", fft_table_buff[0], fft_table_buff[N + 1]);
    TEST_ASSERT_EQUAL( fft_table_buff[0],  1234);
    TEST_ASSERT_EQUAL( fft_table_buff[N + 1],  5678);
    free(fft_table_buff);

    free(data);
    free(check_data);
    dsps_fft2r_deinit_fc32();
}

TEST_CASE("dsps_fft2r_fc32_ansi benchmark", "[dsps]")
{
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    TEST_ESP_OK(ret);

    float *data = (float *)malloc(2 * 4096 * sizeof(float));
    TEST_ASSERT_NOT_NULL(data);

    float *check_data = (float *)malloc(2 * 4096 * sizeof(float));
    TEST_ASSERT_NOT_NULL(check_data);

    for (int i = 5 ; i < 10 ; i++) {
        int N_check = 2 << i;
        unsigned int start_b = dsp_get_cpu_cycle_count();
        dsps_fft2r_fc32_ansi(data, N_check);

        unsigned int end_b = dsp_get_cpu_cycle_count();
        float total_b = end_b - start_b;
        float cycles = total_b;
        ESP_LOGI(TAG, "Benchmark dsps_fft2r_fc32_ansi - %6i cycles for %6i points FFT.", (int)cycles, N_check);
        float min_exec = 3;
        float max_exec = 330000 * 3;
        TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);
    }
    free(data);
    free(check_data);
    dsps_fft2r_deinit_fc32();
}

TEST_CASE("dsps_gen_bitrev2r_table bitrev table generation.", "[dsps]")
{
    for (int i = 4 ; i < 13 ; i++) {
        int N_check = 1 << i;
        dsps_gen_bitrev2r_table(N_check, 8, "fc32");
    }
}
