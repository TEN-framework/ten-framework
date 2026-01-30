#include "dl_fft.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "dl fft";

// Create a new FFT handle
dl_fft_s16_t *dl_fft_s16_init(int fft_point, uint32_t caps)
{
    if (!dl_is_power_of_two(fft_point)) {
        ESP_LOGE(TAG, "FFT point must be power of two");
        return NULL;
    }

    dl_fft_s16_t *handle = (dl_fft_s16_t *)heap_caps_malloc(sizeof(dl_fft_s16_t), caps);
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate FFT handle");
        return NULL;
    }
    handle->fft_table = NULL;
    handle->rfft_table = NULL;
    handle->fft_point = fft_point;
    handle->log2n = dl_power_of_two(fft_point);

    // Allocate and generate FFT table
    handle->fft_table = dl_gen_fft_table_sc16(fft_point, caps);
    if (!handle->fft_table) {
        ESP_LOGE(TAG, "Failed to generate FFT table");
        dl_fft_s16_deinit(handle);
        return NULL;
    }

    return handle;
}

// Free FFT handle
void dl_fft_s16_deinit(dl_fft_s16_t *handle)
{
    if (handle) {
        if (handle->fft_table) {
            free(handle->fft_table);
        }
        free(handle);
    }
}

// Perform FFT
esp_err_t dl_fft_s16_run(dl_fft_s16_t *handle, int16_t *data, int in_exponent, int *out_exponent)
{
    if (!handle || !data) {
        return ESP_FAIL;
    }

    int fft_point = handle->fft_point;
    dl_fft2r_sc16(data, fft_point, handle->fft_table);
    dl_bitrev2r_sc16_ansi(data, fft_point);
    out_exponent[0] = in_exponent + handle->log2n;

    return ESP_OK;
}

esp_err_t dl_ifft_s16_run(dl_fft_s16_t *handle, int16_t *data, int in_exponent, int *out_exponent)
{
    if (!handle || !data) {
        return ESP_FAIL;
    }

    int fft_point = handle->fft_point;
    dl_ifft2r_sc16(data, fft_point, handle->fft_table);
    dl_bitrev2r_sc16_ansi(data, fft_point);

    out_exponent[0] = in_exponent;

    return ESP_OK;
}

esp_err_t dl_fft_s16_hp_run(dl_fft_s16_t *handle, int16_t *data, int in_exponent, int *out_exponent)
{
    if (!handle || !data) {
        return ESP_FAIL;
    }

    int fft_point = handle->fft_point;
    out_exponent[0] = 0;
    dl_fft2r_sc16_hp(data, fft_point, handle->fft_table, out_exponent);
    dl_bitrev2r_sc16_ansi(data, fft_point);
    out_exponent[0] = in_exponent + out_exponent[0];

    return ESP_OK;
}

esp_err_t dl_ifft_s16_hp_run(dl_fft_s16_t *handle, int16_t *data, int in_exponent, int *out_exponent)
{
    if (!handle || !data) {
        return ESP_FAIL;
    }

    int fft_point = handle->fft_point;
    out_exponent[0] = 0;
    dl_ifft2r_sc16_hp(data, fft_point, handle->fft_table, out_exponent);
    dl_bitrev2r_sc16_ansi(data, fft_point);
    out_exponent[0] = in_exponent + out_exponent[0] - handle->log2n;

    return ESP_OK;
}
