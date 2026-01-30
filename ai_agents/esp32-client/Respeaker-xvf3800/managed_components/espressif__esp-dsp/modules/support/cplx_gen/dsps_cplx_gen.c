/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsps_cplx_gen.h"

esp_err_t dsps_cplx_gen_ansi(cplx_sig_t *cplx_gen, void *output, int32_t len)
{
    // angle frequency is already cplx_gen->freq
    const int sin_to_cos = cplx_gen->lut_len / 4;
    float ph = cplx_gen->phase;
    const float fr = cplx_gen->freq;
    int sin_pos, cos_pos;

    for (int i = 0 ; i < len; i++) {

        if (ph < 0) {
            ph += 1.0;
        }
        if (ph >= 1.0) {
            ph -= 1.0;
        }

        sin_pos = (int)(ph * (cplx_gen->lut_len));
        cos_pos = (sin_pos + sin_to_cos) & (cplx_gen->lut_len - 1);

        if (cplx_gen->d_type == S16_FIXED) {
            ((int16_t *)output)[i * 2 + 0] = ((int16_t *)cplx_gen->lut)[cos_pos];
            ((int16_t *)output)[i * 2 + 1] = ((int16_t *)cplx_gen->lut)[sin_pos];
        } else {
            ((float *)output)[i * 2 + 0] = ((float *)cplx_gen->lut)[cos_pos];
            ((float *)output)[i * 2 + 1] = ((float *)cplx_gen->lut)[sin_pos];
        }
        ph += fr;
    }

    return ESP_OK;
}
