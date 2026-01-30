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
#include <math.h>

#include "esp_dsp.h"

static const char *TAG = "main";

// This example shows how to use FFT from esp-dsp library

#define N_SAMPLES 1024
int N = N_SAMPLES;
// Input test array
__attribute__((aligned(16)))
float x1[N_SAMPLES];
// Window coefficients
__attribute__((aligned(16)))
float wind[N_SAMPLES];
// working complex array
__attribute__((aligned(16)))
float y_cf[N_SAMPLES * 2];
// Pointers to result arrays
float *y1_cf = &y_cf[0];

static void process_and_show(float *data, int length)
{
    dsps_fft2r_fc32(data, length);
    // Bit reverse
    dsps_bit_rev_fc32(data, length);
    // Convert one complex vector to two complex vectors
    dsps_cplx2reC_fc32(data, length);

    for (int i = 0 ; i < length / 2 ; i++) {
        data[i] = 10 * log10f((data[i * 2 + 0] * data[i * 2 + 0] + data[i * 2 + 1] * data[i * 2 + 1]) / N);
    }

    // Show power spectrum in 64x10 window from -100 to 0 dB from 0..N/4 samples
    dsps_view(data, length / 2, 64, 10,  -120, 40, '|');
}

void app_main()
{
    esp_err_t ret;
    ESP_LOGI(TAG, "*** Start Example. ***");
    ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret  != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to initialize FFT. Error = %i", ret);
        return;
    }

    // Generate Hann window
    dsps_wind_hann_f32(wind, N);

    ESP_LOGI(TAG, "*** Multiply tone signal with Hann window by standard C loop. ***");
    // Generate input signal
    dsps_tone_gen_f32(x1, N, 1., 0.2, 0);
    // Convert two input vectors to one complex vector
    for (int i = 0 ; i < N ; i++) {
        y_cf[i * 2 + 0] = x1[i] * wind[i];
        y_cf[i * 2 + 1] = 0;
    }
    process_and_show(y_cf, N);

    ESP_LOGI(TAG, "*** Multiply tone signal with Hann window by esp-dsp basic math functions. ***");
    // Convert two input vectors to one complex vector with basic functions
    dsps_mul_f32(x1, wind, y_cf, N, 1, 1, 2); // Multiply input array with window and store as real part
    dsps_mulc_f32(&y_cf[1], &y_cf[1], N, 0, 2, 2); // Clear imaginary part of the complex signal
    process_and_show(y_cf, N);
    ESP_LOGI(TAG, "*** End Example. ***");
}
