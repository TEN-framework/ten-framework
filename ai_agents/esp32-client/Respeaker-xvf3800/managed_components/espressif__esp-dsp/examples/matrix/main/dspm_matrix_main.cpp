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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dsp_platform.h"
#include "esp_log.h"

#include "esp_dsp.h"

static const char *TAG = "main";

// This example shows how to use Mat class from esp-dsp library.
//
// First we create matix A and x, and then calculating matrix b as result
// A*x = b
// Then we can find x as roots of matrices X and b
//
extern "C" void app_main();

void app_main()
{
    ESP_LOGI(TAG, "Start Example.");
    int M = 3;
    int N = 3;
    dspm::Mat A(M, N);
    dspm::Mat x(N, 1);
    for (int m = 0 ; m < M ; m++) {
        for (int n = 0 ; n < N ; n++) {
            A(m, n) = N * m + n;
        }
        x(m, 0) = m;
    }
    A(0, 0) = 10;
    A(0, 1) = 11;

    dspm::Mat b = A * x;
    // Gaussian method
    dspm::Mat x1_ = dspm::Mat::solve(A, b);
    // Non Gaussian method
    dspm::Mat x2_ = dspm::Mat::roots(A, b);

    ESP_LOGI(TAG, "Original vector x:");
    std::cout << x;
    ESP_LOGI(TAG, "Solve result:");
    std::cout << x1_;
    ESP_LOGI(TAG, "Roots result:");
    std::cout << x2_;
    ESP_LOGI(TAG, "End Example.");
}
