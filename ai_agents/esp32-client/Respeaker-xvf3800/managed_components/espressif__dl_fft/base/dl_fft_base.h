#pragma once

#include "dl_fft_dtype.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "dl_fft_platform.h"

// common function
bool dl_is_power_of_two(int x);
int dl_power_of_two(uint32_t n);
float *dl_short_to_float(const int16_t *x, int len, int exponent, float *y);
int16_t dl_array_max_q_s16(const int16_t *x, int size);
int dl_float_to_short(const float *x, int len, int16_t *y, int out_exponent);

// float fftr2
float *dl_gen_fftr2_table_f32(int fft_point, uint32_t caps);
uint16_t *dl_gen_bitrev2r_table(int N, uint32_t caps, int *bitrev_size);

esp_err_t dl_fft2r_fc32_ansi(float *data, int N, float *w);
esp_err_t dl_ifft2r_fc32_ansi(float *data, int N, float *w);

esp_err_t dl_bitrev2r_fc32_ansi(float *data, int N, uint16_t *reverse_tab, int bitrev_size);

// float fftr4
float *dl_gen_rfft_table_f32(int fft_point, uint32_t caps);
float *dl_gen_fft4r_table_f32(int fft_point, uint32_t caps);
uint16_t *dl_gen_bitrev4r_table(int N, uint32_t caps, int *bitrev_size);

esp_err_t dl_fft4r_fc32_ansi(float *data, int length, float *table, int table_size);
esp_err_t dl_ifft4r_fc32_ansi(float *data, int length, float *table, int table_size);
esp_err_t dl_bitrev4r_fc32_ansi(float *data, int N, uint16_t *reverse_tab, int bitrev_size);
esp_err_t dl_rfft_post_proc_fc32_ansi(float *data, int N, float *table);
esp_err_t dl_rfft_pre_proc_fc32_ansi(float *data, int N, float *table);

// int16 fft and rfft
int16_t *dl_gen_fft_table_sc16(int fft_point, uint32_t caps);
int16_t *dl_gen_rfft_table_s16(int fft_point, uint32_t caps);

esp_err_t dl_fft2r_sc16_hp_ansi(int16_t *data, int N, int16_t *table, int *shift);
esp_err_t dl_fft2r_sc16_ansi(int16_t *data, int N, int16_t *table);

esp_err_t dl_ifft2r_sc16_hp_ansi(int16_t *data, int N, int16_t *table, int *shift);
esp_err_t dl_ifft2r_sc16_ansi(int16_t *data, int N, int16_t *table);

esp_err_t dl_bitrev2r_sc16_ansi(int16_t *data, int N);
esp_err_t dl_rfft_post_proc_sc16_ansi(int16_t *data, int N, int16_t *table);
esp_err_t dl_rfft_pre_proc_sc16_ansi(int16_t *data, int N, int16_t *table);
esp_err_t dl_cplx2real_sc16_hp_ansi(int16_t *data, int N, int16_t *table, int *shift);

#if CONFIG_IDF_TARGET_ESP32
#define dl_fft2r_fc32 dl_fft2r_fc32_ae32_
#define dl_ifft2r_fc32 dl_ifft2r_fc32_ae32_
#define dl_fft4r_fc32 dl_fft4r_fc32_ae32_
#define dl_ifft4r_fc32 dl_ifft4r_fc32_ae32_
#elif CONFIG_IDF_TARGET_ESP32S3
#define dl_fft2r_fc32 dl_fft2r_fc32_aes3_
#define dl_ifft2r_fc32 dl_ifft2r_fc32_aes3_
#define dl_fft4r_fc32 dl_fft4r_fc32_aes3_
#define dl_ifft4r_fc32 dl_ifft4r_fc32_aes3_
#elif CONFIG_IDF_TARGET_ESP32P4
#define dl_fft2r_fc32 dl_fft2r_fc32_arp4_
#define dl_ifft2r_fc32 dl_ifft2r_fc32_arp4_
#define dl_fft4r_fc32 dl_fft4r_fc32_arp4_
#define dl_ifft4r_fc32 dl_ifft4r_fc32_arp4_
#else
#define dl_fft2r_fc32 dl_fft2r_fc32_ansi
#define dl_ifft2r_fc32 dl_ifft2r_fc32_ansi
#define dl_fft4r_fc32 dl_fft4r_fc32_ansi
#define dl_ifft4r_fc32 dl_ifft4r_fc32_ansi
#endif

#define dl_fft2r_sc16 dl_fft2r_sc16_ansi
#define dl_fft2r_sc16_hp dl_fft2r_sc16_hp_ansi
#define dl_ifft2r_sc16 dl_ifft2r_sc16_ansi
#define dl_ifft2r_sc16_hp dl_ifft2r_sc16_hp_ansi

#ifdef __cplusplus
}
#endif
