# DL_FFT

DL_FFT is a lightweight FFT library supporting both float32 and int16 data types.

The float FFT implementation is come from esp-dsp. And we further optimized the int16 FFT to achieving better precision.
For int16 FFT, we recommend to use `dl_fft_s16_hp_run` or `dl_rfft_s16_hp_run` interface. `hp` means "high precision".

## Get Started


### C interface
```

#include "dl_fft.h"
#include "dl_rfft.h"

// float fft
float  x[nfft*2];

float *x = (float *)heap_caps_aligned_alloc(16, nfft * sizeof(float) *2, MALLOC_CAP_8BIT);
dl_fft_f32_t *fft_handle = dl_fft_f32_init(nfft, MALLOC_CAP_8BIT);
dl_fft_f32_run(fft_handle, x);
dl_ifft_f32_run(fft_handle, x);
dl_fft_f32_deinit(fft_handle);

// float rfft
float *x = (float *)heap_caps_aligned_alloc(16, nfft * sizeof(float), MALLOC_CAP_8BIT);
dl_fft_f32_t *fft_handle = dl_rfft_f32_init(nfft, MALLOC_CAP_8BIT);
dl_rfft_f32_run(fft_handle, x);
dl_irfft_f32_run(fft_handle, x);
dl_rfft_f32_deinit(fft_handle);

// int16 fft
int16_t *x= (float *)heap_caps_aligned_alloc(16, nfft * sizeof(int16_t) * 2, MALLOC_CAP_8BIT);
float *y = (float *)heap_caps_aligned_alloc(16, nfft * sizeof(float) *2, MALLOC_CAP_8BIT);
int in_exponent = -15;  //  float y = x * 2^in_exponent;
int fft_exponent;
int ifft_exponent;
dl_fft_s16_t *fft_handle = dl_fft_s16_init(nfft, MALLOC_CAP_8BIT);
dl_fft_s16_hp_run(fft_handle, x, in_exponent, &fft_exponent);
dl_fft_s16_hp_run(fft_handle, x, fft_exponent, &ifft_exponent);
dl_short_to_float(x, nfft, ifft_exponent, y); // convert output from int16_t to float
dl_fft_s16_deinit(fft_handle);

// int16 rfft
int16_t *x= (float *)heap_caps_aligned_alloc(16, nfft * sizeof(int16_t), MALLOC_CAP_8BIT);
float *y = (float *)heap_caps_aligned_alloc(16, nfft * sizeof(float), MALLOC_CAP_8BIT);
int in_exponent = -15;  //  float y = x * 2^in_exponent;
int fft_exponent;
int ifft_exponent;
dl_fft_s16_t *fft_handle = dl_rfft_s16_init(nfft, MALLOC_CAP_8BIT);
dl_rfft_s16_hp_run(fft_handle, x, in_exponent, &fft_exponent);
dl_rfft_s16_hp_run(fft_handle, x, fft_exponent, &ifft_exponent);
dl_short_to_float(x, nfft, ifft_exponent, y); // convert output from int16_t to float
dl_rfft_s16_deinit(fft_handle);


```
Please refer to [dl_fft.h](./dl_fft.h) and [dl_rfft.h](./dl_rfft.h) for more details. 
> Note: The input array x must be allocated with heap_caps_aligned_alloc and aligned to 16 bytes.


## FAQ:

#### 1. Why not just use esp-dsp directly? 

Because esp-dsp uses global variables to share FFT tables and other parameters in order to minimize memory consumption. This introduces significant risks for independent components. Your FFT results might be corrupted by other programs, and this is something you have little control over.  

#### 2. What does dl_fft do?

1. Provides an unified and simple FFT/IFFT interface. Users no longer need to worry about their FFT results being affected by other programs. All FFT tables are allocated and released within the function scope.  
2. Reimplements an int16 FFT/IFFT. Dynamic quantization is used during butterfly operations to achieve better precision.  
3. [TODO] Uses built-in FFT instructions on ESP32-S3 and ESP32-P4 to further accelerate int16 FFT/IFFT.


## Benchmark

test code: [test_apps/dl_fft](https://github.com/espressif/esp-dl/tree/master/test_apps/dl_fft) 

- [ESP32-S3 fft benchmark](./benchmark_esp32s3.md)
- [ESP32-P4 fft benchmark](./benchmark_esp32p4.md)
- [ESP32-C5 fft benchmark](./benchmark_esp32c5.md)


## Reference

- [esp-dsp](https://github.com/espressif/esp-dsp)
- [kissfft](https://github.com/mborgerding/kissfft)
- [fftw](https://github.com/FFTW/fftw3)
