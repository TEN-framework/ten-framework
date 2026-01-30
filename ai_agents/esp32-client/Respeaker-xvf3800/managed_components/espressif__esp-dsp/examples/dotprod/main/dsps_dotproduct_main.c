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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"

#include "esp_dsp.h"

static const char *TAG = "main";

// This example shows how to use dsps_dotprod_f32 and dsps_dotprode_f32 functions

#define N_SAMPLES 256
int N = N_SAMPLES;
__attribute__((aligned(16)))
float input1[N_SAMPLES];
__attribute__((aligned(16)))
float input2[N_SAMPLES];

void app_main()
{
    esp_err_t ret;
    ESP_LOGI(TAG, "Start Example.");

    // The example will calculate n!
    //Initialize an input arrays
    for (int i = 0 ; i < N ; i++) {
        input1[i] = 1;
        input2[i] = i;
    }
    float result1 = 0;
    unsigned int start_b = dsp_get_cpu_cycle_count();
    ret = dsps_dotprod_f32(input1, input2, &result1, 101);
    unsigned int end_b = dsp_get_cpu_cycle_count();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Operation error = %i", ret);
    }
    ESP_LOGI(TAG, "The sum of 101 elements from 0..100 = %f", result1);
    ESP_LOGI(TAG, "Operation for 101 samples take %i cycles", end_b - start_b);

    ESP_LOGI(TAG, "End Example.");
}
