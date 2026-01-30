#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_IDF_TARGET_ESP32
void dl_fft2r_fc32_ae32_(float *data, int N, float *table);
void dl_ifft2r_fc32_ae32_(float *data, int N, float *table);
void dl_fft4r_fc32_ae32_(float *data, int N, float *table, int table_size);
void dl_ifft4r_fc32_ae32_(float *data, int N, float *table, int table_size);

#elif CONFIG_IDF_TARGET_ESP32S3
void dl_fft2r_fc32_aes3_(float *data, int N, float *table);
void dl_ifft2r_fc32_aes3_(float *data, int N, float *table);
void dl_fft4r_fc32_aes3_(float *data, int N, float *table, int table_size);
void dl_ifft4r_fc32_aes3_(float *data, int N, float *table, int table_size);

// void test_radix2_fft_bf_s16(int16_t *data, int16_t *table, int16_t fft_point, int16_t log2n, int16_t);
// int test_radix2_fft_bf_s16_hp(int16_t *, int16_t *, int16_t, int16_t, int16_t);
// void test_radix2_bit_reverse(int16_t *data, int16_t cpx_point, int16_t log2n);
// void test_fftr_s16(int16_t *, int16_t *, int16_t);
// void test_ffti_s16(int16_t *, int16_t *, int16_t);
// void test_radix2_ifft_bf_s16(int16_t *, int16_t *, int16_t, int16_t, int16_t);
// int test_radix2_ifft_bf_s16_hp(int16_t *, int16_t *, int16_t, int16_t, int16_t);

#elif CONFIG_IDF_TARGET_ESP32P4
void dl_fft2r_fc32_arp4_(float *data, int N, float *table);
void dl_ifft2r_fc32_arp4_(float *data, int N, float *table);
void dl_fft4r_fc32_arp4_(float *data, int N, float *table, int table_size);
void dl_ifft4r_fc32_arp4_(float *data, int N, float *table, int table_size);
#endif

#ifdef __cplusplus
}
#endif
