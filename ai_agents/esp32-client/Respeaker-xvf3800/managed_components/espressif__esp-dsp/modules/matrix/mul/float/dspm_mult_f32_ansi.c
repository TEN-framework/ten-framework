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
esp_err_t dspm_mult_f32_ansi(const float *A, const float *B, float *C, int m, int n, int k)
{
    for (int i = 0 ; i < m ; i++) {
        for (int j = 0 ; j < k ; j++) {
            C[i * k + j] = A[i * n] * B[j];
            for (int s = 1; s < n ; s++) {
                C[i * k + j] += A[i * n + s] * B[s * k + j];
            }
        }
    }
    return ESP_OK;
}
