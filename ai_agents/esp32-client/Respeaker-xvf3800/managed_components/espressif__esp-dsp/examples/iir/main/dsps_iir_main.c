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

// This example shows how to use iir filters from esp-dsp library

#define N_SAMPLES 1024
int N = N_SAMPLES;
// Input test array
__attribute__((aligned(16)))
float d[N_SAMPLES];
// output array
__attribute__((aligned(16)))
float y[N_SAMPLES];
__attribute__((aligned(16)))
float y_cf[N_SAMPLES * 2];

// Function shows result of IIR filter
void ShowIIRfilter(float freq, float qFactor)
{
    esp_err_t ret = ESP_OK;
    float coeffs_lpf[5];
    float w_lpf[5] = {0, 0};
    // Calculate iir filter coefficients
    ret = dsps_biquad_gen_lpf_f32(coeffs_lpf, freq, qFactor);
    if (ret  != ESP_OK) {
        ESP_LOGE(TAG, "Operation error = %i", ret);
        return;
    }
    // Process input signal
    unsigned int start_b = dsp_get_cpu_cycle_count();
    ret = dsps_biquad_f32(d, y, N, coeffs_lpf, w_lpf);
    unsigned int end_b = dsp_get_cpu_cycle_count();
    if (ret  != ESP_OK) {
        ESP_LOGE(TAG, "Operation error = %i", ret);
        return;
    }

    // Show result as a plot
    ESP_LOGI(TAG, "Impulse response of IIR filter with F=%f, qFactor=%f", freq, qFactor);
    dsps_view(y, 128, 64, 10,  -1, 1, '-');

    // Show result as frequency responce on the plot
    for (int i = 0 ; i < N ; i++) {
        y_cf[i * 2 + 0] = y[i];
        y_cf[i * 2 + 1] = 0;
    }

    // We making FFT transform
    dsps_fft2r_fc32_ansi(y_cf, N);
    // Bit reverse
    dsps_bit_rev_fc32_ansi(y_cf, N);
    // Calculating power of spectrum in dB
    for (int i = 0 ; i < N / 2 ; i++) {
        y_cf[i] = 10 * log10f((y_cf[i * 2 + 0] * y_cf[i * 2 + 0] + y_cf[i * 2 + 1] * y_cf[i * 2 + 1]) / N);
    }
    // Show power spectrum in 64x10 window from -100 to 0 dB from 0..N/2 samples
    dsps_view(y_cf, N / 2, 64, 10,  -100, 0, '-');
    ESP_LOGI(TAG, "IIR for %i samples take %i cycles", N, end_b - start_b);
}

void app_main()
{
    esp_err_t ret;
    ESP_LOGI(TAG, "Start Example.");
    // If user don't care about buffer allocation, the defalt
    // initialization could be used as shown here:
    ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret  != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to initialize FFT. Error = %i", ret);
        return;
    }

    // Initialize input signal
    // Generate d function as input signal
    dsps_d_gen_f32(d, N, 0);

    // Show filter with Q factor 1
    ShowIIRfilter(0.1, 1);
    // Show filter with Q factor 10
    ShowIIRfilter(0.1, 10);

    ESP_LOGI(TAG, "End Example.");
}
