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

#include <string.h>
#include "unity.h"
#include "esp_dsp.h"
#include "dsp_platform.h"
#include "esp_log.h"

#include "dspm_mult.h"
#include "esp_attr.h"
#include "dsp_tests.h"
#include "mat.h"

static const char *TAG = "dspm_Mat";

TEST_CASE("Mat class ", "[dspm]")
{
    int m = 3;
    int n = 3;
    dspm::Mat mat(m, n);
    std::cout << "Test matrix: rows: " << mat.rows << ", columns: " << mat.cols << std::endl;
    std::cout << mat;
}

TEST_CASE("Mat class check solve ", "[dspm]")
{
    int m = 3;
    int n = 3;
    float data_a[9] = {3, 2, 1, 2, 3, 1, 2, 1, 3};
    float data_b[9] = {5, -1, 4};
    dspm::Mat A(data_a, m, n);
    dspm::Mat b(data_b, m, 1);
    dspm::Mat x1 = dspm::Mat::solve(A, b);
    std::cout << "Solve result matrix: rows: " << x1.rows << ", columns: " << x1.cols << std::endl;
    std::cout << (x1 * 12).t();
    dspm::Mat x2 = dspm::Mat::roots(A, b);
    std::cout << "Roots result matrix: rows: " << x2.rows << ", columns: " << x2.cols << std::endl;
    std::cout << (x2 * 12).t();
    dspm::Mat diff_b = x1 - x2;
    std::cout << "Difference between solve() abd roots(): " << diff_b.t();
    for (int m = 0 ; m < diff_b.rows; m++) {
        for (int n = 0 ; n < diff_b.cols ; n++) {
            if (fabs(diff_b(m, n)) > 0.000001) {
                TEST_ASSERT_MESSAGE (false, "Calculation is incorrect! Error more then expected!");
            }
        }
    }
}

TEST_CASE("Mat class basic operations", "[dspm]")
{
    int M = 4;
    int N = 4;

    dspm::Mat A(M, N);
    dspm::Mat x(N, 1);
    for (int m = 0 ; m < M ; m++) {
        for (int n = 0 ; n < N ; n++) {
            A(m, n) = N * (m + 1) + (n + 1);
        }
        x(m, 0) = m + 2;
    }

    A(0, 0) = 10;
    A(0, 1) = 11;


    dspm::Mat b = A * x;
    dspm::Mat x1_ = dspm::Mat::solve(A, b);
    dspm::Mat x2_ = dspm::Mat::roots(A, b);

    ESP_LOGI(TAG, "Matrix A:");
    std::cout << A;
    ESP_LOGI(TAG, "Matrix x.t():");
    std::cout << x.t();
    ESP_LOGI(TAG, "Matrix b.t():");
    std::cout << b.t();
    ESP_LOGI(TAG, "Solve result:");
    std::cout << x1_.t();
    ESP_LOGI(TAG, "Roots result:");
    std::cout << x2_.t();
    dspm::Mat check_b = A * x1_;
    ESP_LOGI(TAG, "Result b.t():");
    std::cout << check_b.t();
    dspm::Mat diff_b = check_b - b;
    ESP_LOGI(TAG, "Difference:");
    std::cout << diff_b.t();

    for (int m = 0 ; m < diff_b.rows; m++) {
        for (int n = 0 ; n < diff_b.cols ; n++) {
            float error = fabs(diff_b(m, n));
            if (fabs(diff_b(m, n)) > 0.0001) {
                ESP_LOGE(TAG, "Solve calculation error: %f", error);
                TEST_ASSERT_MESSAGE (false, "Calculation is incorrect! Error more then expected!");
            }
        }
    }
}

