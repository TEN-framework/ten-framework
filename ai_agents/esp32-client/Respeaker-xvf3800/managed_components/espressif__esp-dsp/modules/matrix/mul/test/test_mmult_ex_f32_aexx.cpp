/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <malloc.h>
#include "unity.h"
#include "esp_dsp.h"
#include "dsp_platform.h"
#include "esp_log.h"

#include "dspm_mult.h"
#include "esp_attr.h"
#include "dsp_tests.h"
#include "test_mat_common.h"


// create ROI rectangles
dspm::Mat::Rect A_roi_rect;
dspm::Mat::Rect B_roi_rect;
dspm::Mat::Rect C_roi_rect;

static void dspm_mult_ex_f32_aexx_functionality_in_cycle(m_test_data_t *test_d)
{
    char message[120];
    sprintf(message, "var = %d,  A_s_row = %d, A_s_col = %d, B_s_row = %d B_s_col = %d, C_s_row = %d,  C_s_col = %d, m = %d, n = %d, k = %d\n", test_d->var,
            test_d->A_start_row, test_d->A_start_col, test_d->B_start_row, test_d->B_start_col,
            test_d->C_start_row, test_d->C_start_col, test_d->m, test_d->n, test_d->k);

    // aligned data for A B C matrices
    float *A_data = (float *)memalign(16, ((test_d->m + (2 * test_d->A_start_row)) * (test_d->n + (2 * test_d->A_start_col))) * sizeof(float));
    float *B_data = (float *)memalign(16, ((test_d->n + (2 * test_d->B_start_row)) * (test_d->k + (2 * test_d->B_start_col))) * sizeof(float));
    float *C_data = (float *)memalign(16, ((test_d->m + (2 * test_d->C_start_row)) * (test_d->k + (2 * test_d->C_start_col))) * sizeof(float));

    // create A B C matrices with m n k dimensions + padding
    // padding is from both sides of the targeted sub-matrix
    // 1 1 1 1
    // 1 x x 1
    // 1 x x 1
    // 1 1 1 1
    dspm::Mat A(A_data, test_d->m + (2 * test_d->A_start_row), test_d->n + (2 * test_d->A_start_col));
    dspm::Mat B(B_data, test_d->n + (2 * test_d->B_start_row), test_d->k + (2 * test_d->B_start_col));
    dspm::Mat C(C_data, test_d->m + (2 * test_d->C_start_row), test_d->k + (2 * test_d->C_start_col));

    // create ROI rectangles for sub-matrices
    A_roi_rect.resizeRect(test_d->A_start_col, test_d->A_start_row, test_d->n, test_d->m);
    B_roi_rect.resizeRect(test_d->B_start_col, test_d->B_start_row, test_d->k, test_d->n);
    C_roi_rect.resizeRect(test_d->C_start_col, test_d->C_start_row, test_d->k, test_d->m);

    // aligned data for sub-matrices
    float *A_sub_data = (float *)memalign(16, A_roi_rect.areaRect() * sizeof(float));
    float *B_sub_data = (float *)memalign(16, B_roi_rect.areaRect() * sizeof(float));
    float *C_sub_data = (float *)memalign(16, C_roi_rect.areaRect() * sizeof(float));

    // create sub-matrices A, B C matrices with aligned data
    // matrices are used as sub-matrices with data copying for a matrix operation testing
    dspm::Mat A_sub(A_sub_data, test_d->m, test_d->n);
    dspm::Mat B_sub(B_sub_data, test_d->n, test_d->k);
    dspm::Mat C_sub(C_sub_data, test_d->m, test_d->k);

    // fill A B matrices with numbers
    // fill C matrix with ones
    for (int i = 0; i < A.length; i++) {
        A.data[i] = i + 1;
    }
    for (int i = 0; i < B.length; i++) {
        B.data[i] = i + 1;
    }

    if (test_d->var < 4) {
        for (int i = 0; i < C.length; i++) {
            C.data[i] = 1;
        }
    }

    // Combinations of A B C matrices and sub-matrices are created for testing
    // As an example: case 1
    // Matrices A and C are sub-matrices - the data are defined as a pointer to an external buffer
    // Matrix B is a matrix - the data are copied into the B matrix
    switch (test_d->var) {
    case 0: {
        A_sub.CopyHead(A.getROI(A_roi_rect));    // A sub-matrix - NO DATA CPY
        B_sub.CopyHead(B.getROI(B_roi_rect));    // B sub-matrix - NO DATA CPY
        C_sub.CopyHead(C.getROI(C_roi_rect));    // C sub-matrix - NO DATA CPY
    } break;
    case 1: {
        A_sub = A.Get(A_roi_rect);               // A matrix     - DATA CPY
        B_sub.CopyHead(B.getROI(B_roi_rect));    // B sub-matrix - NO DATA CPY
        C_sub.CopyHead(C.getROI(C_roi_rect));    // C sub-matrix - NO DATA CPY
    } break;
    case 2: {
        A_sub.CopyHead(A.getROI(A_roi_rect));    // A sub-matrix - NO DATA CPY
        B_sub = B.Get(B_roi_rect);               // B matrix     - DATA CPY
        C_sub.CopyHead(C.getROI(C_roi_rect));    // C sub-matrix - NO DATA CPY
    } break;
    case 3: {
        A_sub = A.Get(A_roi_rect);               // A matrix     - DATA CPY
        B_sub = B.Get(B_roi_rect);               // B matrix     - DATA CPY
        C_sub.CopyHead(C.getROI(C_roi_rect));    // C sub-matrix - NO DATA CPY
    } break;
    case 4: {
        A_sub.CopyHead(A.getROI(A_roi_rect));    // A sub-matrix - NO DATA CPY
        B_sub.CopyHead(B.getROI(B_roi_rect));    // B sub-matrix - NO DATA CPY
        C_sub = C.Get(C_roi_rect);               // B matrix     - DATA CPY
    } break;
    case 5: {
        A_sub.CopyHead(A.getROI(A_roi_rect));    // A sub-matrix - NO DATA CPY
        B_sub = B.Get(B_roi_rect);               // B matrix     - DATA CPY
        C_sub = C.Get(C_roi_rect);               // C matrix     - DATA CPY
    } break;
    case 6: {
        A_sub = A.Get(A_roi_rect);               // A matrix     - DATA CPY
        B_sub.CopyHead(B.getROI(B_roi_rect));    // B sub-matrix - NO DATA CPY
        C_sub = C.Get(C_roi_rect);               // C matrix     - DATA CPY
    } break;
    default:
        break;
    }

    // create A B check sub-matrices, actual matrix data are COPIED
    dspm::Mat A_sub_check = A.Get(A_roi_rect);
    dspm::Mat B_sub_check = B.Get(B_roi_rect);
    dspm::Mat C_sub_check(test_d->m, test_d->k);

    // Calculate C_sub_check = A_sub_check * B_sub_check
    for (int i = 0 ; i < test_d->m ; i++) {
        for (int j = 0 ; j < test_d->k ; j++) {
            C_sub_check(i, j) = 0;
            for (int s = 0 ; s < test_d->n ; s++) {
                C_sub_check(i, j) += A_sub_check(i, s) * B_sub_check(s, j);
            }
        }
    }

    dspm_mult_ex_f32(A_sub.data, B_sub.data, C_sub.data, test_d->m, test_d->n, test_d->k, A_sub.padding, B_sub.padding, C_sub.padding);

    // C is a sub-matrix
    if (C_sub.sub_matrix) {
        // Create a copy of the original C matrix (filled with ones 1)
        // to check if an area around the sub-matrix is unaffected after a matrix operation
        dspm::Mat C_area_check = dspm::Mat::ones(test_d->m + (2 * test_d->C_start_row), test_d->k + (2 * test_d->C_start_col));
        test_assert_equal_mat_mat(C_sub_check, C_sub, message);
        test_assert_check_area_mat_mat(C_area_check, C_sub, test_d->C_start_row, test_d->C_start_col, message);
        // C is a matrix
    } else {
        test_assert_equal_mat_mat(C_sub_check, C_sub, message);
    }

    free(A_data);
    free(B_data);
    free(C_data);
    free(A_sub_data);
    free(B_sub_data);
    free(C_sub_data);
}

