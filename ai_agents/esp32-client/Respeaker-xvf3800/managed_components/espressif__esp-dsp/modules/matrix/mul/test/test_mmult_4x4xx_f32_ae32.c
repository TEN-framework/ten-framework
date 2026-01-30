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

#include "dspm_mult.h"
#include "esp_attr.h"
#include "dsp_tests.h"

static const char *TAG = "dspm_mult_4x4x1_f32_ae32";

TEST_CASE("dspm_mult_4x4x1_f32_ae32 functionality", "[dspm]")
{
    int m = 4;
    int n = 4;
    int k = 1;

    float A[m][n];
    float *A_ptr = (float *)A;

    float B[n][k];
    float *B_ptr = (float *)B;

    float C[m][k];
    float *C_ptr = (float *)C;
    float C_compare[m][k];
    float *Cc_ptr = (float *)C_compare;

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            A[i][j] = i;
        }
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < k; j++) {
            B[i][j] = i;
        }
    }

    dspm_mult_4x4x1_f32(A_ptr, B_ptr, C_ptr);
    dspm_mult_f32_ansi(A_ptr, B_ptr, Cc_ptr, m, n, k);

    for (int i = 0 ; i < m ; i++) {
        for (int j = 0 ; j < k ; j++) {
            ESP_LOGD(TAG, "[%i][%i] calc=%f, expected =%f", i, j, C[i][j], C_compare[i][j]);
        }
    }
    //Compare and check results
    for (int i = 0; i < m * k; i++) {
        if (Cc_ptr[i] != C_ptr[i]) {
            TEST_ASSERT_EQUAL(C_ptr[i], Cc_ptr[i]);
        }
    }
}

TEST_CASE("dspm_mult_4x4x4_f32_ae32 functionality", "[dspm]")
{
    int m = 4;
    int n = 4;
    int k = 4;


    float A[m][n];
    float *A_ptr = (float *)A;

    float B[n][k];
    float *B_ptr = (float *)B;

    float C[m][k];
    float *C_ptr = (float *)C;
    float C_compare[m][k];
    float *Cc_ptr = (float *)C_compare;

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            A[i][j] = i;
            C[i][j] = 0;
        }
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < k; j++) {
            B[i][j] = i;
        }
    }

    dspm_mult_4x4x4_f32(A_ptr, B_ptr, C_ptr);
    dspm_mult_f32_ansi(A_ptr, B_ptr, Cc_ptr, m, n, k);

    for (int i = 0 ; i < m ; i++) {
        for (int j = 0 ; j < k ; j++) {
            ESP_LOGD(TAG, "[%i][%i] calc=%f, expected =%f", i, j, C[i][j], C_compare[i][j]);
        }
    }
    // Compare and check results
    for (int i = 0 ; i < m * k ; i++) {
        if (Cc_ptr[i] != C_ptr[i]) {
            TEST_ASSERT_EQUAL( C_ptr[i], Cc_ptr[i]);
        }
    }
}

static portMUX_TYPE testnlock = portMUX_INITIALIZER_UNLOCKED;

TEST_CASE("dspm_mult_4x4x1_f32_ae32 benchmark", "[dspm]")
{
    int m = 4;
    int n = 4;
    int k = 1;

    float A[m][n];
    float *A_ptr = (float *)A;

    float B[n][k];
    float *B_ptr = (float *)B;

    float C[m][k];
    float *C_ptr = (float *)C;


    portENTER_CRITICAL(&testnlock);

    unsigned int start_b = dsp_get_cpu_cycle_count();
    int repeat_count = 1024;
    for (int i = 0 ; i < repeat_count ; i++) {
        dspm_mult_4x4x1_f32(A_ptr, B_ptr, C_ptr);
    }
    unsigned int end_b = dsp_get_cpu_cycle_count();
    portEXIT_CRITICAL(&testnlock);

    float total_b = end_b - start_b;
    float cycles = total_b / (repeat_count);
    ESP_LOGI("dspm_mult_4x4x1_f32_ae32", "dspm_mult_4x4x1_f32_ae32 - %f per multiplication", cycles);
    float min_exec = 60;
    float max_exec = 300;
    TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);
}

TEST_CASE("dspm_mult_4x4x4_f32_ae32 benchmark", "[dspm]")
{
    int m = 4;
    int n = 4;
    int k = 4;

    float A[m][n];
    float *A_ptr = (float *)A;

    float B[n][k];
    float *B_ptr = (float *)B;

    float C[m][k];
    float *C_ptr = (float *)C;

    ESP_LOGI(TAG, "A: %8.8"PRIx32", B: %8.8"PRIx32", C=%8.8"PRIx32"", (uint32_t)A_ptr, (uint32_t)B_ptr, (uint32_t)C_ptr);

    portENTER_CRITICAL(&testnlock);

    unsigned int start_b = dsp_get_cpu_cycle_count();
    int repeat_count = 1024;
    for (int i = 0 ; i < repeat_count ; i++) {
        dspm_mult_4x4x4_f32(A_ptr, B_ptr, C_ptr);
    }
    unsigned int end_b = dsp_get_cpu_cycle_count();
    portEXIT_CRITICAL(&testnlock);

    float total_b = end_b - start_b;
    float cycles = total_b / (repeat_count);
    ESP_LOGI("dspm_mult_4x4x4_f32_ae32", "dspm_mult_4x4x4_f32_ae32 - %f per multiplication", cycles);
    float min_exec = 50;
    float max_exec = 750;
    TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);
}
