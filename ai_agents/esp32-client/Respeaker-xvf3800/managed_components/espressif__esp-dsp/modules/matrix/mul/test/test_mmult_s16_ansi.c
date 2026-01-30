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

static const char *TAG = "dspm_mult_s16_ansi";

// Test dsps_dotprod_s16_ansi function
TEST_CASE("dspm_mult_s16_ansi functionality", "[dspm]")
{
    int m = 4;
    int n = 3;
    int k = 4;


    int16_t A[m][n];
    int16_t *A_ptr = (int16_t *)A;

    int16_t B[n][k];
    int16_t *B_ptr = (int16_t *)B;

    int16_t C[m][k];
    int16_t *C_ptr = (int16_t *)C;
    int16_t C_compare[m][k];
    int16_t *Cc_ptr = (int16_t *)C_compare;

    int shift = 0;
    for (int i = 0 ; i < m * n; i++) {
        A_ptr[i] = 0x1000;
        B_ptr[i] = 0x200;
    }
    long long store_reg = 0;
    for (int i = 0 ; i < m ; i++) {
        for (int j = 0 ; j < k ; j++) {
            store_reg = (0x7fff >> shift);
            for (int s = 0 ; s < n ; s++) {
                store_reg += ((int32_t)A[i][s] * (int32_t)B[s][j]);
            }
            C_compare[i][j] = store_reg >> (15 - shift);
        }
    }
    dspm_mult_s16_ansi(A_ptr, B_ptr, C_ptr, m, n, k, shift);

    for (int i = 0 ; i < m ; i++) {
        for (int j = 0 ; j < k ; j++) {
            ESP_LOGD(TAG, "[%i][%i] calc=%i, expected =%i", i, j, C[i][j], C_compare[i][j]);
        }
    }
    // Compare and check results
    for (int i = 0 ; i < m * k ; i++) {
        if (Cc_ptr[i] != C_ptr[i]) {
            TEST_ASSERT_EQUAL(Cc_ptr[i], C_ptr[i]);
        }
    }
}

static portMUX_TYPE testnlock = portMUX_INITIALIZER_UNLOCKED;

TEST_CASE("dspm_mult_s16_ansi benchmark", "[dspm]")
{
    int m = 4;
    int n = 4;
    int k = 4;

    int16_t A[m][n];
    int16_t *A_ptr = (int16_t *)A;

    int16_t B[n][k];
    int16_t *B_ptr = (int16_t *)B;

    int16_t C[m][k];
    int16_t *C_ptr = (int16_t *)C;


    portENTER_CRITICAL(&testnlock);

    unsigned int start_b = dsp_get_cpu_cycle_count();
    int repeat_count = 1024;
    for (int i = 0 ; i < repeat_count ; i++) {
        dspm_mult_s16_ansi(A_ptr, B_ptr, C_ptr, m, n, k, 0);
    }
    unsigned int end_b = dsp_get_cpu_cycle_count();
    portEXIT_CRITICAL(&testnlock);

    float total_b = end_b - start_b;
    float cycles = total_b / (repeat_count);
    ESP_LOGI("dspm_mult_s16_ansi", "Benchmark dspm_mult_s16_ansi - %f per multiplication %ix%ix%i.\n", cycles, m, n, k);
    float min_exec = 1000;
    float max_exec = 3000;
    TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);
}
