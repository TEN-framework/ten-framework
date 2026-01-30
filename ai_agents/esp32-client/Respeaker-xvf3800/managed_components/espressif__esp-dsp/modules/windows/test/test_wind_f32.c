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

#include "esp_dsp.h"
#include "dsps_wind.h"

static const int length = 1024;

// This test check if the window is symmetric
TEST_CASE("dsps_wind_hann_f32: test Hann window for symmetry", "[dsps]")
{
    float *data = (float *)malloc(length * sizeof(float));
    dsps_wind_hann_f32(data, length);
    float hann_diff = 0;
    for (int i = 0 ; i < length / 2 ; i++) {
        hann_diff += fabs(data[i] - data[length - 1 - i]);
    }

    if (hann_diff > 0) {
        TEST_ASSERT_EQUAL(0, hann_diff);
    }
    dsps_view(data, length, 64, 10, 0, 1, '.');
    free(data);
}

TEST_CASE("dsps_wind_blackman_f32: test Blackman window for symmetry", "[dsps]")
{
    float *data = (float *)malloc(length * sizeof(float));
    dsps_wind_blackman_f32(data, length);
    float hann_diff = 0;
    for (int i = 0 ; i < length / 2 ; i++) {
        hann_diff += fabs(data[i] - data[length - 1 - i]);
    }
    if (hann_diff > 0) {
        TEST_ASSERT_EQUAL(0, hann_diff);
    }
    dsps_view(data, length, 64, 10, 0, 1, '.');
    free(data);
}

TEST_CASE("dsps_wind_blackman_harris_f32: test Blackman-Hariss window for symmetry", "[dsps]")
{
    float *data = (float *)malloc(length * sizeof(float));
    dsps_wind_blackman_harris_f32(data, length);
    float hann_diff = 0;
    for (int i = 0 ; i < length / 2 ; i++) {
        hann_diff += fabs(data[i] - data[length - 1 - i]);
    }

    if (hann_diff > 0) {
        TEST_ASSERT_EQUAL(0, hann_diff);
    }
    dsps_view(data, length, 64, 10, 0, 1, '.');
    free(data);
}

TEST_CASE("dsps_wind_blackman_nuttall_f32: test Blackman-Nuttall window for symmetry", "[dsps]")
{
    float *data = (float *)malloc(length * sizeof(float));
    dsps_wind_blackman_nuttall_f32(data, length);
    float hann_diff = 0;
    for (int i = 0 ; i < length / 2 ; i++) {
        hann_diff += fabs(data[i] - data[length - 1 - i]);
    }

    if (hann_diff > 0) {
        TEST_ASSERT_EQUAL(0, hann_diff);
    }
    dsps_view(data, length, 64, 10, 0, 1, '.');
    free(data);
}

TEST_CASE("dsps_wind_nuttall_f32: test Nuttall window for symmetry", "[dsps]")
{
    float *data = (float *)malloc(length * sizeof(float));
    dsps_wind_nuttall_f32(data, length);
    float hann_diff = 0;
    for (int i = 0 ; i < length / 2 ; i++) {
        hann_diff += fabs(data[i] - data[length - 1 - i]);
    }

    if (hann_diff > 0) {
        TEST_ASSERT_EQUAL(0, hann_diff);
    }
    dsps_view(data, length, 64, 10, 0, 1, '.');
    free(data);
}

TEST_CASE("dsps_wind_flat_top_f32: test Flat-Top window for symmetry", "[dsps]")
{
    float *data = (float *)malloc(length * sizeof(float));
    dsps_wind_flat_top_f32(data, length);
    float hann_diff = 0;
    for (int i = 0 ; i < length / 2 ; i++) {
        hann_diff += fabs(data[i] - data[length - 1 - i]);
    }

    if (hann_diff > 0) {
        TEST_ASSERT_EQUAL(0, hann_diff);
    }
    dsps_view(data, length, 64, 10, 0, 1, '.');
    free(data);
}
