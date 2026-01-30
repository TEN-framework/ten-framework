/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "unity.h"
#include "dsp_platform.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include "dspm_add.h"
#include "esp_attr.h"
#include "test_mat_common.h"
#include "dsp_tests.h"

TEST_CASE("dspm_add_f32_ansi functionality", "[dsps]")
{
    // create ROI rectangle
    dspm::Mat::Rect roi_rect;

    char message[60];
    for (int var = 0; var < 7; var++) {
        for (int start_row = 0; start_row < 2; start_row++) {
            for (int start_col = 0; start_col < 2; start_col++) {
                for (int row = 1; row < 6; row++) {
                    for (int col = 1; col < 6; col++) {
                        sprintf(message, "var = %d  s_row = %d  s_col = %d, row = %d, col = %d", var, start_row, start_col, row, col);
                        // create A B C matrices with row col dimensions + padding
                        // padding is from both sides of the targeted sub-matrix
                        // 1 1 1 1
                        // 1 x x 1
                        // 1 x x 1
                        // 1 1 1 1
                        dspm::Mat A(row + (2 * start_row), col + (2 * start_col));
                        dspm::Mat B(row + (2 * start_row), col + (2 * start_col));
                        dspm::Mat C_compare(row + (2 * start_row), col + (2 * start_col));
                        dspm::Mat C = dspm::Mat::ones(row + (2 * start_row), col + (2 * start_col));

                        // create A B C sub matrices with undefined dimensions
                        dspm::Mat A_sub;
                        dspm::Mat B_sub;
                        dspm::Mat C_sub;

                        // adjust ROI rectangles
                        roi_rect.resizeRect(start_col, start_row, col, row);

                        for (int i = 0; i < A.length; i++) {
                            A.data[i] = i + 1;
                            B.data[i] = i + 1;
                            C_compare.data[i] = (i + 1) * 2;
                        }

                        // Combinations of A B C matrices and sub-matrices are created for testing
                        // As an example: case 1
                        // Matrices A and C are sub-matrices - the data are defined as a pointer to an external buffer
                        // Matrix B is a matrix - the data are copied into the B matrix
                        switch (var) {
                        case 0: {
                            A_sub.CopyHead(A.getROI(roi_rect));    // A sub-matrix - NO DATA CPY
                            B_sub.CopyHead(B.getROI(roi_rect));    // B sub-matrix - NO DATA CPY
                            C_sub.CopyHead(C.getROI(roi_rect));    // C sub-matrix - NO DATA CPY
                        } break;
                        case 1: {
                            A_sub = A.Get(roi_rect);               // A matrix     - DATA CPY
                            B_sub.CopyHead(B.getROI(roi_rect));    // B sub_matirx - NO DATA CPY
                            C_sub.CopyHead(C.getROI(roi_rect));    // C sub_matirx - NO DATA CPY
                        } break;
                        case 2: {
                            A_sub.CopyHead(A.getROI(roi_rect));    // A sub-matrix - NO DATA CPY
                            B_sub = B.Get(roi_rect);               // B matrix     - DATA CPY
                            C_sub.CopyHead(C.getROI(roi_rect));    // C sub-matrix - NO DATA CPY
                        } break;
                        case 3: {
                            A_sub = A.Get(roi_rect);               // A matrix     - DATA CPY
                            B_sub = B.Get(roi_rect);               // B matrix     - DATA CPY
                            C_sub.CopyHead(C.getROI(roi_rect));    // C sub-matrix - NO DATA CPY
                        } break;
                        case 4: {
                            A_sub.CopyHead(A.getROI(roi_rect));    // A sub-matrix - NO DATA CPY
                            B_sub.CopyHead(B.getROI(roi_rect));    // B sub-matrix - NO DATA CPY
                            C_sub = C.Get(roi_rect);               // C matrix     - DATA CPY
                        } break;
                        case 5: {
                            A_sub.CopyHead(A.getROI(roi_rect));    // A sub-matrix - NO DATA CPY
                            B_sub = B.Get(roi_rect);               // B matrix     - DATA CPY
                            C_sub = C.Get(roi_rect);               // C matrix     - DATA CPY
                        } break;
                        case 6: {
                            A_sub = A.Get(roi_rect);               // A matrix     - DATA CPY
                            B_sub.CopyHead(B.getROI(roi_rect));    // B sub-matrix - NO DATA CPY
                            C_sub = C.Get(roi_rect);               // C matrix     - DATA CPY
                        } break;
                        default:
                            break;
                        }

                        dspm_add_f32_ansi(A_sub.data, B_sub.data, C_sub.data, row, col, A_sub.padding, B_sub.padding, C_sub.padding, 1, 1, 1);
                        dspm::Mat C_sub_check = C_compare.Get(roi_rect);

                        // C is a sub-matrix
                        if (C_sub.sub_matrix) {
                            // Create a copy of the original C matrix (filled with ones 1)
                            // to check if an area around the sub-matrix is unaffected after a matrix operation
                            dspm::Mat C_area_check = dspm::Mat::ones(row + (2 * start_row), col + (2 * start_col));
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

static portMUX_TYPE testnlock = portMUX_INITIALIZER_UNLOCKED;

TEST_CASE("dspm_add_f32_ansi benchmark", "[dsps]")
{
    const int dim = 4;
    const int M_off = 1;

    dspm::Mat mat(dim + M_off, dim + M_off);
    dspm::Mat mat_sub = mat.getROI(M_off, M_off, dim, dim);

    portENTER_CRITICAL(&testnlock);
    dspm_add_f32_ansi(mat_sub.data, mat_sub.data, mat_sub.data, dim, dim, mat_sub.padding, mat_sub.padding, mat_sub.padding, 1, 1, 1);

    unsigned int start_b = dsp_get_cpu_cycle_count();
    int repeat_count = 1024;
    for (int i = 0 ; i < repeat_count ; i++) {
        dspm_add_f32_ansi(mat_sub.data, mat_sub.data, mat_sub.data, dim, dim, mat_sub.padding, mat_sub.padding, mat_sub.padding, 1, 1, 1);
    }
    unsigned int end_b = dsp_get_cpu_cycle_count();
    portEXIT_CRITICAL(&testnlock);

    float total_b = end_b - start_b;
    float cycles = total_b / (repeat_count);
    printf("Benchmark dspm_add_f32_ansi - %f per sample %dx%d.\n", cycles, dim, dim);
    float min_exec = 100;
    float max_exec = 1400;
    TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, cycles);
}
