/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <malloc.h>
#include <stdint.h>
#include "unity.h"
#include "dsp_platform.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include <math.h>

#include "dsp_tests.h"
#include "dsps_cplx_gen.h"
#include "dsps_wind.h"
#include "dsps_view.h"
#include "dsps_fft2r.h"

#define LEAKAGE_BINS 10                     // fft leakage bins

static const char *TAG = "dsps_cplx_gen";

// Error message handler function, which detects errors returned by dsps_cplx_gen_init() function
void error_msg_handler(cplx_sig_t *cplx_signal, esp_err_t status)
{
    if (status != ESP_OK) {
        cplx_gen_free(cplx_signal);

        switch (status) {
        case ESP_ERR_DSP_INVALID_LENGTH:
            TEST_ASSERT_MESSAGE(false, "LUT table has invalid length, must be power of 2");
            break;
        case ESP_ERR_DSP_PARAM_OUTOFRANGE:
            TEST_ASSERT_MESSAGE(false, "LUT table length must be in a range from 256 to 8192");
            break;
        case ESP_ERR_DSP_INVALID_PARAM:
            TEST_ASSERT_MESSAGE(false, "Frequency and initial phase must be in a range from -1 to 1");
            break;
        default:
            TEST_ASSERT_MESSAGE(false, "Unspecified error");
            break;
        }
    }
}

TEST_CASE("cplx_gen_functionality_test", "[dsps]")
{
    const int32_t out_len = 4096;
    const int32_t lut_len = 1024;
    const float frequency = 0.001;
    const float init_phase = 0.1;

    cplx_sig_t cplx_signal, cplx_signal_compare;

    // F32 float
    esp_err_t status1 = dsps_cplx_gen_init(&cplx_signal, F32_FLOAT, NULL, lut_len, frequency, init_phase);
    error_msg_handler(&cplx_signal, status1);
    esp_err_t status2 = dsps_cplx_gen_init(&cplx_signal_compare, F32_FLOAT, cplx_signal.lut, lut_len, frequency, init_phase);
    error_msg_handler(&cplx_signal_compare, status2);

    float *out_array_float = (float *)malloc(out_len * 2 * sizeof(float));          // times 2 for real and complex part
    float *out_array_compare_float = (float *)malloc(out_len * 2 * sizeof(float));

    dsps_cplx_gen_ansi(&cplx_signal_compare, (void *)out_array_compare_float, out_len);
    dsps_cplx_gen(&cplx_signal, (void *)out_array_float, out_len);

    for (int i = 0; i < out_len * 2; i++) {
        TEST_ASSERT_EQUAL(out_array_compare_float[i], out_array_float[i]);
    }

    free(out_array_float);
    free(out_array_compare_float);
    cplx_gen_free(&cplx_signal);
    cplx_gen_free(&cplx_signal_compare);

    // S16 fixed
    status1 = dsps_cplx_gen_init(&cplx_signal, S16_FIXED, NULL, lut_len, frequency, init_phase);
    error_msg_handler(&cplx_signal, status1);
    status2 = dsps_cplx_gen_init(&cplx_signal_compare, S16_FIXED, cplx_signal.lut, lut_len, frequency, init_phase);
    error_msg_handler(&cplx_signal_compare, status2);

    int16_t *out_array_fixed = (int16_t *)malloc(out_len * 2 * sizeof(int16_t));    // times 2 for real and complex part
    int16_t *out_array_compare_fixed = (int16_t *)malloc(out_len * 2 * sizeof(int16_t));

    dsps_cplx_gen_ansi(&cplx_signal_compare, (void *)out_array_compare_fixed, out_len);
    dsps_cplx_gen(&cplx_signal, (void *)out_array_fixed, out_len);

    for (int i = 0; i < out_len * 2; i++) {
        TEST_ASSERT_EQUAL(out_array_compare_fixed[i], out_array_fixed[i]);
    }

    free(out_array_fixed);
    free(out_array_compare_fixed);
    cplx_gen_free(&cplx_signal);
    cplx_gen_free(&cplx_signal_compare);
}


