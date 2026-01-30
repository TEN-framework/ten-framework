#pragma once
#include "dl_fft_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Single-precision floating-point FFT instance structure
 * @param fft_point  Number of FFT points
 * @param log2n      Log base 2 of FFT points
 * @param fft_table  FFT real to complex coefficient table
 * @param rfft_table   FFT complex to real coefficient table
 */
typedef struct {
    int fft_point;
    int log2n;
    float *fft_table;
    float *rfft_table;
    uint16_t *bitrev_table;
    int bitrev_size;
} dl_fft_f32_t;

/**
 * @brief 16-bit fixed-point FFT instance structure
 * @param fft_point  Number of FFT points
 * @param log2n      Log base 2 of FFT points
 * @param fft_table  FFT real to complex coefficient table
 * @param rfft_table   FFT complex to real coefficient table
 */
typedef struct {
    int fft_point;
    int log2n;
    int16_t *fft_table;
    int16_t *rfft_table;
} dl_fft_s16_t;

/**
 * @brief Initialize a single-precision floating-point FFT instance
 * @param fft_point  Number of FFT points (must be power of two)
 * @param caps       Configuration flags for memory allocation, same with esp-idf heap_caps_malloc
 *                   (e.g., MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL, MALLOC_CAP_SPIRAM)
 * @return dl_fft_f32_t*  Handle to FFT instance
 */
dl_fft_f32_t *dl_fft_f32_init(int fft_point, uint32_t caps);

/**
 * @brief Deinitialize a single-precision floating-point FFT instance
 * @param handle  FFT instance handle created by dl_fft_f32_init()
 */
void dl_fft_f32_deinit(dl_fft_f32_t *handle);

/**
 * @brief Execute single-precision floating-point FFT transform
 * @param handle  FFT instance handle
 * @param data    Input/output buffer, in-place fft calculation
 * @return esp_err_t  ESP_OK on success, error code otherwise
 */
esp_err_t dl_fft_f32_run(dl_fft_f32_t *handle, float *data);

/**
 * @brief Execute single-precision floating-point inverse FFT transform
 * @param handle  FFT instance handle
 * @param data    Input/output buffer, in-place ifft calculation
 * @return esp_err_t  ESP_OK on success, error code otherwise
 */
esp_err_t dl_ifft_f32_run(dl_fft_f32_t *handle, float *data);

/**
 * @brief Initialize a 16-bit fixed-point FFT instance
 * @param fft_point  Number of FFT points (must be power of two)
 * @param caps       Configuration flags for memory allocation, same with esp-idf heap_caps_malloc
 *                   (e.g., MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL, MALLOC_CAP_SPIRAM)
 * @return dl_fft_s16_t*  Handle to FFT instance
 */
dl_fft_s16_t *dl_fft_s16_init(int fft_point, uint32_t caps);

/**
 * @brief Deinitialize a 16-bit fixed-point FFT instance
 * @param handle  FFT instance handle created by dl_fft_s16_init()
 */
void dl_fft_s16_deinit(dl_fft_s16_t *handle);

/**
 * @brief Execute 16-bit fixed-point FFT transform
 * @param handle        FFT instance handle
 * @param data          Input/output buffer, in-place fft calculation
 * @param in_exponent   Input data exponent (2^in_exponent scaling factor)
 * @param out_exponent  Output data exponent (2^out_exponent scaling factor)
 * @return esp_err_t    ESP_OK on success, error code otherwise
 */
esp_err_t dl_fft_s16_run(dl_fft_s16_t *handle, int16_t *data, int in_exponent, int *out_exponent);

/**
 * @brief Execute 16-bit fixed-point inverse FFT transform
 * @param handle        FFT instance handle
 * @param data          Input/output buffer, in-place fft calculation
 * @param in_exponent   Input data exponent (2^in_exponent scaling factor)
 * @param out_exponent  Output data exponent (2^out_exponent scaling factor)
 * @return esp_err_t    ESP_OK on success, error code otherwise
 */
esp_err_t dl_ifft_s16_run(dl_fft_s16_t *handle, int16_t *data, int in_exponent, int *out_exponent);

/**
 * @brief Execute 16-bit fixed-point FFT with high-precision scaling
 * @param handle        FFT instance handle
 * @param data          Input/output buffer, in-place fft calculation
 * @param in_exponent   Input data exponent (2^in_exponent scaling factor)
 * @param out_exponent  Output data exponent (2^out_exponent scaling factor)
 * @return esp_err_t    ESP_OK on success, error code otherwise
 */
esp_err_t dl_fft_s16_hp_run(dl_fft_s16_t *handle, int16_t *data, int in_exponent, int *out_exponent);

/**
 * @brief Execute 16-bit fixed-point inverse FFT with high-precision scaling
 * @param handle        FFT instance handle
 * @param data          Input/output buffer, in-place fft calculation
 * @param in_exponent   Input data exponent (2^in_exponent scaling factor)
 * @param out_exponent  Output data exponent (2^out_exponent scaling factor)
 * @return esp_err_t    ESP_OK on success, error code otherwise
 */
esp_err_t dl_ifft_s16_hp_run(dl_fft_s16_t *handle, int16_t *data, int in_exponent, int *out_exponent);

#ifdef __cplusplus
}
#endif
