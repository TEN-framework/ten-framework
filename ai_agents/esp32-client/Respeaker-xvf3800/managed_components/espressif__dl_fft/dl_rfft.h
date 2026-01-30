#pragma once
#include "dl_fft.h"

#ifdef __cplusplus
extern "C" {
#endif

/**data format for in-place rfft

input:   real data, size = fft_point
output:  only one side values are returned because the real-to-complex Fourier transform satisfies the conjugate
symmetry x[0] = real part of DC component x[1] = real part of fft_point/2 component x[2] = real part of 1st component
         x[3] = image part of 1st component
         ......
         x[fft_point-2] = real part of fft_point/2-1 component
         x[fft_point-1] = image part of fft_point/2-1 component
*/

/**
 * @brief Initialize a single-precision floating-point real FFT instance
 * @param fft_point  Number of FFT points (must be power of two)
 * @param caps       Configuration flags for memory allocation, same with esp-idf heap_caps_malloc
 *                   (e.g., MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL, MALLOC_CAP_SPIRAM)
 * @return dl_fft_f32_t*  Handle to FFT instance
 */
dl_fft_f32_t *dl_rfft_f32_init(int fft_point, uint32_t caps);

/**
 * @brief Deinitialize a single-precision floating-point real FFT instance
 * @param handle  FFT instance handle created by dl_rfft_f32_init()
 */
void dl_rfft_f32_deinit(dl_fft_f32_t *handle);

/**
 * @brief Execute single-precision floating-point real FFT transform
 * @param handle  FFT instance handle
 * @param data    Input/output buffer, in-place fft calculation
 * @return esp_err_t  ESP_OK on success, error code otherwise
 */
esp_err_t dl_rfft_f32_run(dl_fft_f32_t *handle, float *data);

/**
 * @brief Execute single-precision floating-point real inverse FFT transform
 * @param handle  FFT instance handle
 * @param data    Input/output buffer, in-place fft calculation
 * @return esp_err_t  ESP_OK on success, error code otherwise
 */
esp_err_t dl_irfft_f32_run(dl_fft_f32_t *handle, float *data);

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
 * @brief Execute inverse 16-bit fixed-point FFT transform
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
 * @brief Execute inverse 16-bit fixed-point FFT with high-precision scaling
 * @param handle        FFT instance handle
 * @param data          Input/output buffer, in-place fft calculation
 * @param in_exponent   Input data exponent (2^in_exponent scaling factor)
 * @param out_exponent  Output data exponent (2^out_exponent scaling factor)
 * @return esp_err_t    ESP_OK on success, error code otherwise
 */
esp_err_t dl_ifft_s16_hp_run(dl_fft_s16_t *handle, int16_t *data, int in_exponent, int *out_exponent);

/**
 * @brief Initialize a 16-bit fixed-point real FFT instance
 * @param fft_point  Number of FFT points (must be power of two)
 * @param caps       Configuration flags for memory allocation, same with esp-idf heap_caps_malloc
 *                   (e.g., MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL, MALLOC_CAP_SPIRAM)
 * @return dl_fft_s16_t*  Handle to FFT instance
 */
dl_fft_s16_t *dl_rfft_s16_init(int fft_point, uint32_t caps);

/**
 * @brief Deinitialize a 16-bit fixed-point real FFT instance
 * @param handle  FFT instance handle created by dl_rfft_s16_init()
 */
void dl_rfft_s16_deinit(dl_fft_s16_t *handle);

/**
 * @brief Execute 16-bit fixed-point real FFT transform
 * @param handle        FFT instance handle
 * @param data          Input/output buffer, in-place fft calculation
 * @param in_exponent   Input data exponent (2^in_exponent scaling factor)
 * @param out_exponent  Output data exponent (2^out_exponent scaling factor)
 * @return esp_err_t    ESP_OK on success, error code otherwise
 */
esp_err_t dl_rfft_s16_run(dl_fft_s16_t *handle, int16_t *data, int in_exponent, int *out_exponent);

/**
 * @brief Execute 16-bit fixed-point real FFT with high-precision scaling
 * @param handle        FFT instance handle
 * @param data          Input/output buffer, in-place fft calculation
 * @param in_exponent   Input data exponent (2^in_exponent scaling factor)
 * @param out_exponent  Output data exponent (2^out_exponent scaling factor)
 * @return esp_err_t    ESP_OK on success, error code otherwise
 */
esp_err_t dl_rfft_s16_hp_run(dl_fft_s16_t *handle, int16_t *data, int in_exponent, int *out_exponent);

/**
 * @brief Execute 16-bit fixed-point real inverse FFT transform
 * @param handle        FFT instance handle
 * @param data          Input/output buffer, in-place fft calculation
 * @param in_exponent   Input data exponent (2^in_exponent scaling factor)
 * @param out_exponent  Output data exponent (2^out_exponent scaling factor)
 * @return esp_err_t    ESP_OK on success, error code otherwise
 */
esp_err_t dl_irfft_s16_run(dl_fft_s16_t *handle, int16_t *data, int in_exponent, int *out_exponent);

/**
 * @brief Execute 16-bit fixed-point real inverse FFT with high-precision scaling
 * @param handle        FFT instance handle
 * @param data          Input/output buffer, in-place fft calculation
 * @param in_exponent   Input data exponent (2^in_exponent scaling factor)
 * @param out_exponent  Output data exponent (2^out_exponent scaling factor)
 * @return esp_err_t    ESP_OK on success, error code otherwise
 */
esp_err_t dl_irfft_s16_hp_run(dl_fft_s16_t *handle, int16_t *data, int in_exponent, int *out_exponent);
#ifdef __cplusplus
}
#endif