TEST_CASE("dspm_mult_ex_f32_aexx functionality", "[dspm]")
{
    m_test_data_t test_data;

    const int test_varations = 7;
    const int start_col_min = 0;
    const int start_row_min = 0;

#if CONFIG_IDF_TARGET_ESP32S3
    const int start_col_max = 4;
    const int start_row_max = 4;
    const int col_row_increment = 4;
    const int m_max = 12;
    const int n_max = 12;
    const int k_mak = 12;
    const int dim_increment = 4;
    const int dim_start = 4;
#elif CONFIG_IDF_TARGET_ESP32P4
    const int start_col_max = 1;
    const int start_row_max = 1;
    const int col_row_increment = 1;
    const int m_max = 4;
    const int n_max = 4;
    const int k_mak = 4;
    const int dim_increment = 1;
    const int dim_start = 2; // <= the esp.lp.setup instruction is not working with loop count 1. The min value is 2.
#else
    const int start_col_max = 1;
    const int start_row_max = 1;
    const int col_row_increment = 1;
    const int m_max = 4;
    const int n_max = 4;
    const int k_mak = 4;
    const int dim_increment = 1;
    const int dim_start = 1;
#endif

    for (int var = 0; var < test_varations; var++) {
        // C Matrix starting row for sub-matrix
        for (int C_start_row = start_row_min; C_start_row <= start_row_max; C_start_row += col_row_increment) {

            // C Matrix starting col for sub-matrix
            for (int C_start_col = start_col_min; C_start_col <= start_col_max; C_start_col += col_row_increment) {

                // A Matrix starting row for sub-matrix
                for (int A_start_row = start_row_min; A_start_row <= start_row_max; A_start_row += col_row_increment) {

                    // A Matrix starting col for sub-matrix
                    for (int A_start_col = start_col_min; A_start_col <= start_col_max; A_start_col += col_row_increment) {

                        // B Matrix starting row for sub-matrix
                        for (int B_start_row = start_row_min; B_start_row <= start_row_max; B_start_row += col_row_increment) {

                            // B Matrix starting col for sub-matrix
                            for (int B_start_col = start_col_min; B_start_col <= start_col_max; B_start_col += col_row_increment) {

                                // sub-matrix m parameter
                                for (int m = dim_start; m <= m_max; m += dim_increment) {

                                    // sub-matrix n paramter
                                    for (int n = dim_start; n <= n_max; n += dim_increment) {

                                        // sub-matrix k parameter
                                        for (int k = dim_start; k <= k_mak; k += dim_increment) {

                                            test_data = {var, A_start_row, A_start_col, B_start_row, B_start_col, C_start_row, C_start_col, m, n, k};
                                            dspm_mult_ex_f32_aexx_functionality_in_cycle(&test_data);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        std::cout << var + 1 << "/" << test_varations << " of test done" << std::endl;
    }
}

static portMUX_TYPE testnlock = portMUX_INITIALIZER_UNLOCKED;

TEST_CASE("dspm_mult_ex_f32_aexx benchmark", "[dspm]")
{
    const int m = 4;
    const int n = 4;
    const int k = 4;
    const int start_row_col = 4;

    A_roi_rect.resizeRect(start_row_col, start_row_col, n, m);
    B_roi_rect.resizeRect(start_row_col, start_row_col, k, n);
    C_roi_rect.resizeRect(start_row_col, start_row_col, k, m);

    float *A_data = (float *)memalign(16, (m + (2 * start_row_col)) * (n + (2 * start_row_col)) * sizeof(float));
    float *B_data = (float *)memalign(16, (n + (2 * start_row_col)) * (k + (2 * start_row_col)) * sizeof(float));
    float *C_data = (float *)memalign(16, (m + (2 * start_row_col)) * (k + (2 * start_row_col)) * sizeof(float));

    dspm::Mat A(A_data, m + (2 * start_row_col), n + (2 * start_row_col));
    dspm::Mat B(B_data, n + (2 * start_row_col), k + (2 * start_row_col));
    dspm::Mat C(C_data, m + (2 * start_row_col), k + (2 * start_row_col));

    dspm::Mat A_subset = A.getROI(A_roi_rect);
    dspm::Mat B_subset = B.getROI(B_roi_rect);
    dspm::Mat C_subset = C.getROI(C_roi_rect);

    portENTER_CRITICAL(&testnlock);
    dspm_mult_ex_f32(A_subset.data, B_subset.data, C_subset.data, m, n, k, A_subset.padding, B_subset.padding, C_subset.padding);

    unsigned int start_b = dsp_get_cpu_cycle_count();
    int repeat_count = 1024;
    for (int i = 0 ; i < repeat_count ; i++) {
        dspm_mult_ex_f32(A_subset.data, B_subset.data, C_subset.data, m, n, k, A_subset.padding, B_subset.padding, C_subset.padding);
    }
    unsigned int end_b = dsp_get_cpu_cycle_count();
    portEXIT_CRITICAL(&testnlock);

    float total_b = end_b - start_b;
    float cycles = total_b / (repeat_count);
    printf("Benchmark dspm_mult_f32 - %f per multiplication 4x4 + overhead.\n", cycles);
    float min_exec = 100;
    float max_exec = 750;
    TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);

    free(A_data);
    free(B_data);
    free(C_data);
}
