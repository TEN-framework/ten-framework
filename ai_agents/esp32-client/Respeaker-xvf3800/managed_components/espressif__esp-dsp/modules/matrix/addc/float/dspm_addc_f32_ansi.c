/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dspm_addc.h"

esp_err_t dspm_addc_f32_ansi(const float *input, float *output, float C, int rows, int cols, int padd_in, int padd_out, int step_in, int step_out)
{
    if (NULL == input) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (NULL == output) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

    if (rows <= 0) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (cols <= 0) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

    if (padd_in < 0) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (padd_out < 0) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

    if (step_in <= 0) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (step_out <= 0) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

    const int ptr_step_in = cols + padd_in;
    const int ptr_step_out = cols + padd_out;
    float *ptr_input = (float *)input;

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            output[col * step_out] = ptr_input[col * step_in] + C;
        }
        output += ptr_step_out;
        ptr_input += ptr_step_in;
    }
    return ESP_OK;
}
