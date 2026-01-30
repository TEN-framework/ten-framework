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

#include "dsps_view.h"


static const char *TAG = "dsps_view";

TEST_CASE("dsps_view functionality", "[dsps]")
{
    float *data = (float *)malloc(1024 * sizeof(float));
    for (int i = 0 ; i < 1024 ; i++) {
        data[i] = -100;
    }
    data[256] = 0;
    dsps_view_spectrum(data, 1024, -100, 0);

    ESP_LOGI(TAG, "Just a check\n");
    free(data);
}
