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

static const char *TAG = "fft2r_ae32";

__attribute__((aligned(16)))
static float data[1024 * 2];
__attribute__((aligned(16)))
static float check_data[1024 * 2];

TEST_CASE("dsps_fft2r_fc32 functionality", "[dsps]")
{

    float *data_test = (float *)memalign(1024, sizeof(float) * 1024 * 2);
    TEST_ASSERT_NOT_NULL(data_test);

    int N = sizeof(data) / sizeof(float) / 2;
    int check_bin = 32;
    float check_ampl = 2;
    for (int i = 0 ; i < N ; i++) {
        data[i * 2 + 0] = check_ampl * sinf(M_PI / N * check_bin * 2 * i) / (N / 2);
        data[i * 2 + 1] = 0;
    }
    for (int i = 0 ; i < N * 2 ; i++) {
        check_data[i] = data[i];
        data_test[i] = -1;
    }

    // Init FFT tables
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    TEST_ESP_OK(ret);

    int N_check = N;

    dsps_fft2r_fc32(data, N_check);
    dsps_fft2r_fc32_ansi(check_data, N_check);

    for (int i = 0 ; i < N_check ; i++) {
        if (fabs(check_data[i] - data[i]) < 1e-5) {
            ESP_LOGD(TAG, "Data[%i] =%8.4f, %8.4f, %8.4f", i, data[i], check_data[i], check_data[i] - data[i]);
        } else {
            ESP_LOGE(TAG, "Data[%i] =%f, %f, %f", i, data[i], check_data[i], check_data[i] - data[i]);
        }
    }

    dsps_bit_rev_fc32_ansi(data, N);

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
    dsps_fft2r_deinit_fc32();
    free(data_test);
}

TEST_CASE("dsps_fft2r_fc32 benchmark", "[dsps]")
{
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    TEST_ESP_OK(ret);
    asm("test_point1:   nop;");

    for (int i = 5 ; i < 10 ; i++) {
        int N_check = 2 << i;
        int check_bin = 32;
        for (int i = 0 ; i < N_check ; i++) {
            data[i * 2 + 0] = 4 * sinf(M_PI / N_check * check_bin * 2 * i) / (N_check / 2);
            data[i * 2 + 1] = 0;
        }

        unsigned int start_b = dsp_get_cpu_cycle_count();
        dsps_fft2r_fc32(data, N_check);

        unsigned int end_b = dsp_get_cpu_cycle_count();
        float total_b = end_b - start_b;
        float cycles = total_b;
        ESP_LOGI(TAG, "Benchmark dsps_fft2r_fc32 - %6i cycles for %6i points FFT.", (int)cycles, N_check);
        float min_exec = 3;
        float max_exec = 330000;
        TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);
    }
    dsps_fft2r_deinit_fc32();
}

TEST_CASE("dsps_bit_rev2r_fc32_ae32 benchmark", "[dsps]")
{
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    TEST_ESP_OK(ret);

    float *data = (float *)memalign(16, 2 * 4096 * sizeof(float));
    TEST_ASSERT_NOT_NULL(data);

    float *check_data = (float *)memalign(16, 2 * 4096 * sizeof(float));
    TEST_ASSERT_NOT_NULL(check_data);

    int N_check = 256;
    for (size_t i = 4; i < 13; i++) {
        N_check = 1 << i;
        for (size_t i = 0; i < N_check * 2; i++) {
            data[i] = i;
            check_data[i] = i;
        }
        dsps_bit_rev_fc32_ansi(data, N_check);
        unsigned int start_b = dsp_get_cpu_cycle_count();
        dsps_bit_rev2r_fc32(data, N_check);
        float cycles = dsp_get_cpu_cycle_count() - start_b;

        for (size_t i = 0; i < N_check * 2; i++) {
            TEST_ASSERT_EQUAL( data[i], check_data[i]);
        }
        ESP_LOGI(TAG, "Benchmark dsps_bit_rev2r_fc32_ae32 - %6i cycles for %i points.", (int)cycles, N_check);
    }

    dsps_fft2r_deinit_fc32();
    free(data);
    free(check_data);
}
