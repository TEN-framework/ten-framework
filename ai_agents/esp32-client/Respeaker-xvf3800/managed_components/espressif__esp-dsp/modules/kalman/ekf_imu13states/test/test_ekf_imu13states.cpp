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

#include "dsp_tests.h"
#include "ekf_imu13states.h"
#include "esp_attr.h"

static const char *TAG = "ekf_imu13states";


TEST_CASE("ekf_imu13states functionality gyro only", "[dspm]")
{
    ekf_imu13states *ekf13 = new  ekf_imu13states();
    ekf13->Init();
    ekf13->Test();
    unsigned int start_b = dsp_get_cpu_cycle_count();
    ekf13->TestFull(false);
    unsigned int end_b = dsp_get_cpu_cycle_count();
    ESP_LOGI(TAG, "Total time %i (K cycles)", (end_b - start_b) / 1000);
    TEST_ASSERT_LESS_THAN(100, (int)(1000 * abs(ekf13->X.data[4] - 0.1)));
    TEST_ASSERT_LESS_THAN(100, (int)(1000 * abs(ekf13->X.data[5] - 0.2)));
    TEST_ASSERT_LESS_THAN(100, (int)(1000 * abs(ekf13->X.data[6] - 0.3)));
    printf("Expected result = %i, calculated result = %i\n", 100, (int)(1000 * ekf13->X.data[4] + 0.5));
    printf("Expected result = %i, calculated result = %i\n", 200, (int)(1000 * ekf13->X.data[5] + 0.5));
    printf("Expected result = %i, calculated result = %i\n", 300, (int)(1000 * ekf13->X.data[6] + 0.5));

}

TEST_CASE("ekf_imu13states functionality gyro and magn", "[dspm]")
{
    ekf_imu13states *ekf13 = new  ekf_imu13states();
    ekf13->Init();
    ekf13->Test();
    unsigned int start_b = dsp_get_cpu_cycle_count();
    ekf13->TestFull(true);
    unsigned int end_b = dsp_get_cpu_cycle_count();
    ESP_LOGI(TAG, "Total time %i (K cycles)", (end_b - start_b) / 1000);

    TEST_ASSERT_LESS_THAN(600, (int)(1000 * abs(ekf13->X.data[4] - 0.1)));
    TEST_ASSERT_LESS_THAN(600, (int)(1000 * abs(ekf13->X.data[5] - 0.2)));
    TEST_ASSERT_LESS_THAN(600, (int)(1000 * abs(ekf13->X.data[6] - 0.3)));
    printf("Expected result = %i, calculated result = %i\n", 100, (int)(1000 * ekf13->X.data[4] + 0.5));
    printf("Expected result = %i, calculated result = %i\n", 200, (int)(1000 * ekf13->X.data[5] + 0.5));
    printf("Expected result = %i, calculated result = %i\n", 300, (int)(1000 * ekf13->X.data[6] + 0.5));
}
