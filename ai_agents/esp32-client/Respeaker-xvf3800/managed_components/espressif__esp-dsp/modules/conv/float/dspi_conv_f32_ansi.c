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

#include "dspi_conv.h"
#include "esp_log.h"

esp_err_t dspi_conv_f32_ansi(const image2d_t *in_image, const image2d_t *filter, image2d_t *out_image)
{
    out_image->size_x = in_image->size_x;
    out_image->size_y = in_image->size_y;
    float *i_data =  (float *)in_image->data;
    float *out_data = (float *)out_image->data;

    int rest_x = (filter->size_x - 1) >> 1;
    int rest_y = (filter->size_y - 1) >> 1;

    int i_pos = 0;
    int i_step = in_image->stride_x * in_image->step_y;
    int f_step = filter->stride_x * filter->step_y;

    // Up side of image
    for (int y = 0 ; y < rest_y; y++ ) {
        int i_pos_y = i_pos;
        for (int x = 0 ; x < rest_x; x++) {
            int i_pos_x = i_pos_y;
            float acc = 0;
            float *f_data =  (float *)filter->data;
            for (int m = rest_y - y ; m < filter->size_y ; m++) {
                for (int n = rest_x - x ; n < filter->size_x ; n++) {
                    acc += i_data[i_pos_x + n * in_image->step_x] * f_data[filter->step_x * n];
                }
                f_data += f_step;
                i_pos_x += i_step;
            }
            i_pos_y += in_image->step_x;
            out_data[x * out_image->step_x + y * out_image->stride_x * out_image->step_y] = acc;
        }
        for (int x = rest_x ; x < in_image->size_x - filter->size_x / 2; x++) {
            int i_pos_x = i_pos_y;
            float acc = 0;
            float *f_data =  (float *)filter->data;
            for (int m = rest_y - y ; m < filter->size_y ; m++) {
                for (int n = 0 ; n < filter->size_x ; n++) {
                    acc += i_data[i_pos_x + n * in_image->step_x] * f_data[filter->step_x * n];
                }
                f_data += f_step;
                i_pos_x += i_step;
            }
            i_pos_y += in_image->step_x;
            out_data[x * out_image->step_x + y * out_image->stride_x * out_image->step_y] = acc;
        }
        for (int x = in_image->size_x - filter->size_x / 2 - 1; x < in_image->size_x; x++) {
            int i_pos_x = i_pos_y;
            float acc = 0;
            float *f_data =  (float *)filter->data;
            for (int m = rest_y - y ; m < filter->size_y ; m++) {
                for (int n = 0 ; n < filter->size_x - (x - in_image->size_x + filter->size_x / 2 + 1); n++) {
                    acc += i_data[i_pos_x + n * in_image->step_x] * f_data[filter->step_x * n];
                }
                f_data += f_step;
                i_pos_x += i_step;
            }
            i_pos_y += in_image->step_x;
            out_data[x * out_image->step_x + y * out_image->stride_x * out_image->step_y] = acc;
        }
        i_pos += in_image->stride_x * in_image->step_y;
    }
    // Middle side of image
    i_pos = 0;
    for (int y = rest_y ; y < in_image->size_y - filter->size_y / 2; y++ ) {
        int i_pos_y = i_pos;
        for (int x = 0 ; x < rest_x; x++) {
            int i_pos_x = i_pos_y;
            float acc = 0;
            float *f_data =  (float *)filter->data;
            for (int m = 0 ; m < filter->size_y ; m++) {
                for (int n = rest_x - x ; n < filter->size_x ; n++) {
                    acc += i_data[i_pos_x + n * in_image->step_x] * f_data[filter->step_x * n];
                }
                f_data += f_step;
                i_pos_x += i_step;
            }
            i_pos_y += in_image->step_x;
            out_data[x * out_image->step_x + y * out_image->stride_x * out_image->step_y] = acc;
        }
        for (int x = in_image->size_x - filter->size_x / 2 - 1; x < in_image->size_x; x++) {
            int i_pos_x = i_pos_y;
            float acc = 0;
            float *f_data =  (float *)filter->data;
            for (int m = 0 ; m < filter->size_y ; m++) {
                for (int n = 0 ; n < filter->size_x - (x - in_image->size_x + filter->size_x / 2 + 1); n++) {
                    acc += i_data[i_pos_x + n * in_image->step_x] * f_data[filter->step_x * n];
                }
                f_data += f_step;
                i_pos_x += i_step;
            }
            i_pos_y += in_image->step_x;
            out_data[x * out_image->step_x + y * out_image->stride_x * out_image->step_y] = acc;
        }

        i_pos += in_image->stride_x * in_image->step_y;
    }
    // Down side of image
    i_pos = 0;
    for (int y = in_image->size_y - filter->size_y / 2 ; y < in_image->size_y; y++ ) {
        int i_pos_y = i_pos;
        for (int x = 0 ; x < rest_x; x++) {
            int i_pos_x = i_pos_y;
            float acc = 0;
            float *f_data =  (float *)filter->data;
            for (int m = 0 ; m < filter->size_y - (y - in_image->size_y + filter->size_y / 2 + 1); m++) {
                for (int n = rest_x - x ; n < filter->size_x ; n++) {
                    acc += i_data[i_pos_x + n * in_image->step_x] * f_data[filter->step_x * n];
                }
                f_data += f_step;
                i_pos_x += i_step;
            }
            i_pos_y += in_image->step_x;
            out_data[x * out_image->step_x + y * out_image->stride_x * out_image->step_y] = acc;
        }
        for (int x = rest_x ; x < in_image->size_x - filter->size_x / 2; x++) {
            int i_pos_x = i_pos_y;
            float acc = 0;
            float *f_data =  (float *)filter->data;
            for (int m = 0 ; m < filter->size_y - (y - in_image->size_y + filter->size_y / 2 + 1); m++) {
                for (int n = 0 ; n < filter->size_x ; n++) {
                    acc += i_data[i_pos_x + n * in_image->step_x] * f_data[filter->step_x * n];
                }
                f_data += f_step;
                i_pos_x += i_step;
            }
            i_pos_y += in_image->step_x;
            out_data[x * out_image->step_x + y * out_image->stride_x * out_image->step_y] = acc;
        }
        for (int x = in_image->size_x - filter->size_x / 2 ; x < in_image->size_x; x++) {
            int i_pos_x = i_pos_y;
            float acc = 0;
            float *f_data =  (float *)filter->data;
            for (int m = 0 ; m < filter->size_y - (y - in_image->size_y + filter->size_y / 2 + 1); m++) {
                for (int n = 0 ; n < filter->size_x - (x - in_image->size_x + filter->size_x / 2 + 1); n++) {
                    acc += i_data[i_pos_x + n * in_image->step_x] * f_data[filter->step_x * n];
                }
                f_data += f_step;
                i_pos_x += i_step;
            }
            i_pos_y += in_image->step_x;
            out_data[x * out_image->step_x + y * out_image->stride_x * out_image->step_y] = acc;
        }

        i_pos += in_image->stride_x * in_image->step_y;
    }
    // Main image block
    i_pos = 0;
    for (int y = rest_y ; y < in_image->size_y - filter->size_y / 2; y++ ) {
        int i_pos_y = i_pos;
        for (int x = rest_x ; x < in_image->size_x - filter->size_x / 2; x++) {
            int i_pos_x = i_pos_y;
            float acc = 0;
            float *f_data =  (float *)filter->data;
            for (int m = 0 ; m < filter->size_y ; m++) {
                for (int n = 0 ; n < filter->size_x ; n++) {
                    acc += i_data[i_pos_x + n * in_image->step_x] * f_data[filter->step_x * n];
                }
                f_data += f_step;
                i_pos_x += i_step;
            }
            i_pos_y += in_image->step_x;
            out_data[x * out_image->step_x + y * out_image->stride_x * out_image->step_y] = acc;
        }
        i_pos += in_image->stride_x * in_image->step_y;
    }
    return ESP_OK;
}
