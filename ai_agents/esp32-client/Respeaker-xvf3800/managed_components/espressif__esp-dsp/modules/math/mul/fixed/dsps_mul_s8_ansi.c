/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsps_mul.h"

esp_err_t dsps_mul_s8_ansi(const int8_t *input1, const int8_t *input2, int8_t *output, int len, int step1, int step2, int step_out, int shift)
{
    if (NULL == input1) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (NULL == input2) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (NULL == output) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

    for (int i = 0 ; i < len ; i++) {
        int32_t acc = (int32_t)input1[i * step1] * (int32_t)input2[i * step2];
        output[i * step_out] = acc >> shift;
    }
    return ESP_OK;
}
