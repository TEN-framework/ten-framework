/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "unity.h"
#include "esp_dsp.h"
#include "dsp_platform.h"
#include "esp_log.h"

#include "dspm_mult.h"
#include "esp_attr.h"
#include "dsp_tests.h"
#include "test_mat_common.h"

TEST_CASE("dspm_mult_ex_f32_ansi functionality", "[dspm]")
{
    // create ROI rectangles
    dspm::Mat::Rect A_roi_rect;
    dspm::Mat::Rect B_roi_rect;
    dspm::Mat::Rect C_roi_rect;

    char message[60];
    for (int var = 0; var < 7; var++) {
        for (int start_row = 0; start_row < 2; start_row++) {
            for (int start_col = 0; start_col < 2; start_col++) {
                for (int m = 1; m < 6; m++) {
                    for (int n = 1; n < 6; n++) {
                        for (int k = 1; k < 6; k++) {
                            sprintf(message, "var = %d  s_row = %d  s_col = %d, m = %d, n = %d, k = %d", var, start_row, start_col, m, n, k);
                            // create A B C matrices with m n k dimensions + padding
                            // padding is from both sides of the targeted sub-matrix
                            // 1 1 1 1
                            // 1 x x 1
                            // 1 x x 1
                            // 1 1 1 1
                            dspm::Mat A(m + (2 * start_row), n + (2 * start_col));
                            dspm::Mat B(n + (2 * start_row), k + (2 * start_col));
                            dspm::Mat C = dspm::Mat::ones(m + (2 * start_row), k + (2 * start_col));

                            // create A B C sub matrices with undefined dimensions
                            dspm::Mat A_sub;
                            dspm::Mat B_sub;
                            dspm::Mat C_sub;

                            // adjust ROI rectangles
                            A_roi_rect.resizeRect(start_col, start_row, n, m);
                            B_roi_rect.resizeRect(start_col, start_row, k, n);
                            C_roi_rect.resizeRect(start_col, start_row, k, m);

                            // fill A B matrices with numbers
                            // fill C matrix with ones
                            for (int i = 0; i < A.length; i++) {
                                A.data[i] = i + 1;
                            }
                            for (int i = 0; i < B.length; i++) {
                                B.data[i] = i + 1;
                            }

                            // Combinations of A B C matrices and sub-matrices are created for testing
                            // As an example: case 1
                            // Matrices B and C are sub-matrices - the data are defined as a pointer to an external buffer
                            // Matrix B is a matrix - the data are copied into the B matrix
                            switch (var) {
                            case 0: {
                                A_sub.CopyHead(A.getROI(A_roi_rect));    // A sub-matrix - NO DATA CPY
                                B_sub.CopyHead(B.getROI(B_roi_rect));    // B sub-matrix - NO DATA CPY
                                C_sub.CopyHead(C.getROI(C_roi_rect));    // C sub-matrix - NO DATA CPY
                            } break;
                            case 1: {
                                A_sub = A.Get(A_roi_rect);               // A matrix     - DATA CPY
                                B_sub.CopyHead(B.getROI(B_roi_rect));    // B sub_matirx - NO DATA CPY
                                C_sub.CopyHead(C.getROI(C_roi_rect));    // C sub_matirx - NO DATA CPY
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
                                C_sub = C.Get(C_roi_rect);               // C matrix     - DATA CPY
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
                            dspm::Mat C_sub_check(m, k);

                            // Calculate C_sub_check = A_sub_check * B_sub_check
                            for (int i = 0 ; i < m ; i++) {
                                for (int j = 0 ; j < k ; j++) {
                                    C_sub_check(i, j) = 0;
                                    for (int s = 0 ; s < n ; s++) {
                                        C_sub_check(i, j) += A_sub_check(i, s) * B_sub_check(s, j);
                                    }
                                }
                            }

                            dspm_mult_ex_f32_ansi(A_sub.data, B_sub.data, C_sub.data, m, n, k, A_sub.padding, B_sub.padding, C_sub.padding);

                            // C is a sub-matrix
                            if (C_sub.sub_matrix) {
                                // Create a copy of the original C matrix (filled with ones 1)
                                // to check if an area around the sub-matrix is unaffected after a matrix operation
                                dspm::Mat C_area_check = dspm::Mat::ones(m + (2 * start_row), k + (2 * start_col));
                                test_assert_equal_mat_mat(C_sub_check, C_sub, message);
                                test_assert_check_area_mat_mat(C_area_check, C_sub, start_row, start_col, message);
                                // C is a matrix
                            } else {
                                test_assert_equal_mat_mat(C_sub_check, C_sub, message);
                            }
                        }
                    }
                }
            }
        }
    }
}

static portMUX_TYPE testnlock = portMUX_INITIALIZER_UNLOCKED;

TEST_CASE("dspm_mult_ex_f32_ansi benchmark", "[dspm]")
{
    const int m = 4;
    const int n = 4;
    const int k = 4;
    const int M_off = 1;

    dspm::Mat A(m + M_off, n + M_off);
    dspm::Mat B(n + M_off, k + M_off);
    dspm::Mat C(m + M_off, k + M_off);

    dspm::Mat A_subset = A.getROI(M_off, M_off, m, n);
    dspm::Mat B_subset = B.getROI(M_off, M_off, n, k);
    dspm::Mat C_subset = C.getROI(M_off, M_off, m, k);

    portENTER_CRITICAL(&testnlock);
    dspm_mult_ex_f32_ansi(A_subset.data, B_subset.data, C_subset.data, m, n, k, A_subset.padding, B_subset.padding, C_subset.padding);

    unsigned int start_b = dsp_get_cpu_cycle_count();
    int repeat_count = 1024;
    for (int i = 0 ; i < repeat_count ; i++) {
        dspm_mult_ex_f32_ansi(A_subset.data, B_subset.data, C_subset.data, m, n, k, A_subset.padding, B_subset.padding, C_subset.padding);
    }
    unsigned int end_b = dsp_get_cpu_cycle_count();
    portEXIT_CRITICAL(&testnlock);

    float total_b = end_b - start_b;
    float cycles = total_b / (repeat_count);
    printf("Benchmark dspm_mult_f32 - %f per multiplication 4x4 + overhead.\n", cycles);
    float min_exec = 100;
    float max_exec = 1400;
    TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);
}
