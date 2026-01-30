/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "unity.h"
#include "dsp_platform.h"
#include "esp_log.h"
#include <malloc.h>

#include "dsps_add.h"
#include "esp_attr.h"
#include "dsp_tests.h"

#if (dsps_add_s16_aes3_enabled == 1)

static const char *TAG = "dsps_add";
TEST_CASE("dsps_add_s8_aes3 functionality", "[dsps]")
{
    int n = 64;
    int8_t *x = (int8_t *)memalign(16, n * sizeof(int8_t));
    int8_t *y = (int8_t *)memalign(16, n * sizeof(int8_t));
    int32_t temp;
    int shift = 0;
    for (int i = 0 ; i < n ; i++) {
        x[i] = i - n / 2;
        temp = ((int32_t)x[i] + (int32_t)x[i]) >> shift;
        y[i] = temp;
    }

    dsps_add_s8_aes3(x, x, x, n, 1, 1, 1, shift);
    for (int i = 0 ; i < n ; i++) {
        ESP_LOGD(TAG, "x[%i] = %i  %i", i, x[i], y[i]);
        if (x[i] != y[i]) {
            TEST_ASSERT_EQUAL(x[i], y[i]);
        }
    }
    free(x);
    free(y);
}

TEST_CASE("dsps_add_s8_aes3 benchmark", "[dsps]")
{
    const int n = 2048;
    int8_t *x = (int8_t *)memalign(16, n * sizeof(int8_t));
    for (int i = 0 ; i < n ; i++) {
        x[i] = i << 4;
    }

    unsigned int start_b = dsp_get_cpu_cycle_count();
    dsps_add_s8_aes3(x, x, x, n, 1, 1, 1, 0);
    unsigned int end_b = dsp_get_cpu_cycle_count();

    float cycles = end_b - start_b;
    ESP_LOGI(TAG, "dsps_add_s8_aes3 - %f cycles per sample \n", cycles);
    free(x);

}
#endif // (dsps_add_s16_aes3_enabled == 1)
