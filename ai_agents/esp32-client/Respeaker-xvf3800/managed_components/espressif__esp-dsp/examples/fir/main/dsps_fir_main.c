/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <math.h>
#include <malloc.h>

#include "esp_dsp.h"


static const char *TAG = "main";

// This example shows how to use FIR filters from esp-dsp library

#define FIR_COEFFS_LEN      64                              // Number of FIR filter coefficients
#define DECIMATION          2                               // Decimation ratio of the FIR filter
#define N_SAMPLES           1024                            // Input samples
#define FIR_DELAY           (FIR_COEFFS_LEN / DECIMATION)   // Amount of samples not being considered for the FFT
#define FIR_BUFF_OUT_LEN    (N_SAMPLES + FIR_DELAY)         // Total length of samples


// Function shows the result of the FIR filter
void show_FFT(float *input_signal, const unsigned int fft_len)
{

    dsps_fft2r_fc32(input_signal, fft_len >> 1);
    dsps_bit_rev2r_fc32(input_signal, fft_len >> 1);
    dsps_cplx2real_fc32(input_signal, fft_len >> 1);

    // Correction factor for the FFT spectrum
    const float correction_factor = fft_len * 3;

    // Calculating power of spectrum in dB
    for (int i = 0 ; i < fft_len / 2 ; i++) {
        input_signal[i] = 10 * log10f((input_signal[i * 2 + 0] * input_signal[i * 2 + 0] + input_signal[i * 2 + 1] * input_signal[i * 2 + 1] + 0.0000001) / correction_factor);
    }

    // Display power spectrum
    dsps_view(input_signal, fft_len / 2, 64, 10,  -120, 40, '|');
}


// Generate Windowed-Sinc filter coefficients
void generate_FIR_coefficients(float *fir_coeffs, const unsigned int fir_len, const float ft)
{

    // Even or odd length of the FIR filter
    const bool is_odd = (fir_len % 2) ? (true) : (false);
    const float fir_order = (float)(fir_len - 1);

    // Window coefficients
    float *fir_window = (float *)malloc(fir_len * sizeof(float));
    dsps_wind_blackman_f32(fir_window, fir_len);

    for (int i = 0; i < fir_len; i++) {
        if ((i == fir_order / 2) && (is_odd)) {
            fir_coeffs[i] = 2 * ft;
        } else {
            fir_coeffs[i] = sinf((2 * M_PI * ft * (i - fir_order / 2))) / (M_PI * (i - fir_order / 2));
        }

        fir_coeffs[i] *= fir_window[i];
    }

    free(fir_window);
}

static __attribute__((aligned(16))) float tone_combined[FIR_BUFF_OUT_LEN];
static __attribute__((aligned(16))) float fir_coeffs[FIR_COEFFS_LEN];
static __attribute__((aligned(16))) float delay_line[FIR_COEFFS_LEN];

void app_main()
{
    const int32_t fir_len = FIR_COEFFS_LEN;
    const float fir_ft = 0.5 / DECIMATION;                                  // Transition frequency of the FIR filter
    const int32_t N = N_SAMPLES;                                            // Number of input samples
    const int32_t fir_decim = DECIMATION;                                   // FIR filter decimation
    const int32_t N_buff = FIR_BUFF_OUT_LEN;                                // Total length of samples with ignored

    fir_f32_t fir1;
    esp_err_t ret;

    // Ignoring the first set of samples, due to the delay line of the FIR filter
    const int fir_out_offset = ((FIR_DELAY / 2) - 1);

    ESP_LOGI(TAG, "Start Example.");

    // If a user doesn't care about buffer allocation, the default
    // initialization could be used as shown here:
    ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret  != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to initialize FFT. Error = %i", ret);
        return;
    }

    ret = dsps_fft4r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret  != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to initialize FFT. Error = %i", ret);
        return;
    }

    // Generate input signal as 2 sine waves
    float *tone_1 = (float *)malloc(N_buff * sizeof(float));
    float *tone_2 = (float *)malloc(N_buff * sizeof(float));

    dsps_tone_gen_f32(tone_1, N_buff, 5, 0.2, 0);
    dsps_tone_gen_f32(tone_2, N_buff, 5, 0.4, 0);

    // Generate windowing coefficients
    float *window = (float *)malloc(N * sizeof(float));
    dsps_wind_blackman_harris_f32(window, N);

    // Add the two waves together
    for (int i = 0 ; i < N_buff ; i++) {
        tone_combined[i] = tone_1[i] + tone_2[i];
    }

    free(tone_1);
    free(tone_2);

    // Apply the windowing
    for (int i = 0 ; i < N ; i++) {
        window[i] *= tone_combined[i];
    }

    // Show FFT spectrum
    show_FFT(window, N);

    // Calculate coefficients for the FIR filter
    generate_FIR_coefficients(fir_coeffs, fir_len, fir_ft);
    ESP_LOGI(TAG, "\n");

    // Filter the input signal with FIR filter
    float *fir_out = (float *)malloc( N_buff * sizeof(float));
    dsps_fird_init_f32(&fir1, fir_coeffs, delay_line, fir_len, fir_decim);

    uint32_t start_b = dsp_get_cpu_cycle_count();
    dsps_fird_f32(&fir1, tone_combined, fir_out, N_buff / fir_decim);
    uint32_t end_b = dsp_get_cpu_cycle_count();

    // Generate windowing coefficients and apply the windowing
    dsps_wind_blackman_harris_f32(window, (N / fir_decim));
    for (int i = 0 ; i < N / fir_decim ; i++) {
        window[i] *= fir_out[fir_out_offset + i];
    }

    // Show FFT spectrum, ignoring first samples from the delay line
    show_FFT(window, N / fir_decim);
    ESP_LOGI(TAG, "FIR for %"PRId32" samples and decimation %"PRId32" takes %"PRId32" cycles", N, fir_decim, (int32_t)(end_b - start_b));
    ESP_LOGI(TAG, "End Example.");

    free(fir_out);
    free(window);

}