TEST_CASE("cplx_gen_benchmark_test", "[dsps]")
{
    int32_t out_len = 32;
    const int32_t lut_len = 256;
    const float frequency = 0.02;
    const float init_phase = 0.9;
    const int repeat_count = 4;

    cplx_sig_t cplx_signal_float, cplx_signal_fixed;

    esp_err_t status1 = dsps_cplx_gen_init(&cplx_signal_float, F32_FLOAT, NULL, lut_len, frequency, init_phase);
    error_msg_handler(&cplx_signal_float, status1);
    esp_err_t status2 = dsps_cplx_gen_init(&cplx_signal_fixed, S16_FIXED, NULL, lut_len, frequency, init_phase);
    error_msg_handler(&cplx_signal_fixed, status2);

    float *out_array_float = (float *)malloc(out_len * 2 * 32 * sizeof(float));    // 8192 (max_out len) * 2 (real and imaginary)
    int16_t *out_array_fixed = (int16_t *)malloc(out_len * 2 * 32 * sizeof(int16_t));

    for (int i = 0; i < 6; i++) {
        const unsigned int start_float = dsp_get_cpu_cycle_count();
        for (int j = 0 ; j < repeat_count ; j++) {
            dsps_cplx_gen(&cplx_signal_float, (void *)out_array_float, out_len);
        }
        const unsigned int end_float = dsp_get_cpu_cycle_count();

        const unsigned int start_fixed = dsp_get_cpu_cycle_count();
        for (int j = 0 ; j < repeat_count ; j++) {
            dsps_cplx_gen(&cplx_signal_fixed, (void *)out_array_fixed, out_len);
        }
        const unsigned int end_fixed = dsp_get_cpu_cycle_count();

        const float total_float = end_float - start_float;
        const float total_fixed = end_fixed - start_fixed;
        const float cycles_float = total_float / (float)(repeat_count);
        const float cycles_fixed = total_fixed / (float)(repeat_count);
        const float cycles_per_lut_sample_float = total_float / (float)(out_len * repeat_count);
        const float cycles_per_lut_sample_fixed = total_fixed / (float)(out_len * repeat_count);

        ESP_LOGI(TAG, "Float : %.2f total cycles, %.2f cycles per sample, for %"PRId32" LUT samples, %"PRId32" output array length",
                 cycles_float, cycles_per_lut_sample_float, lut_len, out_len);

        ESP_LOGI(TAG, "Fixed : %.2f total cycles, %.2f cycles per sample, for %"PRId32" LUT samples, %"PRId32" output array length \n",
                 cycles_fixed, cycles_per_lut_sample_fixed, lut_len, out_len);

        out_len *= 2;
    }

    free(out_array_fixed);
    free(out_array_float);
    cplx_gen_free(&cplx_signal_float);
    cplx_gen_free(&cplx_signal_fixed);
}


