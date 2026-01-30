/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <string.h>
#include <malloc.h>
#include <stdbool.h>
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

#define COEFFS 256
#define N_IN_SAMPLES 4096
#define DECIMATION 2
#define Q15_MAX INT16_MAX
#define LEAKAGE_BINS 10
#define FIR_BUFF_LEN 16

#define MAX_FIR_LEN 64

static const char *TAG = "dsps_fird_s16_aexx";

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



TEST_CASE("dsps_fird_s16_aexx functionality", "[dsps]")
{

    const int32_t max_len[2] = {2048, 2520};                        // 2520 can be divided by 3, 6, 9, 12, 15, 18 and 21
    const int16_t max_dec[2] = {32, 21};
    const int16_t min_dec[2] = {2, 3};
    const int16_t shift_vals[17] = {-15, 0, 15};

    int16_t *x = (int16_t *)memalign(16, max_len[1] * sizeof(int16_t));
    int16_t *y = (int16_t *)memalign(16, max_len[1] * sizeof(int16_t));
    int16_t *y_compare = (int16_t *)memalign(16, max_len[1] * sizeof(int16_t));

    int16_t *coeffs = (int16_t *)memalign(16, MAX_FIR_LEN * sizeof(int16_t));
    int16_t *coeffs_aexx = (int16_t *)memalign(16, MAX_FIR_LEN * sizeof(int16_t));
    int16_t *coeffs_ansi = (int16_t *)memalign(16, MAX_FIR_LEN * sizeof(int16_t));

    int16_t *delay = (int16_t *)memalign(16, MAX_FIR_LEN * sizeof(int16_t));
    int16_t *delay_compare = (int16_t *)memalign(16, MAX_FIR_LEN * sizeof(int16_t));

    int32_t combinations = 0;
    esp_err_t status1 = ESP_OK, status2 = ESP_OK;
    fir_s16_t fir1, fir2;

    for (int i = 0 ; i < MAX_FIR_LEN ; i++) {
        coeffs[i] = i + 0x100;
    }

    for (int i = 0 ; i < max_len[1] ; i++) {
        x[i] = 0x10;
    }

    for (int variations = 0; variations < 2; variations++) {

        ESP_LOGI(TAG, ": %"PRId32" input samples, coefficients range from 2 to %"PRId16", decimation range from %"PRId16" to %"PRId16", shift in range from -40 to 40 and start positions within the coeffs range",
                 max_len[variations], (int16_t)MAX_FIR_LEN, min_dec[variations], max_dec[variations]);

        // decimation increment is set as dec * 2 for input data length 2048 (2, 4, 8, 16, 32)
        //                             as dec + 3 for input data length 2520 (3, 6, 9, 12, 15, 18, 21)
        for (int16_t dec = min_dec[variations]; dec <= max_dec[variations]; ((variations) ? (dec += 3) : (dec *= 2)) ) {

            const int32_t loop_len = max_len[variations] / dec;
            const int16_t start_position = 0;

            for (int16_t fir_length = 2; fir_length <= MAX_FIR_LEN; fir_length += 16) {

                for (int16_t shift_amount = 0; shift_amount < sizeof(shift_vals) / sizeof(uint16_t); shift_amount++) {

                    for (int k = 0 ; k < fir_length; k++) {
                        coeffs_ansi[k] = coeffs[k];
                        coeffs_aexx[k] = coeffs[k];
                    }

                    status1 = dsps_fird_init_s16(&fir1, coeffs_aexx, delay, fir_length, dec, start_position, shift_vals[shift_amount]);
                    error_msg_handler(&fir1, status1);
                    status2 = dsps_fird_init_s16(&fir2, coeffs_ansi, delay_compare, fir_length, dec, start_position, shift_vals[shift_amount]);
                    error_msg_handler(&fir2, status2);

#if(dsps_fird_s16_aes3_enabled)
                    dsps_16_array_rev(fir1.coeffs, fir1.coeffs_len);        // coefficients are being reverted for the purposes of the aes3 TIE implementation
#endif

                    for (int16_t start_pos = 0; start_pos < dec; start_pos++) {

                        fir1.d_pos = start_pos;
                        fir2.d_pos = start_pos;

                        for (int j = 0; j < fir1.coeffs_len; j++) {
                            fir1.delay[j] = 0;
                            fir2.delay[j] = 0;
                        }

                        fir1.pos = 0;
                        fir2.pos = 0;

                        const int32_t total1 = dsps_fird_s16(&fir1, x, y, loop_len);
                        const int32_t total2 = dsps_fird_s16_ansi(&fir2, x, y_compare, loop_len);

                        TEST_ASSERT_EQUAL(total1, total2);
                        for (int i = 0 ; i < total1 ; i++) {
                            TEST_ASSERT_EQUAL(y[i], y_compare[i]);
                        }

                        combinations++;
                    }

                    dsps_fird_s16_aexx_free(&fir1);
                    dsps_fird_s16_aexx_free(&fir2);
                }
            }
        }
    }

    ESP_LOGI(TAG, ": %"PRId32" total filter combinations\n", combinations);

    free(x);
    free(y);
    free(coeffs);
    free(delay);
    free(y_compare);
    free(coeffs_ansi);
    free(coeffs_aexx);
    free(delay_compare);

}

