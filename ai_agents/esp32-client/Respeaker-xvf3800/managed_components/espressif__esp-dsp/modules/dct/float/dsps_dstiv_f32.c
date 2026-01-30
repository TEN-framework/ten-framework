// Copyright 2018-2020 Espressif Systems (Shanghai) PTE LTD
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

#include "dsp_common.h"
#include <math.h>

#include "dsps_dct.h"
#include "dsps_fft2r.h"

esp_err_t dsps_dstiv_f32(float *data, int ndst)
{
    if (dsps_fft2r_initialized == 0) {
        return ESP_ERR_DSP_REINITIALIZED;
    }

    float in1, in2, in3, in4;
    float factor = M_PI / (ndst);

    for (int i = 0; i < ndst / 4; i++) {

        in1 = data[2 * i + 0];
        in2 = data[2 * i + 1];

        in3 = data[ndst - 2 * i - 2];
        in4 = data[ndst - 2 * i - 1];

        data[i * 2 + 1] = (
                              in1 * cos(factor * (i + 0))
                              - in4 * sin(factor * ((ndst - i - 1)))
                          );

        data[i * 2 + 0] = (
                              in1 * sin(factor * (i))
                              - in4 * cos(factor * ((ndst - i - 1)))
                          );

        data[ndst - i * 2 - 2] = (
                                     -in3 * cos(factor * (ndst - i - 1))
                                     + in2 * sin(factor * (ndst - i - 1))
                                 );

        data[ndst - i * 2 - 1] = (
                                     +in3 * sin(factor * (i + 1))
                                     - in2 * cos(-factor * (i + 1))
                                 );

    }

    esp_err_t error = ESP_OK;
    error = dsps_fft2r_fc32(data, ndst / 2);
    if (error != ESP_OK) {
        return error;
    }
    error = dsps_bit_rev_fc32(data, ndst / 2);
    if (error != ESP_OK) {
        return error;
    }

    for (int i = 0; i < ndst / 4; i++) {
        in1 = data[2 * i + 0];
        in2 = data[2 * i + 1];

        in3 = data[ndst - 2 * i - 2];
        in4 = data[ndst - 2 * i - 1];

        data[i * 2 + 0] = (
                              in1 * cos(factor * (0 + i))
                              + in2 * sin(factor * (0 + i))
                          );

        data[ndst - i * 2 - 2 + 1] = (
                                         -in1 * cos(factor * (ndst / 2 - i))
                                         + in2 * sin(factor * (ndst / 2 - i))
                                     );

        data[i * 2 + 1] = (
                              -in3 * cos(factor * (1 + i))
                              + in4 * sin(factor * (1 + i))
                          );

        data[ndst  - i * 2 - 2 + 0] = (
                                          +in3 * cos(factor * (ndst / 2 - i - 1))
                                          + in4 * sin(factor * (ndst / 2 - i - 1))
                                      );
    }
    return ESP_OK;
}
