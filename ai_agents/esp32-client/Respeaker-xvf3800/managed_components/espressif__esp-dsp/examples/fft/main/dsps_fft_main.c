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
__attribute__((aligned(16)))
float x2[N_SAMPLES];
// Window coefficients
__attribute__((aligned(16)))
float wind[N_SAMPLES];
// working complex array
__attribute__((aligned(16)))
float y_cf[N_SAMPLES * 2];
// Pointers to result arrays
float *y1_cf = &y_cf[0];
float *y2_cf = &y_cf[N_SAMPLES];

// Sum of y1 and y2
__attribute__((aligned(16)))
float sum_y[N_SAMPLES / 2];

void app_main()
{
    esp_err_t ret;
    ESP_LOGI(TAG, "Start Example.");
    ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret  != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to initialize FFT. Error = %i", ret);
        return;
    }

    // Generate hann window
    dsps_wind_hann_f32(wind, N);
    // Generate input signal for x1 A=1 , F=0.1
    dsps_tone_gen_f32(x1, N, 1.0, 0.16,  0);
    // Generate input signal for x2 A=0.1,F=0.2
    dsps_tone_gen_f32(x2, N, 0.1, 0.2, 0);

    // Convert two input vectors to one complex vector
    for (int i = 0 ; i < N ; i++) {
        y_cf[i * 2 + 0] = x1[i] * wind[i];
        y_cf[i * 2 + 1] = x2[i] * wind[i];
    }
    // FFT
    unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_fft2r_fc32(y_cf, N);
    unsigned int end_b = dsp_get_cpu_cycle_count();
    // Bit reverse
    dsps_bit_rev_fc32(y_cf, N);
    // Convert one complex vector to two complex vectors
    dsps_cplx2reC_fc32(y_cf, N);

    for (int i = 0 ; i < N / 2 ; i++) {
        y1_cf[i] = 10 * log10f((y1_cf[i * 2 + 0] * y1_cf[i * 2 + 0] + y1_cf[i * 2 + 1] * y1_cf[i * 2 + 1]) / N);
        y2_cf[i] = 10 * log10f((y2_cf[i * 2 + 0] * y2_cf[i * 2 + 0] + y2_cf[i * 2 + 1] * y2_cf[i * 2 + 1]) / N);
        // Simple way to show two power spectrums as one plot
        sum_y[i] = fmax(y1_cf[i], y2_cf[i]);
    }

    // Show power spectrum in 64x10 window from -100 to 0 dB from 0..N/4 samples
    ESP_LOGW(TAG, "Signal x1");
    dsps_view(y1_cf, N / 2, 64, 10,  -60, 40, '|');
    ESP_LOGW(TAG, "Signal x2");
    dsps_view(y2_cf, N / 2, 64, 10,  -60, 40, '|');
    ESP_LOGW(TAG, "Signals x1 and x2 on one plot");
    dsps_view(sum_y, N / 2, 64, 10,  -60, 40, '|');
    ESP_LOGI(TAG, "FFT for %i complex points take %i cycles", N, end_b - start_b);

    ESP_LOGI(TAG, "End Example.");
}
