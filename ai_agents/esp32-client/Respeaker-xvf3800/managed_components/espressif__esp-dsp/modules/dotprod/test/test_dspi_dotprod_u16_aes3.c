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

#include "dspi_dotprod.h"
#include "dsp_tests.h"

static const char *TAG = "dspi_dotprod_u16";

TEST_CASE("dspi_dotprod_u16_aexx functionality", "[dspi]")
{
    int shift = 2;

    int max_N = 8192;
    uint16_t *x = (uint16_t *)memalign(16, max_N * sizeof(int16_t));
    uint16_t *y = (uint16_t *)memalign(16, max_N * sizeof(int16_t));
    uint16_t *z = (uint16_t *)memalign(16, max_N * sizeof(int16_t));

    printf("Data: x=%8.8"PRIx32", y=%8.8"PRIx32", z=%8.8"PRIx32" \n", (uint32_t)x, (uint32_t)y, (uint32_t)z);
    for (size_t i = 0; i < max_N; i++) {
        x[i] = i % 7;
        y[i] = i % 7;
        z[i] = 0;
    }
    {
        ESP_LOGI(TAG, "dspi_dotprod_u16 8x8");
        image2d_t image1 = {&x[3], 1, 1, 64, 64, 64, 64}; // Image 64
        image2d_t image2 = {y, 1, 1, 8, 8, 8, 8}; // Umage 64
        uint16_t result = -1;
        unsigned int start_b = dsp_get_cpu_cycle_count();
        dspi_dotprod_u16(&image1, &image2, &result, 8, 8, shift);
        unsigned int end_b = dsp_get_cpu_cycle_count();
        ESP_LOGI(TAG, "cycles = %i", (end_b - start_b));
        ESP_LOGI(TAG, "result 1 = %i", result);
        uint16_t result_ref = -1;
        dspi_dotprod_u16_ansi(&image1, &image2, &result_ref, 8, 8, shift);
        ESP_LOGI(TAG, "result ref = %i", result_ref);
        TEST_ASSERT_EQUAL( result, result_ref);
    }
    {
        ESP_LOGI(TAG, "dspi_dotprod_u16 16x16");
        image2d_t image1 = {&x[3], 1, 1, 64, 64, 64, 64}; // Image 64x64
        image2d_t image2 = {y, 1, 1, 16, 16, 16, 16}; // Umage 16x16
        uint16_t result = -1;
        unsigned int start_b = dsp_get_cpu_cycle_count();
        dspi_dotprod_u16(&image1, &image2, &result, 16, 16, shift);
        unsigned int end_b = dsp_get_cpu_cycle_count();
        ESP_LOGI(TAG, "cycles = %i", end_b - start_b);
        ESP_LOGI(TAG, "result 1 = %i", result);
        uint16_t result_ref = -1;
        dspi_dotprod_u16_ansi(&image1, &image2, &result_ref, 16, 16, shift);
        ESP_LOGI(TAG, "result ref = %i", result_ref);
        TEST_ASSERT_EQUAL( result, result_ref);
    }
    {
        ESP_LOGI(TAG, "dspi_dotprod_u16 24x24");
        image2d_t image1 = {&x[3], 1, 1, 64, 64, 64, 64}; // Image 64x64
        image2d_t image2 = {y, 1, 1, 24, 24, 24, 24}; // Umage 24x24
        uint16_t result = -1;
        unsigned int start_b = dsp_get_cpu_cycle_count();
        dspi_dotprod_u16(&image1, &image2, &result, 24, 24, shift);
        unsigned int end_b = dsp_get_cpu_cycle_count();
        ESP_LOGI(TAG, "cycles = %i", end_b - start_b);
        ESP_LOGI(TAG, "result 1 = %i", result);
        uint16_t result_ref = -1;
        dspi_dotprod_u16_ansi(&image1, &image2, &result_ref, 24, 24, shift);
        ESP_LOGI(TAG, "result ref = %i", result_ref);
        TEST_ASSERT_EQUAL( result, result_ref);
    }
    {
        ESP_LOGI(TAG, "dspi_dotprod_u16 32x32");
        image2d_t image1 = {&x[3], 1, 1, 64, 64, 64, 64}; // Image 64x64
        image2d_t image2 = {y, 1, 1, 32, 32, 32, 32}; // Umage 32x32
        uint16_t result = -1;
        unsigned int start_b = dsp_get_cpu_cycle_count();
        dspi_dotprod_u16(&image1, &image2, &result, 32, 32, shift);
        unsigned int end_b = dsp_get_cpu_cycle_count();
        ESP_LOGI(TAG, "cycles = %i", end_b - start_b);
        ESP_LOGI(TAG, "result 1 = %i", result);
        uint16_t result_ref = -1;
        dspi_dotprod_u16_ansi(&image1, &image2, &result_ref, 32, 32, shift);
        ESP_LOGI(TAG, "result ref = %i", result_ref);
        TEST_ASSERT_EQUAL( result, result_ref);
    }

    ESP_LOGI(TAG, "dspi_dotprod_u16 done");
    free(x);
    free(y);
    free(z);
}
