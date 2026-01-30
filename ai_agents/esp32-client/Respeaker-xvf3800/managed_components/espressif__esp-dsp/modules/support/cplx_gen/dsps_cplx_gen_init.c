/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "dsps_cplx_gen.h"
#include "dsp_common.h"
#include "esp_log.h"
#include <math.h>
#include <malloc.h>

#define Q15_MAX INT16_MAX

static const char *TAG = "dsps_cplx_gen";

esp_err_t dsps_cplx_gen_init(cplx_sig_t *cplx_gen, out_d_type d_type, void *lut, int32_t lut_len, float freq, float initial_phase)
{
    cplx_gen->lut_len = lut_len;
    cplx_gen->freq = freq;
    cplx_gen->lut = lut;
    cplx_gen->free_status = 0;
    cplx_gen->d_type = d_type;
    cplx_gen->phase = initial_phase;

    // length of the LUT must be power of 2
    if (!dsp_is_power_of_two(lut_len)) {
        ESP_LOGE(TAG, "The length of the LUT must be power of 2");
        return ESP_ERR_DSP_INVALID_LENGTH;
    }

    // LUT length must be in a range from 256 to 8192
    if ((lut == NULL) && ((cplx_gen->lut_len > 8192) || (cplx_gen->lut_len < 256))) {
        ESP_LOGE(TAG, "The length of the LUT table out of range. Valid range is 256 to 8192");
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

    // frequency is a Nyquist frequency, must be in a range from (-1 to 1)
    if ((cplx_gen->freq >= 1) || (cplx_gen->freq <= -1)) {
        ESP_LOGE(TAG, "The frequency is out of range. Valid range is +/- 1. ");
        return ESP_ERR_DSP_INVALID_PARAM;
    }

    // initial phase in a range from (-1 to 1)
    if ((cplx_gen->phase >= 1) || (cplx_gen->phase <= -1)) {
        ESP_LOGE(TAG, "The phase is out of range. Valid range is +/- 1. ");
        return ESP_ERR_DSP_INVALID_PARAM;
    }

    // LUT table coefficients generation
    if (lut == NULL) {                      // lut has not been provided by an user. Allocate and initialize it
        cplx_gen->free_status |= 0x0001;    // lut has been allocated, free_status indicates that the space must be freed afterwards

        if (cplx_gen->d_type == S16_FIXED) {                    // Q15 fixed point
            int16_t *local_lut = (int16_t *)malloc(cplx_gen->lut_len * sizeof(int16_t));

            float term;
            for (int i = 0 ; i < cplx_gen->lut_len; i++) {
                term = (2.0 * M_PI) * ((float)(i) / (float)(cplx_gen->lut_len));
                local_lut[i] = (int16_t)(sin(term) * Q15_MAX);  // conversion to Q15 fixed point
            }
            cplx_gen->lut = (void *)local_lut;
        } else if (cplx_gen->d_type == F32_FLOAT) {             // Single precision floating point
            float *local_lut = (float *)malloc(cplx_gen->lut_len * sizeof(float));

            float term;
            for (int i = 0 ; i < cplx_gen->lut_len; i++) {
                term = (2.0 * M_PI) * ((float)(i) / (float)(cplx_gen->lut_len));
                local_lut[i] = (float)sin(term);
            }
            cplx_gen->lut = (void *)local_lut;
        } else {
            cplx_gen->lut = NULL;
            return ESP_ERR_DSP_INVALID_PARAM;
        }
    }
    return ESP_OK;
}

esp_err_t dsps_cplx_gen_freq_set(cplx_sig_t *cplx_gen, float freq)
{
    if ((freq >= 1) || (freq <= -1)) {          // frequency is a Nyquist frequency, must be in a range from (-1 to 1)
        ESP_LOGE(TAG, "The frequency is out of range. Valid range is +/- 1. ");
        return ESP_ERR_DSP_INVALID_PARAM;
    }

    cplx_gen->freq = freq;
    return ESP_OK;
}

float dsps_cplx_gen_freq_get(cplx_sig_t *cplx_gen)
{
    // Check if the structure was initialized
    if (!dsp_is_power_of_two(cplx_gen->lut_len)) {
        ESP_LOGE(TAG, "cplx_gen strucure was not initialized");
        return -2;
    }

    return (cplx_gen->freq);
}

esp_err_t dsps_cplx_gen_phase_set(cplx_sig_t *cplx_gen, float phase)
{
    if ((phase >= 1) || (phase <= -1)) {        // initial phase in a range from (-1 to 1)
        ESP_LOGE(TAG, "The phase is out of range. Valid range is +/- 1. ");
        return ESP_ERR_DSP_INVALID_PARAM;
    }

    cplx_gen->phase = phase;
    return ESP_OK;
}

float dsps_cplx_gen_phase_get(cplx_sig_t *cplx_gen)
{
    // Check if the structure was initialized
    if (!dsp_is_power_of_two(cplx_gen->lut_len)) {
        ESP_LOGE(TAG, "cplx_gen strucure was not initialized");
        return -2;
    }

    return (cplx_gen->phase);
}

esp_err_t dsps_cplx_gen_set(cplx_sig_t *cplx_gen, float freq, float phase)
{
    if ((freq >= 1) || (freq <= -1)) {          // frequency is a Nyquist frequency, must be in a range from (-1 to 1)
        ESP_LOGE(TAG, "The frequency is out of range. Valid range is +/- 1. ");
        return ESP_ERR_DSP_INVALID_PARAM;
    }

    if ((phase >= 1) || (phase <= -1)) {        // phase in a range from (-1 to 1)
        ESP_LOGE(TAG, "The phase is out of range. Valid range is +/- 1. ");
        return ESP_ERR_DSP_INVALID_PARAM;
    }

    cplx_gen->phase = phase;
    cplx_gen->freq = freq;
    return ESP_OK;
}

void cplx_gen_free(cplx_sig_t *cplx_gen)
{
    if (cplx_gen->free_status & 0x0001) {
        free(cplx_gen->lut);
        cplx_gen->free_status = 0;
    }
}
