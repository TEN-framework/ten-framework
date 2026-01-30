// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _dspi_conv_H_
#define _dspi_conv_H_
#include "dsp_err.h"

#include "dsps_conv_platform.h"
#include "dsp_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**@{*/
/**
 * @brief   2D Convolution
 *
 * The function convolve Signal image with Kernel (filter) image.
 * The implementation use ANSI C and could be compiled and run on any platform
 *
 * @param[in] in_image:  input image
 * @param[in] filter:    input array with convolution kernel
 * @param[out] out_image: output image. The stride and step parameters must be set.
 *
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dspi_conv_f32_ansi(const image2d_t *in_image, const image2d_t *filter, image2d_t *out_image);
/**@}*/

#ifdef __cplusplus
}
#endif

#ifdef CONFIG_DSP_OPTIMIZED
#define dspi_conv_f32 dspi_conv_f32_ansi
#else
#define dspi_conv_f32 dspi_conv_f32_ansi
#endif // CONFIG_DSP_OPTIMIZED

#endif // _dspi_conv_H_
