/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <malloc.h>
#include <stdint.h>
#include "unity.h"
#include "dsp_platform.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_dsp.h"

#include "dsps_tone_gen.h"
#include "dsps_d_gen.h"
#include "dsps_fir.h"
#include "dsp_tests.h"
#include "dsps_wind.h"
#include "dsps_view.h"
#include "dsps_fft2r.h"

#define COEFFS 64
#define N_IN_SAMPLES 1024
#define DECIMATION 2
#define Q15_MAX INT16_MAX
#define LEAKAGE_BINS 10
#define FIR_BUFF_LEN 16

static const char *TAG = "dsps_fird_s16_ansi";

const static int32_t len = N_IN_SAMPLES;
const static int32_t N_FFT = (N_IN_SAMPLES / DECIMATION);
const static int16_t decim = DECIMATION;
const static int16_t fir_len = COEFFS;
const static int32_t fir_buffer = (N_IN_SAMPLES + FIR_BUFF_LEN);


// error messages for the init functions
static void error_msg_handler(fir_s16_t *fir, esp_err_t status)
{

    if (status != ESP_OK) {
        dsps_fird_s16_aexx_free(fir);

        switch (status) {
        case ESP_ERR_DSP_INVALID_LENGTH:
            TEST_ASSERT_MESSAGE(false, "Number of the coefficients must be higher than 1");
            break;
        case ESP_ERR_DSP_ARRAY_NOT_ALIGNED:
            TEST_ASSERT_MESSAGE(false, "Delay line or (and) coefficients arrays not aligned");
            break;
        case ESP_ERR_DSP_PARAM_OUTOFRANGE:
            TEST_ASSERT_MESSAGE(false, "Start position or (and) Decimation ratio or (and) Shift out of range");
            break;
        default:
            TEST_ASSERT_MESSAGE(false, "Unspecified error");
            break;
        }
    }
}


TEST_CASE("dsps_fird_s16_ansi functionality", "[dsps]")
{

    int16_t *x = (int16_t *)memalign(16, len * sizeof(int16_t));
    int16_t *y = (int16_t *)memalign(16, len * sizeof(int16_t));

    int16_t *coeffs = (int16_t *)memalign(16, fir_len * sizeof(int16_t));
    int16_t *delay = (int16_t *)memalign(16, fir_len * sizeof(int16_t));

    const int16_t start_pos = 0;
    const int16_t shift = 0;
    const int16_t dec = decim;
    const int32_t output_len = (int32_t)(len / dec);

    fir_s16_t fir1;
    for (int i = 0 ; i < fir_len ; i++) {
        coeffs[i] = 0;
    }
    coeffs[0] = 0x4000;

    for (int i = 0 ; i < len ; i++) {
        x[i] = 0x4000;
    }

    esp_err_t status = dsps_fird_init_s16(&fir1, coeffs, delay, fir_len, dec, start_pos, shift);
    error_msg_handler(&fir1, status);

    const int32_t total = dsps_fird_s16_ansi(&fir1, x, y, output_len);

    ESP_LOGI(TAG, "%"PRId32" input samples, decimation %"PRId16",total result = %"PRId32"\n", len, dec, total);
    TEST_ASSERT_EQUAL(total, len / decim);
    for (int i = 0 ; i < total ; i++) {
        ESP_LOGD(TAG, "data[%i] = %d\n", i, y[i]);
    }

    for (int i = 0 ; i < total ; i++) {
        TEST_ASSERT_EQUAL(y[i], (0x2000));
    }

    dsps_fird_s16_aexx_free(&fir1);
    free(x);
    free(y);
    free(coeffs);
    free(delay);
}


TEST_CASE("dsps_fird_s16_ansi benchmark", "[dsps]")
{

    int16_t *x = (int16_t *)memalign(16, len * sizeof(int16_t));
    int16_t *y = (int16_t *)memalign(16, len * sizeof(int16_t));

    int16_t *coeffs = (int16_t *)memalign(16, fir_len * sizeof(int16_t));
    int16_t *delay = (int16_t *)memalign(16, fir_len * sizeof(int16_t));

    const int repeat_count = 4;
    const int16_t dec = 1;
    const int16_t start_pos = 0;
    const int16_t shift = 0;
    int32_t output_len = 0;

    fir_s16_t fir1;
    for (int i = 0 ; i < fir_len ; i++) {
        coeffs[i] = i;
    }

    for (int i = 0 ; i < len ; i++) {
        x[i] = 0;
    }
    x[0] = 1;

    esp_err_t status = dsps_fird_init_s16(&fir1, coeffs, delay, fir_len, dec, start_pos, shift);
    error_msg_handler(&fir1, status);

    // Decimations 1, 2, 4, 8
    for (int i = 0 ; i < 4 ; i++) {

        output_len = (int32_t)(len / fir1.decim);
        const unsigned int start_b = dsp_get_cpu_cycle_count();
        for (int i = 0 ; i < repeat_count ; i++) {
            dsps_fird_s16_ansi(&fir1, x, y, output_len);
        }
        const unsigned int end_b = dsp_get_cpu_cycle_count();

        const float total_b = end_b - start_b;
        const float cycles = total_b / (len * repeat_count);

        ESP_LOGI(TAG, "total cycles %f per sample for %"PRId16" coefficients, decimation %"PRId16", %f per decim tap \n",
                 cycles, fir_len, fir1.decim, cycles / (float)fir_len * fir1.decim);
        float min_exec = 10;
        float max_exec = 1500;
        TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);
        fir1.decim *= 2;
    }

    dsps_fird_s16_aexx_free(&fir1);
    free(x);
    free(y);
    free(coeffs);
    free(delay);
}


