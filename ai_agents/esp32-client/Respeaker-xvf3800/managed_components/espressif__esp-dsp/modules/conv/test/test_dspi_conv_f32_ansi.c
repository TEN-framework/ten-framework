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
#include <math.h>
#include "unity.h"
#include "dsp_platform.h"
#include "esp_log.h"

#include "esp_attr.h"
#include "esp_dsp.h"
#include <malloc.h>
#include "dsp_tests.h"

static const char *TAG = "dspi_conv";

TEST_CASE("dspi_conv_f32_ansi functionality", "[dspi]")
{
    int max_N = 8192;

    float *data1 = (float *)memalign(16, max_N * sizeof(float));
    float *data2 = (float *)memalign(16, max_N * sizeof(float));
    float *data3 = (float *)memalign(16, max_N * sizeof(float));

    image2d_t image1 = {data1, 1, 1, 8, 8, 8, 8}; // Image 8x8
    image2d_t image2 = {data2, 1, 1, 4, 4, 4, 4}; // Image 4x4
    image2d_t image3 = {data3, 1, 1, 10, 10, 0, 0}; // Image 8x8

    for (int i = 0 ; i < max_N ; i++) {
        data1[i] = 0;
        data2[i] = 0;
        data3[i] = 0;
    }

    for (int y = 0 ; y < image1.stride_y / image1.step_y ; y++) {
        for (int x = 0 ; x < image1.stride_x / image1.step_x ; x++) {
            data1[y * image1.stride_x * image1.step_y + x * image1.step_x] = 1;
        }
    }
    for (int y = 0 ; y < image2.stride_y / image2.step_y ; y++) {
        for (int x = 0 ; x < image2.stride_x / image2.step_x ; x++) {
            data2[y * image2.stride_x * image2.step_y + x * image2.step_x] = 1;
        }
    }

    dspi_conv_f32_ansi(&image1, &image2, &image3);
    // x , y
    TEST_ASSERT_EQUAL(data3[0 * image3.stride_x * image3.step_y + 0 * image3.step_x], 9);
    TEST_ASSERT_EQUAL(data3[0 * image3.stride_x * image3.step_y + 6 * image3.step_x], 9);
    TEST_ASSERT_EQUAL(data3[6 * image3.stride_x * image3.step_y + 6 * image3.step_x], 9);
    TEST_ASSERT_EQUAL(data3[0 * image3.stride_x * image3.step_y + 6 * image3.step_x], 9);

    TEST_ASSERT_EQUAL(data3[7 * image3.stride_x * image3.step_y + 0 * image3.step_x], 6);
    TEST_ASSERT_EQUAL(data3[7 * image3.stride_x * image3.step_y + 6 * image3.step_x], 6);
    TEST_ASSERT_EQUAL(data3[0 * image3.stride_x * image3.step_y + 7 * image3.step_x], 6);
    TEST_ASSERT_EQUAL(data3[7 * image3.stride_x * image3.step_y + 7 * image3.step_x], 4);

    TEST_ASSERT_EQUAL(data3[1 * image3.stride_x * image3.step_y + 1 * image3.step_x], 16);
    TEST_ASSERT_EQUAL(data3[5 * image3.stride_x * image3.step_y + 1 * image3.step_x], 16);
    TEST_ASSERT_EQUAL(data3[1 * image3.stride_x * image3.step_y + 5 * image3.step_x], 16);
    TEST_ASSERT_EQUAL(data3[5 * image3.stride_x * image3.step_y + 5 * image3.step_x], 16);
    TEST_ASSERT_EQUAL(data3[3 * image3.stride_x * image3.step_y + 3 * image3.step_x], 16);

    free(data1);
    free(data2);
    free(data3);
}

TEST_CASE("dspi_conv_f32_ansi benchmark", "[dspi]")
{
    int max_N = 8192;

    float *data1 = (float *)memalign(16, max_N * sizeof(float));
    float *data2 = (float *)memalign(16, max_N * sizeof(float));
    float *data3 = (float *)memalign(16, max_N * sizeof(float));

    image2d_t image1 = {data1, 1, 1, 8, 8, 8, 8}; // Image 8x8
    image2d_t image2 = {data2, 1, 1, 4, 4, 4, 4}; // Image 4x4
    image2d_t image3 = {data3, 1, 1, 10, 10, 0, 0}; // Image 8x8

    for (int i = 0 ; i < max_N ; i++) {
        data1[i] = 0;
        data2[i] = 0;
        data3[i] = 0;
    }

    for (int y = 0 ; y < image1.stride_y / image1.step_y ; y++) {
        for (int x = 0 ; x < image1.stride_x / image1.step_x ; x++) {
            data1[y * image1.stride_x * image1.step_y + x * image1.step_x] = 1;
        }
    }
    for (int y = 0 ; y < image2.stride_y / image2.step_y ; y++) {
        for (int x = 0 ; x < image2.stride_x / image2.step_x ; x++) {
            data2[y * image2.stride_x * image2.step_y + x * image2.step_x] = 1;
        }
    }

    unsigned int start_b = dsp_get_cpu_cycle_count();
    dspi_conv_f32_ansi(&image1, &image2, &image3);
    unsigned int end_b = dsp_get_cpu_cycle_count();
    float cycles = end_b - start_b;
    ESP_LOGI(TAG, "dspi_conv_f32_ansi - %f cycles", cycles);

    free(data1);
    free(data2);
    free(data3);
}