TEST_CASE("dsps_fird_s16_aexx benchmark", "[dsps]")
{

    const int16_t local_dec = 2;
    const int32_t local_len = (len % 16) ? (4096) : (len);                          // length must be devisible by 16

    int16_t *x = (int16_t *)memalign(16, local_len * sizeof(int16_t));
    int16_t *y = (int16_t *)memalign(16, local_len * sizeof(int16_t));

    int16_t *coeffs = (int16_t *)memalign(16, MAX_FIR_LEN * sizeof(int16_t));
    int16_t *delay = (int16_t *)memalign(16, MAX_FIR_LEN * sizeof(int16_t));

    const int repeat_count = 100;
    const int16_t start_pos = 0;
    const int16_t shift = 0;
    int32_t loop_len = 0;

    fir_s16_t fir;
    esp_err_t status = ESP_OK;

    status = dsps_fird_init_s16(&fir, coeffs, delay, MAX_FIR_LEN, local_dec, start_pos, shift);
    error_msg_handler(&fir, status);

#if(dsps_fird_s16_aes3_enabled)
    dsps_16_array_rev(fir.coeffs, fir.coeffs_len);
#endif

    // Test for decimations 2, 4, 8, 16
    for (int dec = local_dec; dec <= 16 ; dec *= 2) {

        loop_len = (local_len / dec);
        fir.decim = dec;

        const unsigned int start_b = dsp_get_cpu_cycle_count();
        for (int j = 0 ; j < repeat_count ; j++) {
            dsps_fird_s16(&fir, x, y, loop_len);
        }
        const unsigned int end_b = dsp_get_cpu_cycle_count();

        const float total_b = end_b - start_b;
        const float cycles = total_b / (float)(repeat_count);
        const float cycles_per_sample = total_b / (float)(local_len * repeat_count);
        const float cycles_per_decim_tap = cycles_per_sample / (float)(fir.coeffs_len * fir.decim);

        ESP_LOGI(TAG, ": %.2f total cycles, %.2f cycles per sample, %.2f per decim tap, for %"PRId32" input samples, %"PRId16" coefficients and decimation %"PRId16"\n",
                 cycles, cycles_per_sample, cycles_per_decim_tap, local_len, (int16_t)MAX_FIR_LEN, fir.decim);

        const float min_exec = (((local_len / fir.decim) * fir.coeffs_len) / 2);
        const float max_exec = min_exec * 20;
        TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);

    }

    dsps_fird_s16_aexx_free(&fir);
    free(x);
    free(y);
    free(coeffs);
    free(delay);
}

TEST_CASE("dsps_fird_s16_aexx noise_snr", "[dsps]")
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
    const float ft = 0.25;                                                          // Transition frequency
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
    int16_t *fir_y2 = (int16_t *)memalign(16, fir_buffer * sizeof(int16_t));
    for (int i = 0; i < fir_buffer; i++) {
        fir_x[i] = f_in_signal[i] * (int16_t)Q15_MAX;
    }

    free(f_in_signal);

    // FIR
    const int16_t start_pos = 0;
    const int16_t shift = 0;
    const int32_t loop_len = (int32_t)(fir_buffer / decim);                         // loop_len result must be without remainder
    fir_s16_t fir;
    esp_err_t status = dsps_fird_init_s16(&fir, s_coeffs, delay_line, fir_len, decim, start_pos, shift);
    fir_s16_t fir2;
    esp_err_t status2 = dsps_fird_init_s16(&fir2, s_coeffs, delay_line, fir_len, decim, start_pos, shift);
    error_msg_handler(&fir, status);
    error_msg_handler(&fir2, status2);

#if(dsps_fird_s16_aes3_enabled || dsps_fird_s16_arv4_enabled)
    dsps_16_array_rev(fir.coeffs, fir.coeffs_len);
#endif

    dsps_fird_s16(&fir, fir_x, fir_y, loop_len);
    dsps_fird_s16_ansi(&fir2, fir_x, fir_y2, loop_len);
    for (int i = 0 ; i < loop_len ; i++) {
        ESP_LOGD(TAG, "Data[%i] = %i vs %i, diff = %i", i, fir_y[i], fir_y2[i], fir_y[i] - fir_y2[i]);
    }

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

    // Convert the signal power and noise power from watts to dB and calculate SNR
    const float snr = 10 * log10f(signal_pow / noise_pow);
    noise_pow = 10 * log10f(noise_pow);
    signal_pow = 10 * log10f(signal_pow);

    ESP_LOGI(TAG, "\nSignal Power: \t%f\nNoise Power: \t%f\nSNR: \t\t%f", signal_pow, noise_pow, snr);
    dsps_view(fft_data, N_FFT / 2, 64, 16,  -140, 40, '|');
    free(fft_data);

    const float min_exec_snr = 50;
    const float max_exec_snr = 120;
    TEST_ASSERT_EXEC_IN_RANGE(min_exec_snr, max_exec_snr, snr);
    dsps_fird_s16_aexx_free(&fir);

}