TEST_CASE("dsps_fird_s16_ansi noise_snr", "[dsps]")
{

    // In the SNR-noise test we are generating a sine wave signal, filtering the signal using a fixed point FIRD filter
    // and do the FFT of the filtered signal. Afterward, a noise and SNR calculated from the FFT spectrum

    // FIR Coeffs
    int16_t *s_coeffs = (int16_t *)memalign(16, fir_len * sizeof(int16_t));         // fixed point coefficients
    int16_t *delay_line = (int16_t *)memalign(16, fir_len * sizeof(int16_t));       // fixed point delay line
    float *f_coeffs = (float *)memalign(16, fir_len * sizeof(float));               // floating point coefficients

    // Coefficients windowing
    dsps_wind_hann_f32(f_coeffs, fir_len);
    const float fir_order = (float)fir_len - 1;
    const float ft = 0.25;                                                          // sine frequency
    for (int i = 0; i < fir_len; i++) {
        f_coeffs[i] *= sinf((2 * M_PI * ft * (i - fir_order / 2))) / (M_PI * (i - fir_order / 2));
    }

    // FIR coefficients conversion to q15
    for (int i = 0; i < fir_len; i++) {
        s_coeffs[i] = f_coeffs[i] * Q15_MAX;
    }

    free(f_coeffs);

    // Signal generation
    const float amplitude = 0.9;
    const float frequency = 0.05;
    const float phase = 0;
    float *f_in_signal = (float *)memalign(16, fir_buffer * sizeof(float));
    dsps_tone_gen_f32(f_in_signal, fir_buffer, amplitude, frequency, phase);

    // Input signal conversion to q15
    int16_t *fir_x = (int16_t *)memalign(16, fir_buffer * sizeof(int16_t));
    int16_t *fir_y = (int16_t *)memalign(16, fir_buffer * sizeof(int16_t));
    for (int i = 0; i < fir_buffer; i++) {
        fir_x[i] = f_in_signal[i] * (int16_t)Q15_MAX;
    }

    free(f_in_signal);

    // FIR
    const int16_t start_pos = 0;
    const int16_t shift = 0;
    const int32_t output_len = (int32_t)(fir_buffer / decim);
    fir_s16_t fir1;
    esp_err_t status = dsps_fird_init_s16(&fir1, s_coeffs, delay_line, fir_len, decim, start_pos, shift);
    error_msg_handler(&fir1, status);
    dsps_fird_s16_ansi(&fir1, fir_x, fir_y, output_len);

    free(delay_line);
    free(s_coeffs);
    free(fir_x);

    // FIR Output conversion to float
    const unsigned int ignored_fir_samples = (FIR_BUFF_LEN / 2) - 1;
    float *fir_output = (float *)memalign(16, len * sizeof(float));
    for (int i = 0; i < N_FFT; i++) {
        fir_output[i] = (float)(fir_y[ignored_fir_samples + i] / (float)Q15_MAX);
    }

    free(fir_y);

    // Signal windowing
    float *window = (float *)memalign(16, N_FFT * sizeof(float));
    dsps_wind_blackman_f32(window, N_FFT);

    // Prepare FFT input, real and imaginary part
    const int32_t fft_data_len = (N_IN_SAMPLES / DECIMATION) * 2;
    float *fft_data = (float *)memalign(16, fft_data_len * sizeof(float));
    for (int i = 0 ; i < N_FFT ; i++) {
        fft_data[i * 2 + 0] = fir_output[i] * window[i];
        fft_data[i * 2 + 1] = 0;
    }
    free(fir_output);
    free(window);

    // Initialize FFT
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, N_FFT * 2);
    TEST_ESP_OK(ret);

    // Do the FFT
    dsps_fft2r_fc32(fft_data, N_FFT);
    dsps_bit_rev_fc32(fft_data, N_FFT);
    dsps_cplx2reC_fc32(fft_data, N_FFT);

    // Convert the FFT spectrum from amplitude to watts, find the max value and its position
    float max_val = -1000000;
    int max_pos = 0;
    for (int i = 0 ; i < N_FFT / 2 ; i++) {
        fft_data[i] = (fft_data[i * 2 + 0] * fft_data[i * 2 + 0] + fft_data[i * 2 + 1] * fft_data[i * 2 + 1]) / (N_FFT * 3);
        if (fft_data[i] > max_val) {
            max_val = fft_data[i];
            max_pos = i;
        }
    }

    // Calculate the power of the signal and noise of the spectrum and convert the spectrum to dB
    float signal_pow = 0, noise_pow = 0;
    for (int i = 0 ; i < N_FFT / 2 ; i++) {
        if ((i >= max_pos - LEAKAGE_BINS) && (i <= max_pos + LEAKAGE_BINS)) {
            signal_pow += fft_data[i];
        } else {
            noise_pow += fft_data[i];
        }

        fft_data[i] = 10 * log10f(0.0000000000001 + fft_data[i]);
    }

    // Convert the signal power and noise power to dB, calculate SNR
    const float snr = 10 * log10f(signal_pow / noise_pow);
    noise_pow = 10 * log10f(noise_pow);
    signal_pow = 10 * log10f(signal_pow);

    ESP_LOGI(TAG, "\nSignal Power: \t%f\nNoise Power: \t%f\nSNR: \t\t%f", signal_pow, noise_pow, snr);
    dsps_view(fft_data, N_FFT / 2, 64, 16,  -140, 40, '|');
    free(fft_data);

    const float min_exec_snr = 50;
    const float max_exec_snr = 120;
    TEST_ASSERT_EXEC_IN_RANGE(min_exec_snr, max_exec_snr, snr);
    dsps_fird_s16_aexx_free(&fir1);

}
