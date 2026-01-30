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

#include "dsps_dotprod.h"
#include "dspm_mult.h"

// Matrinx A(m,n), m - amount or rows, n - amount of columns
// C(m,k) = A(m,n)*B(n,k)
// c(i,j) = sum(a(i,s)*b(s,j)) , s=1..n
esp_err_t dspm_mult_s16_ansi(const int16_t *A, const int16_t *B, int16_t *C, int m, int n, int k, int shift)
{
    int final_shift = shift - 15;
    for (int i = 0 ; i < m ; i++) {
        for (int j = 0 ; j < k ; j++) {
            // This code also could be used
            //dsps_dotprode_f32_ae32(&A[i*n],&B[j],&C[i*k + j],n,1,n);
            long long acc = 0x7fff >> shift;
            for (int s = 0; s < n ; s++) {
                acc += (int32_t)A[i * n + s] * (int32_t)B[s * k + j];
            }
            if (final_shift > 0) {
                C[i * k + j] = (acc << final_shift);
            } else {
                C[i * k + j] = (acc >> (-final_shift));
            }
        }
    }
    return ESP_OK;
}
