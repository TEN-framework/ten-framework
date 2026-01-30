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
#include "dsps_fft4r.h"
#include "dsp_common.h"
#include "dsp_types.h"
#include <math.h>
#include "esp_attr.h"

#include "dsps_fft4r_platform.h"


#if (dsps_cplx2real_fc32_ae32_enabled == 1)
esp_err_t dsps_cplx2real_fc32_ae32_(float *data, int fft_points, float *table, int table_size)
{
    float *ptr_inv;

    int wind_step = table_size / (fft_points);

    float *win0 = table + wind_step;
    float *win1 = table + wind_step * 2;

    ////0 & 1 => dc frequency
    ///fft_points * 2 + 0 & fft_points * 2 + 1 => Nyquist frequency
    asm volatile ("const.s f14, 0");  //f14 = 0f;
    asm volatile ("lsi f0, %0, 0" :: "a" (data)); //f0 = *data;
    asm volatile ("lsi f1, %0, 4" :: "a" (data)); //f1 = *(data + 1);
    asm volatile ("addx8 %0, %1, %2" : "=a" (ptr_inv) : "a" (fft_points), "a" (data)); //ptr_inv = data + fft_points * 2;
    asm volatile ("add.s f6, f0, f1"); //f6 = f0 + f1;
    asm volatile ("sub.s f7, f0, f1"); //f7 = f0 - f1;

    asm volatile ("srli %0, %0, 2" : "+a" (fft_points)); //fft_points >>= 2;

    asm volatile ("const.s f14, 3");  //f14 = 0.5f;, this is for multiply 0.5
    asm volatile ("neg.s f15, f14");  //f15 = -f14;

    asm volatile ("ssi f6, %0, 0" :: "a" (data)); //*data = f6;
    asm volatile ("ssi f7, %0, 4" :: "a" (data)); //*ptr_inv = f7;
    asm volatile ("addi %0, %0, -16" : "+a" (ptr_inv)); //ptr_inv -= 4; ///here increase address by -4 because float load/store not support negective immediate offset

    asm volatile ("loopnez %0, __loop_end_fftr_real_post_proc" :: "a" (fft_points)); //for (k = 0; k < fft_points; k++) {
    asm volatile ("lsi f1, %0, 12" :: "a" (data)); //f1 = *(data + 3);
    asm volatile ("lsi f3, %0, 12" :: "a" (ptr_inv)); //f3 = *(ptr_inv + 3);
    asm volatile ("lsi f0, %0, 8" :: "a" (data)); //f0 = *(data + 2);
    asm volatile ("lsi f2, %0, 8" :: "a" (ptr_inv)); //f2 = *(ptr_inv + 2);

    asm volatile ("lsi f7, %0, 20" :: "a" (data)); //f7 = *(data + 5);
    asm volatile ("lsi f9, %0, 4" :: "a" (ptr_inv)); //f9 = *(ptr_inv + 1);

    asm volatile ("lsi f6, %0, 16" :: "a" (data)); //f6 = *(data + 4);
    asm volatile ("lsi f8, %0, 0" :: "a" (ptr_inv)); //f8 = *ptr_inv;
    asm volatile ("sub.s f5, f1, f3"); //f5 = f1 - f3;
    asm volatile ("add.s f4, f0, f2"); //f4 = f0 + f2;
    asm volatile ("sub.s f11, f7, f9"); //f11 = f7 - f9;
    asm volatile ("add.s f10, f6, f8"); //f10 = f6 + f8;

    asm volatile ("add.s f1, f1, f3"); //f1 = f1 + f3;
    asm volatile ("sub.s f0, f0, f2"); //f0 = f0 - f2;

    asm volatile ("lsi f12, %0, 4" :: "a" (win0)); //f12 = *(win + 1);
    asm volatile ("add.s f7, f7, f9"); //f7 = f7 + f9;
    asm volatile ("sub.s f6, f6, f8"); //f6 = f6 - f8;
    asm volatile ("lsi f13, %0, 4" :: "a" (win1)); //f13 = *(win + 3);

    asm volatile ("mul.s f3, f1, f12"); //f3 = f1 * f12;
    asm volatile ("mul.s f2, f0, f12"); //f2 = f0 * f12;
    asm volatile ("lsi f12, %0, 0" :: "a" (win0)); //f12 = *(win + 0);

    asm volatile ("mul.s f9, f7, f13"); //f9 = f7 * f13;
    asm volatile ("mul.s f8, f6, f13"); //f8 = f6 * f13;

    asm volatile ("lsi f13, %0, 0" :: "a" (win1)); //f13 = *(win + 2);

    asm volatile ("madd.s f3, f0, f12"); //f3 += f0 * f12;
    asm volatile ("msub.s f2, f1, f12"); //f2 -= f1 * f12;
    asm volatile ("madd.s f9, f6, f13"); //f9 += f6 * f13;
    asm volatile ("msub.s f8, f7, f13"); //f8 -= f7 * f13;
    asm volatile ("addx8 %0, %1, %0" : "+a" (win0) : "a" (wind_step)); //win0 += 8 * wind_step;
    asm volatile ("addx8 %0, %1, %0" : "+a" (win1) : "a" (wind_step)); //win1 += 8 * wind_step;
    // Here we have tw1: f2,f3 and tw2: f8,f9

    asm volatile ("sub.s f1, f5, f3"); //f1 = f5 - f3;
    asm volatile ("sub.s f0, f4, f2"); //f0 = f4 - f2;

    asm volatile ("add.s f3, f3, f5"); //f3 = f3 + f5;
    asm volatile ("add.s f2, f4, f2"); //f2 = f4 + f2;

    asm volatile ("sub.s f7, f11, f9"); //f7 = f11 - f9;
    asm volatile ("sub.s f6, f10, f8"); //f6 = f10 - f8;
    asm volatile ("add.s f9, f9, f11"); //f9 = f9 + f11;
    asm volatile ("add.s f8, f10, f8"); //f8 = f10 + f8;

    asm volatile ("mul.s f1, f1, f14"); //f1 *= f14;
    asm volatile ("mul.s f0, f0, f14"); //f0 *= f14;

    asm volatile ("mul.s f3, f3, f15"); //f3 *= -f14;
    asm volatile ("mul.s f2, f2, f14"); //f2 *= f14;

    asm volatile ("mul.s f7, f7, f14"); //f7 *= f14;
    asm volatile ("mul.s f6, f6, f14"); //f6 *= f14;
    asm volatile ("mul.s f9, f9, f15"); //f9 *= -f14;
    asm volatile ("mul.s f8, f8, f14"); //f8 *= f14;

    asm volatile ("ssi f1, %0, 12" :: "a" (data)); //*(data + 3) = f1;
    asm volatile ("ssi f0, %0, 8" :: "a" (data)); //*(data + 2) = f0;

    asm volatile ("ssi f3, %0, 12" :: "a" (ptr_inv)); //*(ptr_inv + 3) = f3;
    asm volatile ("ssi f2, %0, 8" :: "a" (ptr_inv)); //*(ptr_inv + 2) = f2;

    asm volatile ("ssi f7, %0, 20" :: "a" (data)); //*(data + 5) = f7;
    asm volatile ("ssi f6, %0, 16" :: "a" (data)); //*(data + 4) = f6;
    asm volatile ("addi %0, %0, 16" : "+a" (data)); //data += 4;

    asm volatile ("ssi f9, %0, 4" :: "a" (ptr_inv)); //*(ptr_inv + 1) = f9;
    asm volatile ("ssi f8, %0, 0" :: "a" (ptr_inv)); //*ptr_inv = f8;
    asm volatile ("addi %0, %0, -16" : "+a" (ptr_inv)); //ptr_inv -= 4;
    //}
    asm volatile ("__loop_end_fftr_real_post_proc: nop");

    return ESP_OK;
}
#endif // dsps_cplx2real_fc32_ae32_enabled
