/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsps_fir.h"
#include "malloc.h"
#include <string.h>
#include "dsp_tests.h"

#define ROUNDING_VALUE  0x7fff

esp_err_t dsps_fird_init_s16(fir_s16_t *fir, int16_t *coeffs, int16_t *delay, int16_t coeffs_len, int16_t decim, int16_t start_pos, int16_t shift)
{
    fir->coeffs = coeffs;
    fir->delay = delay;
    fir->coeffs_len = coeffs_len;
    fir->pos = 0;
    fir->decim = decim;
    fir->d_pos = start_pos;
    fir->shift = shift;
    fir->rounding_val = (int16_t)(ROUNDING_VALUE);
    fir->free_status = 0;

    if (fir->coeffs_len < 2) {                                          // number of coeffcients must be higer than 1
        return ESP_ERR_DSP_INVALID_LENGTH;
    }

    if ((fir->shift > 40) || (fir->shift < -40)) {                      // shift amount must be within a range from -40 to 40
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

    if (fir->d_pos >= fir->decim) {                                     // start position must be lower than decimation
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

#if CONFIG_DSP_OPTIMIZED

    // Rounding value buffer primary for a purpose of ee.ld.accx.ip, but used for both the esp32 and esp32s3
    // dsps_fird_s16_aexx_free() must be called to free the memory after the FIR function is finished
    int32_t *aexx_rounding_buff = (int32_t *)memalign(16, 2 * sizeof(int32_t));

    long long rounding = (long long)(fir->rounding_val);

    if (fir->shift >= 0) {
        rounding = (rounding >> fir->shift);
    } else {
        rounding = (rounding << (-fir->shift));
    }
#if dsps_fird_s16_arp4_enabled
    fir->pos = start_pos;

    int16_t *new_delay_buff = (int16_t *)memalign(16, (coeffs_len + 8 * 2) * sizeof(int16_t));
    for (int i = 0 ; i < (coeffs_len + 8 * 2) ; i++) {
        new_delay_buff[i] = 0;
    }
    fir->delay = &new_delay_buff[8];
    fir->free_status |= 0x0001;

#endif // dsps_fird_s16_arp4_enabled


    aexx_rounding_buff[0] = (int32_t)(rounding);                        // 32 lower bits (acclo) type reassignment to 32-bit
    aexx_rounding_buff[1] = (int32_t)((rounding >> 32) & 0xFF);         // 8 higher bits (acchi) shift by 32 and apply the mask
    fir->rounding_buff = aexx_rounding_buff;
    fir->free_status |= 0x0004;

#if dsps_fird_s16_aes3_enabled

    if (fir->delay == NULL) {                                   // New delay buffer is allocated if the current delay line is NULL
        int16_t *new_delay_buff = (int16_t *)memalign(16, coeffs_len * sizeof(int16_t));
        fir->delay = new_delay_buff;
        fir->free_status |= 0x0001;
    } else {
        if ((int)fir->delay & 0xf) {                            // Delay line array must be aligned
            return ESP_ERR_DSP_ARRAY_NOT_ALIGNED;
        }
    }

    if ((int)fir->coeffs & 0xf) {                               // Coefficients array must be aligned
        return ESP_ERR_DSP_ARRAY_NOT_ALIGNED;
    }

    // If the number of coefficients is not divisible by 8, a new delay line a new coefficients arrays are allocated
    // the newly allocated arrays are divisible by 8. Coefficients are copied from the original fir structure to
    // the new coeffs array and the remaining space is filled with zeroes
    // dsps_fird_s16_free_coeffs_delay must be called to free the memory after the FIR function is finished
    if (fir->coeffs_len % 8) {                                           // Number of coefficients must be devisible by 8
        int16_t zero_coeffs = (8 - (fir->coeffs_len % 8));
        int16_t new_coeffs_len = fir->coeffs_len + zero_coeffs;
        int16_t *aes3_delay_buff = (int16_t *)memalign(16, new_coeffs_len * sizeof(int16_t));
        int16_t *aes3_coeffs_buff = (int16_t *)memalign(16, new_coeffs_len * sizeof(int16_t));

        for (int i = 0; i < fir->coeffs_len; i++) {                      // copy fir->coeffs to aes3_coeffs_buff
            aes3_coeffs_buff[i] = fir->coeffs[i];
        }

        for (int i = fir->coeffs_len; i < new_coeffs_len; i++) {                  // add zeroes to the end
            aes3_coeffs_buff[i] = 0;
        }

        fir->delay = aes3_delay_buff;
        fir->coeffs = aes3_coeffs_buff;
        fir->coeffs_len = new_coeffs_len;
        fir->free_status |= 0x0002;
    }

#endif      // dsps_fird_s16_aes3_enabled
#endif      // CONFIG_DSP_OPTIMIZED

    for (int i = 0; i < fir->coeffs_len; i++) {                                  // Initialize the dealy line to zero
        fir->delay[i] = 0;
    }

    return ESP_OK;
}

esp_err_t dsps_fird_s16_aexx_free(fir_s16_t *fir)
{

    if (fir->free_status == 0) {
        return ESP_OK;
    }

    if (fir->free_status & 0x0003) {

        if (fir->free_status & 0x0002) {
            free(fir->coeffs);
        }
#if dsps_fird_s16_arp4_enabled
        fir->delay = &fir->delay[-8];
#endif
        free(fir->delay);
    }

    if (fir->free_status & 0x0004) {
        free(fir->rounding_buff);
    }
    fir->free_status = 0;

    return ESP_OK;
}


esp_err_t dsps_16_array_rev(int16_t *arr, int16_t len)
{

    int16_t temp;

    for (int i = 0; i < (int)(len / 2); i++) {
        temp = arr[i];
        arr[i] = arr[len - 1 - i];
        arr[len - 1 - i] = temp;
    }
    return ESP_OK;
}
