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
#include "dsps_fft4r.h"
#include "dsp_tests.h"


static const char *TAG = "dsps_fft4r_ansi";

TEST_CASE("dsps_fft4r_fc32_ansi functionality", "[dsps]")
{
    float *data =  (float *)memalign(16, sizeof(float) * 1024 * 2);
    TEST_ASSERT_NOT_NULL(data);

    float *check_data_fft = (float *)memalign(16, sizeof(float) * 1024 * 2);
    TEST_ASSERT_NOT_NULL(check_data_fft);

    esp_err_t ret;
    ret = dsps_fft2r_init_fc32(NULL, 1024);
    TEST_ESP_OK(ret);

    ret = dsps_fft4r_init_fc32(NULL, 1024);
    TEST_ESP_OK(ret);

    int N_check = 256;
    for (size_t pow = 2; pow < 6; pow++) {
        N_check = 1 << (pow * 2);
        for (size_t i = 0; i < N_check; i++) {
            data[i * 2] = cosf(2 * M_PI * 4 / 256 * i);
            data[i * 2 + 1] = sinf(2 * M_PI * 18 / 256 * i);

            check_data_fft[i * 2] = data[i * 2];
            check_data_fft[i * 2 + 1] = data[i * 2 + 1];
        }

        dsps_fft2r_fc32_ansi(data, N_check);
        dsps_bit_rev_fc32_ansi(data, N_check);

        dsps_fft4r_fc32_ansi(check_data_fft, N_check);
        dsps_bit_rev4r_fc32(check_data_fft, N_check);

        float diff = 0;
        for (size_t i = 0; i < N_check * 2; i++) {
            diff += fabs(data[i] - check_data_fft[i]);
        }
        diff = diff / N_check;
        ESP_LOGI(TAG, "diff[%i] = %f\n", N_check, diff);
        if (diff > 0.00001) {
            dsps_view(data, N_check * 2, 64, 16, -N_check, N_check, '.');
            dsps_view(check_data_fft, N_check * 2, 64, 16, -N_check, N_check, '.');
            TEST_ASSERT_MESSAGE (false, "Result out of range!\n");
        }
    }

    dsps_view(data, N_check * 2, 64, 16, -N_check, N_check, '.');
    dsps_view(check_data_fft, N_check * 2, 64, 16, -N_check, N_check, '.');

    dsps_fft2r_deinit_fc32();
    dsps_fft4r_deinit_fc32();
    free(data);
    free(check_data_fft);
}

TEST_CASE("dsps_fft4r_fc32_ansi benchmark", "[dsps]")
{
    float *check_data_fft = (float *)malloc(sizeof(float) * 4096 * 2);
    TEST_ASSERT_NOT_NULL(check_data_fft);

    unsigned int start_b;
    float cycles;

    esp_err_t ret = dsps_fft4r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    TEST_ESP_OK(ret);

    int N_check = 256;
    for (size_t pow = 2; pow < 7; pow++) {
        N_check = 1 << (pow * 2);
        for (size_t i = 0; i < N_check; i++) {
            check_data_fft[i * 2] = cosf(2 * M_PI * 4 / 256 * i);
            check_data_fft[i * 2 + 1] = sinf(2 * M_PI * 18 / 256 * i);
        }

        start_b = dsp_get_cpu_cycle_count();
        dsps_fft4r_fc32_ansi(check_data_fft, N_check);
        dsps_bit_rev4r_fc32(check_data_fft, N_check);
        cycles = dsp_get_cpu_cycle_count() - start_b;

        ESP_LOGI(TAG, "Benchmark dsps_fft4r_fc32_ansi - %6i cycles for %6i points FFT.", (int)cycles, N_check);
    }

    dsps_fft4r_deinit_fc32();
    free(check_data_fft);
}


