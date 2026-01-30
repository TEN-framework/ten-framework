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
#include "esp_log.h"

// Test dsps_dotprod_s16_ansi function
TEST_CASE("dspm_mult_s16_aexx functionality", "[dspm]")
{
    for (int m = 1 ; m < 8 ; m++) {
        for (int n = 1 ; n < 16 ; n++) {
            for (int k = 1 ; k < 16 ; k++) {

                int16_t A[m][n];
                int16_t *A_ptr = (int16_t *)A;

                int16_t B[n][k];
                int16_t *B_ptr = (int16_t *)B;

                int16_t C[m][k];
                int16_t *C_ptr = (int16_t *)C;
                int16_t C_compare[m][k];
                int16_t *Cc_ptr = (int16_t *)C_compare;
                for (int shift = -4 ; shift < 4 ; shift++) {
                    for (int i = 0 ; i < m ; i++) {
                        for (int j = 0 ; j < n; j++) {
                            A[i][j] = 0x123;
                        }
                    }
                    for (int i = 0 ; i < n ; i++) {
                        for (int j = 0 ; j < k; j++) {
                            B[i][j] = 0x123;
                        }
                    }

                    dspm_mult_s16_ansi(A_ptr, B_ptr, Cc_ptr, m, n, k, shift);
                    dspm_mult_s16(A_ptr, B_ptr, C_ptr,  m, n, k, shift);

                    // Compare and check results
                    for (int i = 0 ; i < m * k ; i++) {
                        if (Cc_ptr[i] != C_ptr[i]) {
                            ESP_LOGE("dspm_mult_s16_aexx", "Process path m=%i, n=%i, k=%i,  shift=%i", m, n, k, shift);
                            ESP_LOGE("dspm_mult_s16_aexx", "data[%i] %4.4x != %4.4x expected \n", i, C_ptr[i], Cc_ptr[i]);
                            TEST_ASSERT_EQUAL(Cc_ptr[i], C_ptr[i]);
                        }
                    }
                }
            }
        }
    }
}

static portMUX_TYPE testnlock = portMUX_INITIALIZER_UNLOCKED;

TEST_CASE("dspm_mult_s16_aexx benchmark", "[dspm]")
{
    unsigned int start_b = dsp_get_cpu_cycle_count();
    unsigned int end_b = dsp_get_cpu_cycle_count();
    for (int m = 2 ; m <= 8 ; m++) {
        for (int n = 2 ; n <= 16 ; n++) {
            for (int k = 1 ; k <= 16 ; k++) {

                int16_t A[m][n];
                int16_t *A_ptr = (int16_t *)A;

                int16_t B[m][n];
                int16_t *B_ptr = (int16_t *)B;

                int16_t C[m][k];
                int16_t *C_ptr = (int16_t *)C;

                memset(A, 0, sizeof(A));
                memset(B, 0, sizeof(A));
                memset(C, 0, sizeof(A));
                portENTER_CRITICAL(&testnlock);

                start_b = dsp_get_cpu_cycle_count();
                dspm_mult_s16(A_ptr, B_ptr, C_ptr, m, n, k, 0);
                end_b = dsp_get_cpu_cycle_count();
                portEXIT_CRITICAL(&testnlock);

                float total_b = end_b - start_b;
                float cycles = total_b;
                ESP_LOGD("dspm_mult_s16_aexx", "dspm_mult_s16_aexx[%i][%i][%i] - %f", m, n, k, cycles);
            }
        }
    }
}