TEST_CASE("cplx_gen_noise_SNR_test", "[dsps]")
{
    const int32_t out_len = 2048;
    const int32_t lut_len = 8192;
    const int32_t n_fft = out_len * 2;      // * 2 (real and imaginary)
    const float frequency = 0.01;
    const float init_phase = 0.0;
    const float real_ampl = 0.5;
    const float imag_ampl = 0.2;

    cplx_sig_t cplx_signal_float;

    esp_err_t status = dsps_cplx_gen_init(&cplx_signal_float, F32_FLOAT, NULL, lut_len, frequency, init_phase);
    error_msg_handler(&cplx_signal_float, status);

    float *out_array_float = (float *)memalign(16, n_fft * sizeof(float));
    dsps_cplx_gen(&cplx_signal_float, (void *)out_array_float, out_len);

    // Signal windowing
    float *window = (float *)memalign(16, out_len * sizeof(float));
    dsps_wind_blackman_harris_f32(window, out_len);

    for (int i = 0 ; i < out_len ; i++) {
        out_array_float[i * 2 + 0] *= (window[i] * real_ampl);
        out_array_float[i * 2 + 1] *= (window[i] * imag_ampl);
    }
    free(window);

    // Initialize FFT
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, n_fft);
    if (ret  != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to initialize FFT. Error = %i", ret);
        return;
    }

    // Do the FFT
    dsps_fft2r_fc32(out_array_float, out_len);
    dsps_bit_rev_fc32(out_array_float, out_len);
    dsps_cplx2reC_fc32(out_array_float, out_len);

    // Convert the FFT spectrum from amplitude to watts, find the max value and its position
    float max_val_1 = -1000000, max_val_2 = -1000000;
    int max_pos_1 = 0, max_pos_2 = 0, spur_pos_1 = 0, spur_pos_2 = 0;
    for (int i = 0 ; i < n_fft / 2 ; i++) {
        out_array_float[i] = (out_array_float[i * 2 + 0] * out_array_float[i * 2 + 0] + out_array_float[i * 2 + 1] * out_array_float[i * 2 + 1]) / (n_fft * 3);
        if (i < n_fft / 4) {
            if (out_array_float[i] > max_val_1) {
                max_val_1 = out_array_float[i];
                max_pos_1 = i;
            }
        } else {
            if (out_array_float[i] > max_val_2) {
                max_val_2 = out_array_float[i];
                max_pos_2 = i;
            }
        }
    }

    // Calculate the power of the signal and noise of the spectrum and convert the spectrum to dB
    float signal_pow_1 = 0, signal_pow_2 = 0, noise_pow_1 = 0, noise_pow_2 = 0;
    float spur_1 = -1000000, spur_2 = -1000000;
    for (int i = 0 ; i < n_fft / 2 ; i++) {
        if (i < n_fft / 4) {
            if ((i >= max_pos_1 - LEAKAGE_BINS) && (i <= max_pos_1 + LEAKAGE_BINS)) {
                signal_pow_1 += out_array_float[i];
            } else {
                noise_pow_1 += out_array_float[i];
                if (out_array_float[i] > spur_1) {
                    spur_1 = out_array_float[i];
                    spur_pos_1 = i;
                }
            }
        } else {
            if ((i >= max_pos_2 - LEAKAGE_BINS) && (i <= max_pos_2 + LEAKAGE_BINS)) {
                signal_pow_2 += out_array_float[i];
            } else {
                noise_pow_2 += out_array_float[i];
                if (out_array_float[i] > spur_2) {
                    spur_2 = out_array_float[i];
                    spur_pos_2 = i;
                }
            }
        }
        out_array_float[i] = 10 * log10f(0.0000000000001 + out_array_float[i]);
    }

    // Convert the signal power and noise power from watts to dB and calculate SNR and SFDR
    const float snr_1 = 10 * log10f(signal_pow_1 / noise_pow_1);
    const float snr_2 = 10 * log10f(signal_pow_2 / noise_pow_2);
    noise_pow_1 = 10 * log10f(noise_pow_1);
    noise_pow_2 = 10 * log10f(noise_pow_2);
    signal_pow_1 = 10 * log10f(signal_pow_1);
    signal_pow_2 = 10 * log10f(signal_pow_2);
    const float sfdr_1 = out_array_float[max_pos_1] - out_array_float[spur_pos_1];
    const float sfdr_2 = out_array_float[max_pos_2] - out_array_float[spur_pos_2];

    ESP_LOGI(TAG, "\nSignal Power: \t%f\nNoise Power: \t%f\nSNR: \t\t%f \nSFDR: \t\t%f", signal_pow_1, noise_pow_1, snr_1, sfdr_1);
    dsps_view(out_array_float, n_fft / 4, 64, 16,  -140, 40, '|');
    putchar('\n');

    ESP_LOGI(TAG, "\nSignal Power: \t%f\nNoise Power: \t%f\nSNR: \t\t%f \nSFDR: \t\t%f", signal_pow_2, noise_pow_2, snr_2, sfdr_2);
    dsps_view(out_array_float + (n_fft / 4), n_fft / 4, 64, 16,  -140, 40, '|');

    free(out_array_float);
    cplx_gen_free(&cplx_signal_float);
}
