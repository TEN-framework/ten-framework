// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
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

#include "dsps_fft2r.h"
#include "dsp_common.h"
#include "dsp_types.h"
#include <math.h>
#include "esp_attr.h"
#include "dsps_fft2r_platform.h"

#if (dsps_bit_rev_lookup_fc32_ae32_enabled == 1)

esp_err_t dsps_bit_rev_lookup_fc32_ae32(float *data, int reverse_size, uint16_t *reverse_tab)
{
    int idx_0, idx_1;
    int idx_2, idx_3;

    float *data_i;

    asm volatile ("addi %0, %1, 4" : "=a" (data_i) : "a" (data)); ///data_i = &data[1];
    asm volatile ("srli %0, %0, 1" : "+a"  (reverse_size)); ///    reverse_size >>= 1;
    asm volatile ("loopnez %0, __loop_end_radix2_reorder_lookup_table" :: "a" (reverse_size)); ///for (i = 0; i < reverse_size; i++) {
    asm volatile ("l16ui %0, %1, 0" : "=a" (idx_0) : "a" (reverse_tab)); //idx_0 = *(reverse_tab + 0);  ///idx_0 = reverse_tab[i * 2 + 0];
    asm volatile ("l16ui %0, %1, 2" : "=a" (idx_1) : "a" (reverse_tab)); //idx_1 = *(reverse_tab + 1);  ///idx_1 = reverse_tab[i * 2 + 1];
    asm volatile ("l16ui %0, %1, 4" : "=a" (idx_2) : "a" (reverse_tab)); //idx_2 = *(reverse_tab + 2);  ///idx_2 = reverse_tab[i * 2 + 2];
    asm volatile ("l16ui %0, %1, 6" : "=a" (idx_3) : "a" (reverse_tab)); //idx_3 = *(reverse_tab + 3);  ///idx_3 = reverse_tab[i * 2 + 3];
    asm volatile ("addi %0, %0, 8" : "+a" (reverse_tab)); // reverse_tab += 4;

    asm volatile ("lsx f0, %0, %1" :: "a" (data), "a" (idx_0)); // f0 = *(data + idx_0); //f0 = data[idx_0 + 0];
    asm volatile ("lsx f2, %0, %1" :: "a" (data_i), "a" (idx_0)); // f2 = *(data_i + idx_0); //f2 = data[idx_0 + 1];
    asm volatile ("lsx f1, %0, %1" :: "a" (data), "a" (idx_1)); //f1 = *(data + idx_1); //f1 = data[idx_1 + 0];
    asm volatile ("lsx f3, %0, %1" :: "a" (data_i), "a" (idx_1)); //f3 = *(data_i + idx_1); //f3 = data[idx_1 + 1];

    asm volatile ("ssx f0, %0, %1" :: "a" (data), "a" (idx_1)); //*(data + idx_1) = f0; //data[idx_1 + 0] = f0;
    asm volatile ("ssx f2, %0, %1" :: "a" (data_i), "a" (idx_1)); //*(data_i + idx_1) = f2; //data[idx_1 + 1] = f2;
    asm volatile ("ssx f1, %0, %1" :: "a" (data), "a" (idx_0)); //*(data + idx_0) = f1; //data[idx_0 + 0] = f1;
    asm volatile ("ssx f3, %0, %1" :: "a" (data_i), "a" (idx_0)); //*(data_i + idx_0) = f3;//data[idx_0 + 1] = f3;

    asm volatile ("lsx f0, %0, %1" :: "a" (data), "a" (idx_2)); // f0 = *(data + idx_2); //f0 = data[idx_2 + 0];
    asm volatile ("lsx f2, %0, %1" :: "a" (data_i), "a" (idx_2)); // f2 = *(data_i + idx_2); //f2 = data[idx_2 + 1];
    asm volatile ("lsx f1, %0, %1" :: "a" (data), "a" (idx_3)); //f1 = *(data + idx_3); //f1 = data[idx_3 + 0];
    asm volatile ("lsx f3, %0, %1" :: "a" (data_i), "a" (idx_3)); //f3 = *(data_i + idx_3); //f3 = data[idx_3 + 1];

    asm volatile ("ssx f0, %0, %1" :: "a" (data), "a" (idx_3)); //*(data + idx_3) = f0; //data[idx_3 + 0] = f0;
    asm volatile ("ssx f2, %0, %1" :: "a" (data_i), "a" (idx_3)); //*(data_i + idx_3) = f2; //data[idx_3 + 1] = f2;
    asm volatile ("ssx f1, %0, %1" :: "a" (data), "a" (idx_2)); //*(data + idx_2) = f1; //data[idx_2 + 0] = f1;
    asm volatile ("ssx f3, %0, %1" :: "a" (data_i), "a" (idx_2)); //*(data_i + idx_2) = f3; //data[idx_2 + 1] = f3;
    //}
    asm volatile("__loop_end_radix2_reorder_lookup_table: nop");
    return ESP_OK;
}
#endif // dsps_bit_rev_lookup_fc32_ae32_enabled
