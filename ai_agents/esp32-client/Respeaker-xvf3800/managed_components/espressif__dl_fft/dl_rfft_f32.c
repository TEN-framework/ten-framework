#include "dl_rfft.h"
#include "esp_attr.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "dl rfft";

dl_fft_f32_t *dl_rfft_f32_init(int fft_point, uint32_t caps)
{
    if (!dl_is_power_of_two(fft_point)) {
        ESP_LOGE(TAG, "FFT point must be power of two");
        return NULL;
    }

    dl_fft_f32_t *handle = (dl_fft_f32_t *)heap_caps_malloc(sizeof(dl_fft_f32_t), caps);
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate FFT handle");
        return NULL;
    }
    handle->fft_table = NULL;
    handle->rfft_table = NULL;
    handle->bitrev_table = NULL;
    handle->fft_point = fft_point;
    handle->log2n = dl_power_of_two(fft_point);

    // rfft table
    handle->rfft_table = dl_gen_rfft_table_f32(fft_point, caps);
    if (!handle->rfft_table) {
        ESP_LOGE(TAG, "Failed to generate FFT table");
        dl_rfft_f32_deinit(handle);
        return NULL;
    }

    if (handle->log2n % 2 == 1) {
        handle->bitrev_table = dl_gen_bitrev4r_table(fft_point, caps, &handle->bitrev_size);
        handle->fft_table = dl_gen_fft4r_table_f32(fft_point, caps);
        if (!handle->fft_table) {
            ESP_LOGE(TAG, "Failed to generate FFT table");
            dl_rfft_f32_deinit(handle);
            return NULL;
        }
    } else {
        handle->bitrev_table = dl_gen_bitrev2r_table(fft_point >> 1, caps, &handle->bitrev_size);
        handle->fft_table = dl_gen_fftr2_table_f32(fft_point >> 1, caps);
        if (!handle->fft_table) {
            ESP_LOGE(TAG, "Failed to generate FFT table");
            dl_rfft_f32_deinit(handle);
            return NULL;
        }
    }

    return handle;
}

void dl_rfft_f32_deinit(dl_fft_f32_t *handle)
{
    if (handle) {
        if (handle->fft_table) {
            free(handle->fft_table);
        }
        if (handle->rfft_table) {
            free(handle->rfft_table);
        }
        if (handle->bitrev_table) {
            free(handle->bitrev_table);
        }
        free(handle);
    }
}
esp_err_t dl_rfft_f32_run(dl_fft_f32_t *handle, float *data)
{
    if (!handle || !data) {
        return ESP_FAIL;
    }
    int fft_point = handle->fft_point;
    float *fft_table = handle->fft_table;
    float *rfft_table = handle->rfft_table;

    if (handle->log2n % 2 == 1) {
        dl_fft4r_fc32(data, fft_point >> 1, fft_table, fft_point);
        dl_bitrev4r_fc32_ansi(data, fft_point >> 1, handle->bitrev_table, handle->bitrev_size);
    } else {
        dl_fft2r_fc32(data, fft_point >> 1, fft_table);
        dl_bitrev2r_fc32_ansi(data, fft_point >> 1, handle->bitrev_table, handle->bitrev_size);
    }

    // Convert one complex vector with length N/2 to one real spectrum vector with length N/2
    dl_rfft_post_proc_fc32_ansi(data, fft_point >> 1, rfft_table);

    return ESP_OK;
}

esp_err_t dl_irfft_f32_run(dl_fft_f32_t *handle, float *data)
{
    if (!handle || !data) {
        return ESP_FAIL;
    }
    int fft_point = handle->fft_point;
    float *fft_table = handle->fft_table;
    float *rfft_table = handle->rfft_table;
    float scale = 2.0 / fft_point;

    dl_rfft_pre_proc_fc32_ansi(data, fft_point >> 1, rfft_table);

    if (handle->log2n % 2 == 1) {
        dl_ifft4r_fc32(data, fft_point >> 1, fft_table, fft_point);
        dl_bitrev4r_fc32_ansi(data, fft_point >> 1, handle->bitrev_table, handle->bitrev_size);
    } else {
        dl_ifft2r_fc32(data, fft_point >> 1, fft_table);
        dl_bitrev2r_fc32_ansi(data, fft_point >> 1, handle->bitrev_table, handle->bitrev_size);
    }

    // Scale by 1/N
    for (int i = 0; i < fft_point; i++) {
        data[i] *= scale;
    }

    return ESP_OK;
}
