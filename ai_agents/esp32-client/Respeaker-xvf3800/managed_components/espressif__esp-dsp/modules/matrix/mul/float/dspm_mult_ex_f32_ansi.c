/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dspm_mult.h"

// Matrix A(m,n), m - amount or rows, n - amount of columns
// C(m,k) = A(m,n)*B(n,k)
// c(i * c_step,j) = sum(a(i * a_step,s)*b(s * b_step,j)) , s=1..n
esp_err_t dspm_mult_ex_f32_ansi(const float *A, const float *B, float *C, int A_rows, int A_cols, int B_cols, int A_padding, int B_padding, int C_padding)
{
    if (NULL == A) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (NULL == B) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (NULL == C) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

    if (A_rows <= 0) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (A_cols <= 0) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (B_cols <= 0) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

    if (A_padding < 0) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (B_padding < 0) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (C_padding < 0) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

    const int A_step = A_cols + A_padding;
    const int B_step = B_cols + B_padding;
    const int C_step = B_cols + C_padding;

    for (int i = 0; i < A_rows; i++) {
        for (int j = 0; j < B_cols; j++) {
            C[i * C_step + j] = A[i * A_step] * B[j];
            for (int s = 1; s < A_cols; s++) {
                C[i * C_step + j] += A[i * A_step + s] * B[s * B_step + j];
            }
        }
    }
    return ESP_OK;
}