TEST_CASE("dsps_cplx2real_fc32 functionality", "[dsps]")
{
    float *data =  (float *)malloc(sizeof(float) * 1024 * 2);
    TEST_ASSERT_NOT_NULL(data);

    float *check_data_fft = (float *)malloc(sizeof(float) * 1024 * 2);
    TEST_ASSERT_NOT_NULL(check_data_fft);

    esp_err_t ret = dsps_fft4r_init_fc32(NULL, 1024);
    TEST_ESP_OK(ret);

    ret = dsps_fft2r_init_fc32(NULL, 1024);
    TEST_ESP_OK(ret);

    int N_check = 256;
    for (size_t pow = 4; pow < 11; pow++) {
        N_check = 1 << (pow);
        for (size_t i = 0; i < N_check * 2; i++) {
            data[i] = 0;
            check_data_fft[i] = data[i];
        }
        data[1] = N_check;
        check_data_fft[1] = data[1];

        dsps_fft2r_fc32_ansi(data, N_check);
        dsps_bit_rev_fc32_ansi(data, N_check);
        dsps_cplx2real_fc32_ansi(data, N_check);

        dsps_fft2r_fc32_ansi(check_data_fft, N_check);
        dsps_bit_rev_fc32_ansi(check_data_fft, N_check);
        dsps_cplx2real_fc32(check_data_fft, N_check);

        float diff = 0;
        for (size_t i = 0; i < N_check * 2; i++) {
            diff += fabs(data[i] - check_data_fft[i]);
        }
        diff = diff / N_check;
        if (diff > 0.00001) {
            for (int i = 0; i < N_check * 2; i++) {
                ESP_LOGD(TAG, "data[%i]= %f,    %f = check_data_fft[%i], diff=%f\n", i, data[i], check_data_fft[i], i, data[i] - check_data_fft[i]);
            }

            dsps_view(data, N_check * 2, 128, 16, -N_check, N_check, '.');
            dsps_view(check_data_fft, N_check * 2, 128, 16, -N_check, N_check, '.');
            ESP_LOGE(TAG, "Error diff[%i] = %f\n", N_check, diff);
            TEST_ASSERT_MESSAGE (false, "Result out of range!\n");
        }

        ESP_LOGI(TAG, "diff[%i] = %f\n", N_check, diff);
    }

    free(data);
    free(check_data_fft);

    dsps_fft4r_deinit_fc32();
    dsps_fft2r_deinit_fc32();
}

static portMUX_TYPE testnlock = portMUX_INITIALIZER_UNLOCKED;

TEST_CASE("dsps_cplx2real_fc32_ansi benchmark", "[dsps]")
{
    float *check_data_fft = (float *)malloc(sizeof(float) * 4096 * 2);
    TEST_ASSERT_NOT_NULL(check_data_fft);

    unsigned int start_b;
    float cycles;

    esp_err_t ret = dsps_fft4r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    TEST_ESP_OK(ret);

    int N_check = 256;
    for (size_t pow = 4; pow < 13; pow++) {
        N_check = 1 << (pow);
        for (size_t i = 0; i < N_check; i++) {
            check_data_fft[i * 2] = cosf(2 * M_PI * 4 / 256 * i);
            check_data_fft[i * 2 + 1] = sinf(2 * M_PI * 18 / 256 * i);
        }

        portENTER_CRITICAL(&testnlock);
        start_b = dsp_get_cpu_cycle_count();
        dsps_cplx2real_fc32_ansi(check_data_fft, N_check);
        cycles = dsp_get_cpu_cycle_count() - start_b;
        portEXIT_CRITICAL(&testnlock);

        ESP_LOGI(TAG, "Benchmark dsps_cplx2real_fc32_ansi - %6i cycles for %6i points FFT.", (int)cycles, N_check);
    }

    dsps_fft4r_deinit_fc32();
    free(check_data_fft);
}

TEST_CASE("dsps_gen_bitrev4r_table bitrev table generation.", "[dsps]")
{
    for (int i = 2 ; i < 7 ; i++) {
        int N_check = 1 << (i * 2);
        dsps_gen_bitrev4r_table(N_check, 8, "fc32");
    }
}
