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
#include "dsp_platform.h"
#include "esp_log.h"
#include <malloc.h>

#include "dsps_dotprod.h"
#include "dsp_tests.h"

// Test dsps_dotprod_s16_ansi function
TEST_CASE("dsps_dotprod_s16_ansi functionality", "[dsps]")
{
    int16_t check_value = 1235;
    int max_N = 1024;
    int16_t *x = (int16_t *)memalign(16, max_N * sizeof(int16_t));
    int16_t *y = (int16_t *)memalign(16, max_N * sizeof(int16_t));
    int16_t *z = (int16_t *)memalign(16, max_N * sizeof(int16_t));

    for (int i = 0 ; i < max_N ; i++) {
        x[i] = 0;
        y[i] = 1000;
    }

    z[0] = check_value;
    z[2] = check_value + 1;

    // Check result == 0
    for (int i = 4; i < 1024; i++) {
        esp_err_t status = dsps_dotprod_s16_ansi(x, y, &z[1], i, 0);
        TEST_ASSERT_EQUAL(status, ESP_OK);
        TEST_ASSERT_EQUAL(check_value, z[0]);
        TEST_ASSERT_EQUAL(check_value + 1, z[2]);
        TEST_ASSERT_EQUAL(0, z[1]);
    }

    int16_t val_x = 0x080;
    int16_t val_y = 0x100;
    int16_t val_shift = 0;

    for (int i = 0; i < max_N; i++) {
        x[i] = val_x;
        y[i] = val_y;
    }

    // We check that dotproduct working with shift = 0;
    for (int i = 4 ; i < 1024 ; i++) {
        esp_err_t status = dsps_dotprod_s16_ansi(x, y, &z[1], i, val_shift);

        TEST_ASSERT_EQUAL(status, ESP_OK);
        TEST_ASSERT_EQUAL(check_value, z[0]);
        TEST_ASSERT_EQUAL(check_value + 1, z[2]);
        TEST_ASSERT_EQUAL((i * (val_x * val_y) + (0x7fff >> val_shift)) >> (15 - val_shift), z[1]);
    }
    val_shift = 2;
    for (int i = 4 ; i < 1024 ; i++) {
        esp_err_t status = dsps_dotprod_s16_ansi(x, y, &z[1], i, val_shift);

        TEST_ASSERT_EQUAL(status, ESP_OK);
        TEST_ASSERT_EQUAL(check_value, z[0]);
        TEST_ASSERT_EQUAL(check_value + 1, z[2]);
        TEST_ASSERT_EQUAL(((long long)i * ((long long)val_x * (long long)val_y) + ((long long)0x7fff >> val_shift)) >> (15 - val_shift), z[1]);
    }

    free(x);
    free(y);
    free(z);
}

// Test dsps_dotprod_s16_ansi function
TEST_CASE("dsps_dotprod_s16_aexx functionality", "[dsps]")
{
    int16_t check_value = 1235;
    int max_N = 1024;
    int16_t *x = (int16_t *)memalign(16, max_N * sizeof(int16_t));
    int16_t *y = (int16_t *)memalign(16, max_N * sizeof(int16_t));
    int16_t *z = (int16_t *)memalign(16, max_N * sizeof(int16_t));

    for (int i = 0 ; i < max_N ; i++) {
        x[i] = 0;
        y[i] = 1000;
    }

    z[0] = check_value;
    z[2] = check_value + 1;

    // Check result == 0
    for (int i = 4 ; i < 1024 ; i++) {
        esp_err_t status = dsps_dotprod_s16(x, y, &z[1], i, 0);
        {
            TEST_ASSERT_EQUAL(status, ESP_OK);
            TEST_ASSERT_EQUAL(check_value, z[0]);
            TEST_ASSERT_EQUAL(check_value + 1, z[2]);
            TEST_ASSERT_EQUAL(0, z[1]);
        }
    }

    int16_t val_x = 0x080;
    int16_t val_y = 0x100;
    int16_t val_shift = 0;

    for (int i = 0 ; i < max_N ; i++) {
        x[i] = val_x;
        y[i] = val_y;
    }
    // We check that dotproduct working with shift = 0;
    for (int i = 4 ; i < 1024 ; i++) {
        esp_err_t status = dsps_dotprod_s16(x, y, &z[1], i, val_shift);
        {
            TEST_ASSERT_EQUAL(status, ESP_OK);
            TEST_ASSERT_EQUAL(check_value, z[0]);
            TEST_ASSERT_EQUAL(check_value + 1, z[2]);
            TEST_ASSERT_EQUAL((i * (val_x * val_y) + (0x7fff >> val_shift)) >> (15 - val_shift), z[1]);
        }
    }
    val_shift = 2;
    for (int i = 4 ; i < 1024 ; i++) {
        esp_err_t status = dsps_dotprod_s16(x, y, &z[1], i, val_shift);
        {
            TEST_ASSERT_EQUAL(status, ESP_OK);
            TEST_ASSERT_EQUAL(check_value, z[0]);
            TEST_ASSERT_EQUAL(check_value + 1, z[2]);
            TEST_ASSERT_EQUAL((i * (val_x * val_y) + ((int)0x7fff >> val_shift)) >> (15 - val_shift), z[1]);
        }
    }

    free(x);
    free(y);
    free(z);
}