TEST_CASE("Mat class operators", "[dspm]")
{
    int M = 4;
    int N = 4;

    dspm::Mat test1(M, N);
    dspm::Mat test2(M, N);
    dspm::Mat result(M, N);
    float *check_array = new float[M * N];
    for (int m = 0 ; m < M ; m++) {
        for (int n = 0 ; n < N ; n++) {
            test1(m, n) = (m * N + n) * 2;
            test2(m, n) = m * N + n;
            result(m, n) = 0;
        }
    }

    result = test1 + test2;
    for (int m = 0 ; m < M ; m++) {
        for (int n = 0 ; n < N ; n++) {
            if ((result(m, n) != (test1(m, n) + test2(m, n))) ||
                    (result(m, n) != 3 * (m * N + n)) ||
                    (result.data[m * N + n] != 3 * (m * N + n))) {
                TEST_ASSERT_MESSAGE (false, "Error in + operator!");
            }
        }
    }
    result = test1 - test2;
    for (int m = 0 ; m < M ; m++) {
        for (int n = 0 ; n < N ; n++) {
            if ((result(m, n) != (test1(m, n) - test2(m, n))) ||
                    (result(m, n) != (m * N + n)) ||
                    (result.data[m * N + n] != (m * N + n))) {
                TEST_ASSERT_MESSAGE (false, "Error in - operator!");
            }
        }
    }
    // Check * operator (result = A*B;)
    // result = I*test2
    // result == test2
    test1 = test1.eye(test1.rows);
    result = test1 * test2;
    dspm::Mat result2 = test1;
    result2 *= test2;

    for (int m = 0 ; m < M ; m++) {
        for (int n = 0 ; n < N ; n++) {
            // if (result(m,n) < 0.000000001)
            // {
            //     result(m,n) = 0;
            // }
            if ((result(m, n) != test2(m, n)) ||
                    (result(m, n) != (m * N + n)) ||
                    (result.data[m * N + n] != (m * N + n))) {
                std::cout << "Error: " << result(m, n) << "!=" << test2(m, n) << " , "
                          << result(m, n) << "!=" << (m * N + n) << " , "
                          << result.data[m * N + n] << "!=" << (m * N + n) << std::endl;
                TEST_ASSERT_MESSAGE (false, "Error in * operator!");
            }
        }
    }
    if (!(result == result2)) {
        std::cout << "result matrix: " << std::endl << result << std::endl;
        std::cout << "result2 matrix: " << std::endl << result2 << std::endl;
        TEST_ASSERT_MESSAGE (false, "Error in *= or in == operator!");
    }
    // Check * and + operator (result = A*const1 + const2;)

    test1 = test2;
    float const1 = 2;
    float const2 = 10;
    result = test1 * const1 + const2;
    result = (result - const2) / const1;
    for (int m = 0 ; m < M ; m++) {
        for (int n = 0 ; n < N ; n++) {
            if ((result(m, n) != test2(m, n)) ||
                    (result(m, n) != (m * N + n)) ||
                    (result.data[m * N + n] != (m * N + n))
               ) {
                TEST_ASSERT_MESSAGE (false, "Error in + * const operator!");
            }
        }
    }
    // Test block(...):
    int count = 0;
    for (int m = 0 ; m < M ; m++) {
        for (int n = 0 ; n < N ; n++) {
            result(m, n) = count++;
        }
    }
    std::cout << "Original matrix: " <<  std::endl;
    std::cout << result << std::endl;
    std::cout << "block: " << std::endl;
    std::cout << result.block(1, 1, M - 1, N - 1) << std::endl;
    // Test normalize()
    result = dspm::Mat(2, 2);
    for (int m = 0 ; m < result.rows ; m++) {
        for (int n = 0 ; n < result.cols ; n++) {
            result(m, n) = 1;
        }
    }
    std::cout << "Befor normalize: " << std::endl;
    std::cout << result << std::endl;
    result.normalize();
    std::cout << "normalize: " << std::endl;
    std::cout << result << std::endl;

    for (int m = 0 ; m < result.rows ; m++) {
        for (int n = 0 ; n < result.cols ; n++) {
            if (std::abs(result(m, n) - 0.5) > dspm::Mat::abs_tol) {
                ESP_LOGE(TAG, "Error bigger then expected: %f", std::abs(result(m, n) - 0.5));
                TEST_ASSERT_MESSAGE (false, "Error in normalize() operation! ");
            }
        }
    }
    // Test inverse()
    float m_data[] = {2, 5, 7,
                      6, 3, 4,
                      5, -2, -3
                     };
    float m_result[] = {  1.0000,   -1.0000,    1.0000,
                          -38.0000,   41.0000,  -34.0000,
                          27.0000,  -29.0000,   24.0000
                       };
    result = dspm::Mat(m_data, 3, 3);
    result = result.inverse();
    std::cout << "inverse: " << std::endl;
    std::cout << result << std::endl;
    for (int i = 0 ; i < 3 * 3 ; i++) {
        if (std::abs(result.data[i] - m_result[i]) > 1e-4) {
            printf("Error at[%i] = %f, expected= %f, calculated = %f \n", i, std::abs(result.data[i] - m_result[i]), m_result[i], result.data[i]);
            TEST_ASSERT_MESSAGE (false, "Error in inverse() operation!\n");
        }
    }

    result = dspm::Mat(m_data, 3, 3);
    result = result.pinv();
    std::cout << "pinv: " << std::endl;
    std::cout << result << std::endl;
    for (int i = 0 ; i < 3 * 3 ; i++) {
        if (std::abs(result.data[i] - m_result[i]) > 1e-2) {
            printf("Error at[%i] = %f, expected= %f, calculated = %f \n", i, std::abs(result.data[i] - m_result[i]), m_result[i], result.data[i]);
            TEST_ASSERT_MESSAGE (false, "Error in pinv() operation!\n");
        }
    }

    delete[] check_array;
}

TEST_CASE("mat.cpp functionality", "[dsps]")
{
    int max_size = 10;
    for (int i = 3 ; i < max_size ; i++) {
        dspm::Mat A = dspm::Mat::eye(i);
        float det = A.det(i);
        printf("Det[%i] = %f\n", i, det);
        TEST_ASSERT_EQUAL(det, 1);
    }
}
