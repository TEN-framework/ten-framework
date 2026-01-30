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

#define N_SAMPLES 2048 // Amount of real input samples
int N = N_SAMPLES;
// Input test array
__attribute__((aligned(16)))
float x1[N_SAMPLES];
__attribute__((aligned(16)))
float x2[N_SAMPLES];
// Window coefficients
__attribute__((aligned(16)))
float wind[N_SAMPLES];
// Pointers to result arrays
float *y1_cf = &x1[0];
float *y2_cf = &x2[0];

// diff of y1 and y2
__attribute__((aligned(16)))
float diff_y[N_SAMPLES / 2];

void app_main()
{
    esp_err_t ret;
    ESP_LOGI(TAG, "Start Example.");
    ret = dsps_fft2r_init_fc32(NULL, N >> 1);
    if (ret  != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to initialize FFT2R. Error = %i", ret);
        return;
    }

    ret = dsps_fft4r_init_fc32(NULL, N >> 1);
    if (ret  != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to initialize FFT4R. Error = %i", ret);
        return;
    }

    // Generate hann window
    dsps_wind_hann_f32(wind, N);
    // Generate input signal for x1 A=1 , F=0.1
    dsps_tone_gen_f32(x1, N, 1.0, 0.16,  0);

    // Convert two input vectors to one complex vector
    for (int i = 0 ; i < N ; i++) {
        x1[i] = x1[i] * wind[i];
        x2[i] = x1[i];
    }
    // FFT Radix-2
    unsigned int start_r2 = dsp_get_cpu_cycle_count();
    dsps_fft2r_fc32(x1, N >> 1);
    // Bit reverse
    dsps_bit_rev2r_fc32(x1, N >> 1);
    // Convert one complex vector with length N/2 to one real spectrum vector with length N/2
    dsps_cplx2real_fc32(x1, N >> 1);
    unsigned int end_r2 = dsp_get_cpu_cycle_count();

    // FFT Radix-4
    unsigned int start_r4 = dsp_get_cpu_cycle_count();
    dsps_fft4r_fc32(x2, N >> 1);
    // Bit reverse
    dsps_bit_rev4r_fc32(x2, N >> 1);
    // Convert one complex vector with length N/2 to one real spectrum vector with length N/2
    dsps_cplx2real_fc32(x2, N >> 1);
    unsigned int end_r4 = dsp_get_cpu_cycle_count();

    for (int i = 0 ; i < N / 2 ; i++) {
        x1[i] = 10 * log10f((x1[i * 2 + 0] * x1[i * 2 + 0] + x1[i * 2 + 1] * x1[i * 2 + 1] + 0.0000001) / N);
        x2[i] = 10 * log10f((x2[i * 2 + 0] * x2[i * 2 + 0] + x2[i * 2 + 1] * x2[i * 2 + 1] + 0.0000001) / N);
        // Simple way to show two power spectrums as one plot
        diff_y[i] = fabs(x1[i] - x2[i]);
    }

    // Show power spectrum in 64x10 window from -100 to 0 dB from 0..N/4 samples
    ESP_LOGW(TAG, "Signal x1");
    dsps_view(x1, N / 2, 64, 10,  -60, 40, '|');
    ESP_LOGW(TAG, "Signal x2");
    dsps_view(x2, N / 2, 64, 10,  -60, 40, '|');
    ESP_LOGW(TAG, "Difference between signals x1 and x2 on one plot");
    dsps_view(diff_y, N / 2, 64, 10,  0, 40, '-');
    ESP_LOGI(TAG, "FFT Radix 2 for %i complex points take %i cycles", N / 2, end_r2 - start_r2);
    ESP_LOGI(TAG, "FFT Radix 4 for %i complex points take %i cycles", N / 2, end_r4 - start_r4);

    ESP_LOGI(TAG, "End Example.");
}