static portMUX_TYPE testnlock = portMUX_INITIALIZER_UNLOCKED;
TEST_CASE("dsps_dotprod_s16 benchmark", "[dsps]")
{
    int max_N = 1024;

    int16_t *x = (int16_t *)memalign(16, max_N * sizeof(int16_t));
    int16_t *y = (int16_t *)memalign(16, max_N * sizeof(int16_t));
    int16_t *z = (int16_t *)memalign(16, max_N * sizeof(int16_t));

    for (int i = 0 ; i < max_N ; i++) {
        x[i] = 0x100;
        y[i] = 0x200;
    }
    // Disable interrupt to get exect count

    portENTER_CRITICAL(&testnlock);

    unsigned int start_b = dsp_get_cpu_cycle_count();
    int repeat_count = 1024;
    for (int i = 0 ; i < repeat_count ; i++) {
        dsps_dotprod_s16(x, y, &z[1], 1024, 0);
    }
    unsigned int end_b = dsp_get_cpu_cycle_count();
    portEXIT_CRITICAL(&testnlock);

    float total_b = end_b - start_b;
    float cycles = total_b / (repeat_count);
    printf("Benchmark dsps_dotprod_s16 - %f cycles for 1024 samples + overhead. Result = %08x\n", cycles, z[1]);
    float min_exec = 256;
    float max_exec = 8 * 1024;
    TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);

    free(x);
    free(y);
    free(z);
}

TEST_CASE("dsps_dotprod_s16_ansi benchmark", "[dsps]")
{
    int max_N = 1024;

    int16_t *x = (int16_t *)memalign(16, max_N * sizeof(int16_t));
    int16_t *y = (int16_t *)memalign(16, max_N * sizeof(int16_t));
    int16_t *z = (int16_t *)memalign(16, max_N * sizeof(int16_t));

    for (int i = 0 ; i < max_N ; i++) {
        x[i] = 0x100;
        y[i] = 0x200;
    }
    // Disable interrupt to get exect count

    portENTER_CRITICAL(&testnlock);

    unsigned int start_b = dsp_get_cpu_cycle_count();
    int repeat_count = 1024;
    for (int i = 0 ; i < repeat_count ; i++) {
        dsps_dotprod_s16_ansi(x, y, &z[1], 1024, 0);
    }
    unsigned int end_b = dsp_get_cpu_cycle_count();
    portEXIT_CRITICAL(&testnlock);

    float total_b = end_b - start_b;
    float cycles = total_b / (repeat_count);
    printf("Benchmark dsps_dotprod_s16 - %f cycles for 1024 samples + overhead. Result = %08x\n", cycles, z[1]);
    float min_exec = 1024 * 10;
    float max_exec = 1024 * 30;
    TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);

    free(x);
    free(y);
    free(z);
}
