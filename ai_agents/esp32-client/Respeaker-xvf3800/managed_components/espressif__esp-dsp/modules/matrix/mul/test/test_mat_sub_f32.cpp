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
#include "mat.h"
#include "test_mat_common.h"

static const char *TAG = "[dspm]";

#define MAT_ROW 6       // test_matrix rows
#define MAT_COL 6       // test_matrix cols
#define ROI_ROW 4       // sub_matrix rows
#define ROI_COL 4       // sub_matrix cols
#define START_ROI 1     // start row/col dimension to create sub matrix from test matrix

dspm::Mat::Rect roi_rect(START_ROI, START_ROI, ROI_ROW, ROI_COL);

// matrix subset
TEST_CASE("Mat class matrix subset", TAG)
{
    float data[25] = {0, 1, 2, 3, 4,
                      5, 6, 7, 8, 9,
                      0, 1, 2, 3, 4,
                      5, 6, 7, 8, 9,
                      0, 1, 2, 3, 4
                     };

    // Test matrix dimensions
    const int m = 5;
    const int n = 5;

    dspm::Mat mat(data, m, n);
    std::cout << "Test matrix: rows: " << mat.rows << ", columns: " << mat.cols << std::endl;
    std::cout << mat << std::endl;

    // Sub matrix method 1 - sub-matrix dimensions
    int start_row = 1;
    int start_col = 1;
    int roi_rows = 4;
    int roi_cols = 3;

    // Create matrix subset as a shallow copy of mat matrix (no matrix data are copied)
    dspm::Mat mat_subset1 = mat.getROI(start_row, start_col, roi_rows, roi_cols);

    // Create matrix subset as a deep copy of mat matrix (matrix data are copied)
    dspm::Mat mat_subset1_check = mat.Get(start_row, roi_rows, start_col, roi_cols);

    std::cout << "Matrix subset, method 1: rows: " << mat_subset1.rows << ", columns: " << mat_subset1.cols << std::endl;
    std::cout << mat_subset1 << std::endl;

    // Compare the deep and the shallow copies
    test_assert_equal_mat_mat(mat_subset1_check, mat_subset1, "matrix subset 1");

    // Sub matrix method 2 - sub-matrix dimensions as a matrix rectangle
    int x = 1;
    int y = 1;
    int width = 4;
    int height = 3;

    // Create matrix ROI as a rectangle area
    dspm::Mat::Rect roi_rect(x, y, width, height);
    dspm::Mat mat_subset2 = mat.getROI(roi_rect);
    std::cout << "Matrix subset method 2: rows: " << mat_subset2.rows << ", columns: " << mat_subset2.cols << std::endl;
    std::cout << mat_subset2 << std::endl;
    dspm::Mat mat_subset2_check = mat.Get(roi_rect);

    test_assert_equal_mat_mat(mat_subset2_check, mat_subset2, "matrix subset 2");

    // Sub matrix method 2 - sub-matrix dimensions with specified stride
    start_row = 0;
    start_col = 1;
    roi_rows = 3;
    roi_cols = 3;
    int stride = 10;

    dspm::Mat mat_subset3 = mat.getROI(start_row, start_col, roi_rows, roi_cols, stride);
    std::cout << "Matrix subset method 3: rows: " << mat_subset1.rows << ", columns: " << mat_subset3.cols << std::endl;
    std::cout << mat_subset3 << std::endl;
    dspm::Mat mat_subset3_check = mat.Get(start_row, 5, start_col, roi_cols);

    for (int row = 0; row < mat_subset3_check.rows; row++) {
        if (row % 2) {
            continue;
        };
        for (int col =  0; col < mat_subset3_check.cols; col++) {
            TEST_ASSERT_EQUAL_FLOAT(mat_subset3_check(row, col), mat_subset3(row / 2, col));
        }
    }
}

static void test_mat_subset_operator_eq()
{
    dspm::Mat mat(2, 2);
    for (int i = 0; i < mat.length; i++) {
        mat.data[i] = 1;
    }

    dspm::Mat mat1(2, 2);
    for (int i = 0; i < mat1.length; i++) {
        mat1.data[i] = i + 1;
    }

    // matrices, dimensions are equal
    // mat(2, 2), mat1(2, 2)
    mat = mat1;
    TEST_ASSERT_EQUAL_INT(2, mat.rows);
    TEST_ASSERT_EQUAL_INT(mat1.rows, mat.rows);
    TEST_ASSERT_EQUAL_INT(2, mat.cols);
    TEST_ASSERT_EQUAL_INT(mat1.cols, mat.cols);
    test_assert_equal_mat_mat(mat1, mat, "=operator, mat = mat (equal dim)");

    dspm::Mat mat2(3, 3);
    for (int i = 0; i < mat2.length; i++) {
        mat2.data[i] = (i + 1) * 2;
    }

    // matrices, dimensions are not equal
    // mat1(2, 2), mat2(3, 3)
    mat1 = mat2;
    TEST_ASSERT_EQUAL_INT(3, mat1.rows);
    TEST_ASSERT_EQUAL_INT(mat2.rows, mat1.rows);
    TEST_ASSERT_EQUAL_INT(3, mat1.cols);
    TEST_ASSERT_EQUAL_INT(mat2.cols, mat1.cols);
    test_assert_equal_mat_mat(mat2, mat1, "=operator, mat = mat (not equal dim)");

    dspm::Mat mat3(4, 4);
    dspm::Mat mat4(4, 4);
    dspm::Mat mat4_compare(4, 4);
    for (int i = 0; i < mat3.length; i++) {
        mat3.data[i] = (i + 1) * 3;
        mat4.data[i] = (i + 1) * 4;
        mat4_compare.data[i] = (i + 1) * 4;
    }
    dspm::Mat mat3_sub_3x3 = mat3.getROI(1, 1, 3, 3);
    dspm::Mat mat3_sub_2x2 = mat3.getROI(1, 1, 2, 2);
    dspm::Mat mat3_mat_2x2 = mat3.Get(1, 2, 1, 2);

    // matrix and sub-matrix, dimensions are equal
    // mat1(3, 3), mat3_sub_3x3(3, 3)
    mat1 = mat3_sub_3x3;
    TEST_ASSERT_FALSE(mat1.sub_matrix);
    TEST_ASSERT_EQUAL_INT(3, mat1.rows);
    TEST_ASSERT_EQUAL_INT(mat3_sub_3x3.rows, mat1.rows);
    TEST_ASSERT_EQUAL_INT(3, mat1.cols);
    TEST_ASSERT_EQUAL_INT(mat3_sub_3x3.cols, mat1.cols);
    test_assert_equal_mat_mat(mat3_sub_3x3, mat1, "=operator, mat = sub_mat (equal dim)");

    dspm::Mat mat4_sub_2x2 = mat4.getROI(1, 1, 2, 2);
    dspm::Mat mat4_mat_2x2 = mat4.Get(1, 2, 1, 2);

    // matrix and sub-matrix, dimensions are not equal
    // mat1(3, 3), mat4_sub_2x2(2, 2)
    mat1 = mat4_sub_2x2;
    TEST_ASSERT_FALSE(mat1.sub_matrix);
    TEST_ASSERT_EQUAL_INT(2, mat1.rows);
    TEST_ASSERT_EQUAL_INT(mat4_sub_2x2.rows, mat1.rows);
    TEST_ASSERT_EQUAL_INT(2, mat1.cols);
    TEST_ASSERT_EQUAL_INT(mat4_sub_2x2.cols, mat1.cols);
    test_assert_equal_mat_mat(mat4_sub_2x2, mat1, "=operator, mat = sub_mat (not equal dim)");

    // sub-matrix and sub-matrix, dimensions are not equal
    // mat4_sub_2x2(2, 2), mat3_sub_3x3(3, 3)
    ESP_LOGI("=operator test", "following is an expected error message about matrices not having equal dimensions");
    mat4_sub_2x2 = mat3_sub_3x3;
    TEST_ASSERT_TRUE(mat4_sub_2x2.sub_matrix);
    TEST_ASSERT_EQUAL_INT(2, mat4_sub_2x2.rows);
    TEST_ASSERT_EQUAL_INT(2, mat4_sub_2x2.cols);
    test_assert_equal_mat_mat(mat4_mat_2x2, mat4_sub_2x2, "=operator, sub_mat = sub_mat (not equal dim)");
    test_assert_check_area_mat_mat(mat4_compare, mat4_sub_2x2, 1, 1, "=operator area, sub_mat = sub_mat (not equal dim)");

    // sub-matrix and sub-matrix, dimensions are equal
    // mat4_sub_2x2(2, 2), mat3_sub_2x2(2, 2)
    mat4_sub_2x2 = mat3_sub_2x2;
    TEST_ASSERT_TRUE(mat4_sub_2x2.sub_matrix);
    TEST_ASSERT_EQUAL_INT(2, mat4_sub_2x2.rows);
    TEST_ASSERT_EQUAL_INT(mat3_sub_2x2.rows, mat4_sub_2x2.rows);
    TEST_ASSERT_EQUAL_INT(2, mat4_sub_2x2.cols);
    TEST_ASSERT_EQUAL_INT(mat3_sub_2x2.cols, mat4_sub_2x2.cols);
    test_assert_equal_mat_mat(mat3_mat_2x2, mat4_sub_2x2, "=operator, sub_mat = sub_mat (equal dim)");
    test_assert_check_area_mat_mat(mat4_compare, mat4_sub_2x2, 1, 1, "=operator area, sub_mat = sub_mat (equal dim)");
}

// operator==
static void test_mat_subset_operator_eq_eq(void)
{
    dspm::Mat A(MAT_ROW, MAT_COL);
    dspm::Mat B(MAT_ROW, MAT_COL);

    for (int i = 0; i < A.length; i++) {
        A.data[i] = i;
        B.data[i] = i * 2;
    }

    dspm::Mat A_sub = A.getROI(roi_rect);
    dspm::Mat A_mat = A.Get(roi_rect);

    dspm::Mat B_sub = B.getROI(roi_rect);

    for (int row = 0; row < B_sub.rows; row++) {
        for (int col = 0; col < B_sub.cols; col++) {
            B_sub(row, col) = B_sub(row, col) / 2;
        }
    }
    dspm::Mat B_mat = B.Get(roi_rect);
    dspm::Mat B_mat_neq_cont = B_mat * 3;
    dspm::Mat B_mat_neq_dim(3, 3);

    TEST_ASSERT_TRUE(A_mat == B_mat);
    TEST_ASSERT_TRUE(A_sub == B_sub);
    TEST_ASSERT_TRUE(A_sub == B_mat);
    TEST_ASSERT_TRUE(A_mat == B_sub);
    ESP_LOGI("==operator test", "following is an expected error message about matrices not having equal content");
    TEST_ASSERT_FALSE(A_sub == B_mat_neq_cont);
    TEST_ASSERT_FALSE(A_sub == B_mat_neq_dim);
}

// operator/
static void test_mat_subset_operator_mat_div_mat(void)
{
    dspm::Mat mat(MAT_ROW, MAT_COL);
    for (int i = 0; i < mat.length; i++) {
        mat.data[i] = i;
    }

    dspm::Mat C = mat;
    dspm::Mat C_compare_area = mat;

    dspm::Mat A_sub = mat.getROI(roi_rect);
    dspm::Mat A_mat = mat.Get(roi_rect);

    dspm::Mat B_sub = mat.getROI(roi_rect);
    dspm::Mat B_mat = mat.Get(roi_rect);

    dspm::Mat C_sub = C.getROI(roi_rect);
    dspm::Mat C_mat = C.Get(roi_rect);
    dspm::Mat C_compare(ROI_ROW, ROI_COL);

    for (int i = 0; i < C_compare.length; i++) {
        C_compare.data[i] = A_mat.data[i] / B_mat.data[i];
    }

    C_mat = A_mat / B_mat;
    test_assert_equal_mat_mat(C_compare, C_mat, "/ operator, mat = mat / mat");

    C_mat = A_sub / B_sub;
    test_assert_equal_mat_mat(C_compare, C_mat, "/ operator, mat = sub_mat / sub_mat");

    C_mat = A_sub / B_mat;
    test_assert_equal_mat_mat(C_compare, C_mat, "/ operator, mat = sub_mat / mat");

    C_mat = A_mat / B_sub;
    test_assert_equal_mat_mat(C_compare, C_mat, "/ operator, mat = mat / sub_mat");

    C_sub = A_sub / B_sub;
    test_assert_equal_mat_mat(C_compare, C_sub, "/ operator, sub_mat = sub_mat / sub_mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "/ operator, area check, sub_mat = sub_mat / sub_mat");

    C_sub = A_mat / B_sub;
    test_assert_equal_mat_mat(C_compare, C_sub, "/ operator, sub_mat = mat / sub_mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "/ operator, area check, sub_mat = sub_mat / sub_mat");

    C = mat;
    C_mat = C.Get(roi_rect);    // C_mat must be refreshed
    C_mat /= A_mat;
    test_assert_equal_mat_mat(C_compare, C_mat, "/ operator, mat /= mat");

    C = mat;
    C_mat = C.Get(roi_rect);    // C_mat must be refreshed
    C_mat /= A_sub;
    test_assert_equal_mat_mat(C_compare, C_mat, "/ operator, mat /= sub_mat");

    C = mat;                    // C must be refreshed, to refresh the C_sub
    C_sub /= A_mat;
    test_assert_equal_mat_mat(C_compare, C_sub, "/ operator, sub_mat /= mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "/ operator, area check, sub_mat /= mat");

    C = mat;                    // C must be refreshed, to refresh the C_sub
    C_sub /= A_sub;
    test_assert_equal_mat_mat(C_compare, C_sub, "/ operator, sub_mat /= sub_mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "/ operator, area check, sub_mat /= sub_mat");
}

// operator^
static void test_mat_subset_operator_xor(void)
{
    dspm::Mat mat(5, 5);
    dspm::Mat mat_area_check(5, 5);
    for (int i = 0; i < mat.length; i++) {
        mat.data[i] = i;
        mat_area_check.data[i] = i;
    }

    dspm::Mat::Rect roi_rect(1, 1, 3, 3);
    dspm::Mat mat_mat = mat.Get(roi_rect);
    dspm::Mat mat_sub = mat.getROI(roi_rect);

    // XOR 0
    dspm::Mat res_mat = mat_mat ^ 0;
    dspm::Mat res_sub = mat_sub ^ 0;
    test_assert_equal_mat_mat(res_mat, res_sub, "sub-matrix operator^ 0");
    test_assert_check_area_mat_mat(mat_area_check, mat_sub, 1, 1, "sub-matrix area check operator^ 0");

    // XOR 1
    res_mat = mat_mat ^ 1;
    res_sub = mat_sub ^ 1;
    test_assert_equal_mat_mat(res_mat, res_sub, "sub-matrix operator^ 1");
    test_assert_check_area_mat_mat(mat_area_check, mat_sub, 1, 1, "sub-matrix area check operator^ 1");

    // XOR even
    res_mat = mat_mat ^ 2;
    res_sub = mat_sub ^ 2;
    test_assert_equal_mat_mat(res_mat, res_sub, "sub-matrix operator^ 2");
    test_assert_check_area_mat_mat(mat_area_check, mat_sub, 1, 1, "sub-matrix area check operator^ 2");

    // XOR odd
    res_mat = mat_mat ^ 3;
    res_sub = mat_sub ^ 3;
    test_assert_equal_mat_mat(res_mat, res_sub, "sub-matrix operator^ 3");
    test_assert_check_area_mat_mat(mat_area_check, mat_sub, 1, 1, "sub-matrix area check operator^ 3");
}

// operator/
static void test_mat_subset_operator_mat_div_const(void)
{
    const float div_const = 2;

    dspm::Mat mat(MAT_ROW, MAT_COL);
    for (int i = 0; i < mat.length; i++) {
        mat.data[i] = i;
    }

    dspm::Mat C_compare_area = mat;
    dspm::Mat C = mat;

    dspm::Mat A_sub = mat.getROI(roi_rect);
    dspm::Mat A_mat = mat.Get(roi_rect);

    dspm::Mat C_sub = C.getROI(roi_rect);
    dspm::Mat C_mat = C.Get(roi_rect);
    dspm::Mat C_compare = mat.Get(roi_rect);

    for (int i = 0; i < C_compare.length; i++) {
        C_compare.data[i] /= div_const;
    }

    C_mat = A_mat / div_const;
    test_assert_equal_mat_mat(C_compare, C_mat, "/ operator, mat = mat / const");

    C_mat = A_sub / div_const;
    test_assert_equal_mat_mat(C_compare, C_mat, "/ operator, mat = sub_mat / const");
    C_mat = C.Get(roi_rect);

    C_mat /= div_const;
    test_assert_equal_mat_mat(C_compare, C_mat, "/ operator, mat /= const");

    C_sub = A_mat / div_const;
    test_assert_equal_mat_mat(C_compare, C_sub, "/ operator, sub_mat = mat / const");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "/ operator, area check, sub_mat = mat / const");

    C = mat;
    C_sub /= div_const;
    test_assert_equal_mat_mat(C_compare, C_sub, "/ operator, sub_mat /= const");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "/ operator, area check, sub_mat /= const");
}

// operator-
static void test_mat_subset_operator_mat_sub_const(void)
{
    const float sub_const = 2;

    dspm::Mat mat(MAT_ROW, MAT_COL);
    for (int i = 0; i < mat.length; i++) {
        mat.data[i] = i;
    }

    dspm::Mat C_compare_area = mat;
    dspm::Mat C = mat;

    dspm::Mat A_sub = mat.getROI(roi_rect);
    dspm::Mat A_mat = mat.Get(roi_rect);

    dspm::Mat C_sub = C.getROI(roi_rect);
    dspm::Mat C_mat = C.Get(roi_rect);
    dspm::Mat C_compare = mat.Get(roi_rect);

    for (int i = 0; i < C_compare.length; i++) {
        C_compare.data[i] -= sub_const;
    }

    C_mat = A_mat - sub_const;
    test_assert_equal_mat_mat(C_compare, C_mat, "- operator, mat = mat - const");

    C_mat = A_sub - sub_const;
    test_assert_equal_mat_mat(C_compare, C_mat, "- operator, mat = sub_mat - const");
    C_mat = C.Get(roi_rect);

    C_mat -= sub_const;
    test_assert_equal_mat_mat(C_compare, C_mat, "- operator, mat -= const");

    C_sub = A_mat - sub_const;
    test_assert_equal_mat_mat(C_compare, C_sub, "- operator, sub_mat = mat - const");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "- operator, area check, sub_mat = mat - const");

    C = mat;
    C_sub -= sub_const;
    test_assert_equal_mat_mat(C_compare, C_sub, "- operator, sub_mat -= const");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "- operator, area check, sub_mat -= const");
}

// operator-
static void test_mat_subset_operator_mat_sub_mat(void)
{
    dspm::Mat mat(MAT_ROW, MAT_COL);
    for (int i = 0; i < mat.length; i++) {
        mat.data[i] = i;
    }

    dspm::Mat C = mat;
    dspm::Mat C_compare_area = mat;

    dspm::Mat A_sub = mat.getROI(roi_rect);
    dspm::Mat A_mat = mat.Get(roi_rect);

    dspm::Mat B_sub = mat.getROI(roi_rect);
    dspm::Mat B_mat = mat.Get(roi_rect);

    dspm::Mat C_sub = C.getROI(roi_rect);
    dspm::Mat C_mat = C.Get(roi_rect);
    dspm::Mat C_compare(ROI_ROW, ROI_COL);

    for (int i = 0; i < C_compare.length; i++) {
        C_compare.data[i] = A_mat.data[i] - B_mat.data[i];
    }

    C_mat = A_mat - B_mat;
    test_assert_equal_mat_mat(C_compare, C_mat, "- operator, mat = mat - mat");

    C_mat = A_sub - B_sub;
    test_assert_equal_mat_mat(C_compare, C_mat, "- operator, mat = sub_mat - sub_mat");

    C_mat = A_sub - B_mat;
    test_assert_equal_mat_mat(C_compare, C_mat, "- operator, mat = sub_mat - mat");

    C_mat = A_mat - B_sub;
    test_assert_equal_mat_mat(C_compare, C_mat, "- operator, mat = mat - sub_mat");

    C_sub = A_sub - B_sub;
    test_assert_equal_mat_mat(C_compare, C_sub, "- operator, sub_mat = sub_mat - sub_mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "- operator, area check, sub_mat = sub_mat - sub_mat");

    C_sub = A_mat - B_sub;
    test_assert_equal_mat_mat(C_compare, C_sub, "- operator, sub_mat = mat - sub_mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "- operator, area check, sub_mat = sub_mat - sub_mat");

    C = mat;
    C_mat = C.Get(roi_rect);    // C_mat must be refreshed
    C_mat -= A_mat;
    test_assert_equal_mat_mat(C_compare, C_mat, "- operator, mat -= mat");

    C = mat;
    C_mat = C.Get(roi_rect);    // C_mat must be refreshed
    C_mat -= A_sub;
    test_assert_equal_mat_mat(C_compare, C_mat, "- operator, mat -= sub_mat");

    C = mat;                    // C must be refreshed, to refresh the C_sub
    C_sub -= A_mat;
    test_assert_equal_mat_mat(C_compare, C_sub, "- operator, sub_mat -= mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "- operator, area check, sub_mat -= mat");

    C = mat;                    // C must be refreshed, to refresh the C_sub
    C_sub -= A_sub;
    test_assert_equal_mat_mat(C_compare, C_sub, "- operator, sub_mat -= sub_mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "- operator, area check, sub_mat -= sub_mat");
}

// operator+
static void test_mat_subset_operator_mat_add_mat(void)
{
    dspm::Mat mat(MAT_ROW, MAT_COL);
    for (int i = 0; i < mat.length; i++) {
        mat.data[i] = i;
    }

    dspm::Mat C = mat;
    dspm::Mat C_compare_area = mat;

    dspm::Mat A_sub = mat.getROI(roi_rect);
    dspm::Mat A_mat = mat.Get(roi_rect);

    dspm::Mat B_sub = mat.getROI(roi_rect);
    dspm::Mat B_mat = mat.Get(roi_rect);

    dspm::Mat C_sub = C.getROI(roi_rect);
    dspm::Mat C_mat = C.Get(roi_rect);
    dspm::Mat C_compare(ROI_ROW, ROI_COL);

    for (int i = 0; i < C_compare.length; i++) {
        C_compare.data[i] = A_mat.data[i] + B_mat.data[i];
    }

    C_mat = A_mat + B_mat;
    test_assert_equal_mat_mat(C_compare, C_mat, "+ operator, mat = mat + mat");

    C_mat = A_sub + B_sub;
    test_assert_equal_mat_mat(C_compare, C_mat, "+ operator, mat = sub_mat + sub_mat");

    C_mat = A_sub + B_mat;
    test_assert_equal_mat_mat(C_compare, C_mat, "+ operator, mat = sub_mat + mat");

    C_sub = A_sub + B_sub;
    test_assert_equal_mat_mat(C_compare, C_sub, "+ operator, sub_mat = sub_mat + sub_mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "+ operator, area check, sub_mat = sub_mat + sub_mat");

    C_sub = A_mat + B_sub;
    test_assert_equal_mat_mat(C_compare, C_sub, "+ operator, sub_mat = mat + sub_mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "+ operator, area check, sub_mat = sub_mat + sub_mat");

    C = mat;
    C_mat = C.Get(roi_rect);    // C_mat must be refreshed
    C_mat += A_mat;
    test_assert_equal_mat_mat(C_compare, C_mat, "+ operator, mat += mat");

    C = mat;
    C_mat = C.Get(roi_rect);    // C_mat must be refreshed
    C_mat += A_sub;
    test_assert_equal_mat_mat(C_compare, C_mat, "+ operator, mat += sub_mat");

    C = mat;                    // C must be refreshed, to refresh the C_sub
    C_sub += A_mat;
    test_assert_equal_mat_mat(C_compare, C_sub, "+ operator, sub_mat += mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "+ operator, area check, sub_mat += mat");

    C = mat;                    // C must be refreshed, to refresh the C_sub
    C_sub += A_sub;
    test_assert_equal_mat_mat(C_compare, C_sub, "+ operator, sub_mat += sub_mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "+ operator, area check, sub_mat += sub_mat");
}

// operator+
static void test_mat_subset_operator_mat_add_const(void)
{
    const float add_const = 2;

    dspm::Mat mat(MAT_ROW, MAT_COL);
    for (int i = 0; i < mat.length; i++) {
        mat.data[i] = i;
    }

    dspm::Mat C_compare_area = mat;
    dspm::Mat C = mat;

    dspm::Mat A_sub = mat.getROI(roi_rect);
    dspm::Mat A_mat = mat.Get(roi_rect);

    dspm::Mat C_sub = C.getROI(roi_rect);
    dspm::Mat C_mat = C.Get(roi_rect);
    dspm::Mat C_compare = mat.Get(roi_rect);

    for (int i = 0; i < C_compare.length; i++) {
        C_compare.data[i] += add_const;
    }

    C_mat = A_sub + add_const;
    test_assert_equal_mat_mat(C_compare, C_mat, "+ operator, mat = sub_mat + const");
    C_mat = C.Get(roi_rect);

    C_mat += add_const;
    test_assert_equal_mat_mat(C_compare, C_mat, "+ operator, mat += const");

    C_sub = A_mat + add_const;
    test_assert_equal_mat_mat(C_compare, C_sub, "+ operator, sub_mat = mat + const");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "+ operator, area check, sub_mat = mat + const");

    C = mat;
    C_sub += add_const;
    test_assert_equal_mat_mat(C_compare, C_sub, "+ operator, sub_mat += const");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "+ operator, area check, sub_mat += const");
}

// operator*
static void test_mat_subset_operator_mat_mul_const(void)
{
    const float mul_const = 2;

    dspm::Mat mat(MAT_ROW, MAT_COL);
    for (int i = 0; i < mat.length; i++) {
        mat.data[i] = i;
    }

    dspm::Mat C_compare_area = mat;
    dspm::Mat C = mat;

    dspm::Mat A_sub = mat.getROI(roi_rect);
    dspm::Mat A_mat = mat.Get(roi_rect);

    dspm::Mat C_sub = C.getROI(roi_rect);
    dspm::Mat C_mat = C.Get(roi_rect);
    dspm::Mat C_compare = mat.Get(roi_rect);

    for (int i = 0; i < C_compare.length; i++) {
        C_compare.data[i] *= mul_const;
    }

    C_mat = A_mat * mul_const;
    test_assert_equal_mat_mat(C_compare, C_mat, "* operator, mat = mat * const");

    C_mat = A_sub * mul_const;
    test_assert_equal_mat_mat(C_compare, C_mat, "* operator, mat = sub_mat * const");
    C_mat = C.Get(roi_rect);

    C_mat *= mul_const;
    test_assert_equal_mat_mat(C_compare, C_mat, "* operator, mat *= const");

    C_sub = A_mat * mul_const;
    test_assert_equal_mat_mat(C_compare, C_sub, "* operator, sub_mat = mat * const");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "* operator, area check, sub_mat = mat * const");

    C = mat;
    C_sub *= mul_const;
    test_assert_equal_mat_mat(C_compare, C_sub, "* operator, sub_mat *= const");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "* operator, area check, sub_mat *= const");
}

// operator*
static void test_mat_subset_operator_mat_mul_mat_2(void)
{
    dspm::Mat mat(MAT_ROW, MAT_COL);
    for (int i = 0; i < mat.length; i++) {
        mat.data[i] = i;
    }

    dspm::Mat C_compare_area = mat;
    dspm::Mat C = mat;

    const int m = 4, n = 4, k = 4;
    dspm::Mat::Rect roi_rect_mul(1, 1, k, m);

    dspm::Mat A_sub = mat.getROI(roi_rect_mul);
    dspm::Mat A_mat = mat.Get(roi_rect_mul);

    dspm::Mat C_sub = C.getROI(roi_rect_mul);
    dspm::Mat C_mat = C.Get(roi_rect_mul);
    dspm::Mat C_compare = dspm::Mat::ones(m, k);

    for (int i = 0 ; i < m ; i++) {
        for (int j = 0 ; j < k ; j++) {
            C_compare.data[(i * k) + j] = 0;
            for (int s = 0 ; s < n ; s++) {
                C_compare.data[(i * k) + j] += A_mat.data[(i * n) + s] * C_mat.data[(s * k) + j];
            }
        }
    }

    C_mat *= A_mat;
    test_assert_equal_mat_mat(C_compare, C_mat, "*= operator, mat *= mat");
    C_mat = C.Get(roi_rect_mul);

    C_mat *= A_sub;
    test_assert_equal_mat_mat(C_compare, C_mat, "*= operator, mat *= sub_mat");

    C_sub *= A_sub;
    test_assert_equal_mat_mat(C_compare, C_sub, "*= operator, sub_mat *= sub_mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "*= operator, area check, sub_mat *= sub_mat");

    C = mat;
    C_sub *= A_mat;
    test_assert_equal_mat_mat(C_compare, C_sub, "*= operator, sub_mat *= sub_mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "*= operator, area check, sub_mat *= sub_mat");
}

// operator*
static void test_mat_subset_operator_mat_mul_mat_1(void)
{
    dspm::Mat mat(MAT_ROW, MAT_COL);
    for (int i = 0; i < mat.length; i++) {
        mat.data[i] = i;
    }

    dspm::Mat C = dspm::Mat::ones(6);
    dspm::Mat C_compare_area = dspm::Mat::ones(6);

    // matrix dimensions
    const int m = 4, n = 3, k = 4;
    dspm::Mat::Rect A_roi_rect(2, 1, n, m);
    dspm::Mat::Rect B_roi_rect(1, 2, k, n);
    dspm::Mat::Rect C_roi_rect(1, 1, k, m);

    dspm::Mat A_sub = mat.getROI(A_roi_rect);
    dspm::Mat A_mat = mat.Get(A_roi_rect);

    dspm::Mat B_sub = mat.getROI(B_roi_rect);
    dspm::Mat B_mat = mat.Get(B_roi_rect);

    dspm::Mat C_sub = C.getROI(C_roi_rect);
    dspm::Mat C_mat = C.Get(C_roi_rect);
    dspm::Mat C_compare = dspm::Mat::ones(m, k);

    for (int i = 0 ; i < m ; i++) {
        for (int j = 0 ; j < k ; j++) {
            C_compare.data[(i * k) + j] = 0;
            for (int s = 0 ; s < n ; s++) {
                C_compare.data[(i * k) + j] += A_mat.data[(i * n) + s] * B_mat.data[(s * k) + j];
            }
        }
    }

    C_mat = A_mat * B_mat;
    test_assert_equal_mat_mat(C_compare, C_mat, "* operator, mat = mat * mat");

    C_mat = A_sub * B_sub;
    test_assert_equal_mat_mat(C_compare, C_mat, "* operator, mat = sub_mat * sub_mat");

    C_mat = A_sub * B_mat;
    test_assert_equal_mat_mat(C_compare, C_mat, "* operator, mat = sub_mat * mat");

    C_sub = A_sub * B_sub;
    test_assert_equal_mat_mat(C_compare, C_sub, "* operator, sub_mat = sub_mat * sub_mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "* operator, area check, sub_mat = sub_mat * sub_mat");

    C_sub = A_mat * B_sub;
    test_assert_equal_mat_mat(C_compare, C_sub, "*operator, sub_mat = mat * sub_mat");
    test_assert_check_area_mat_mat(C_compare_area, C_sub, START_ROI, START_ROI,  "* operator, area check, sub_mat = sub_mat * sub_mat");
}

TEST_CASE("Matrix subset operators", TAG)
{
    test_mat_subset_operator_eq();                  // mat = mat
    test_mat_subset_operator_eq_eq();               // mat == mat
    test_mat_subset_operator_xor();                 // mat ^ const
    test_mat_subset_operator_mat_mul_mat_1();       // mat * mat
    test_mat_subset_operator_mat_mul_mat_2();       // mat * mat
    test_mat_subset_operator_mat_mul_const();       // mat * const
    test_mat_subset_operator_mat_add_mat();         // mat + mat
    test_mat_subset_operator_mat_add_const();       // mat + const
    test_mat_subset_operator_mat_sub_mat();         // mat - mat
    test_mat_subset_operator_mat_sub_const();       // mat - const
    test_mat_subset_operator_mat_div_mat();         // mat / mat
    test_mat_subset_operator_mat_div_const();       // mat / const
}

static void test_mat_subset_solve(void)
{
    int m = 3;
    int n = 3;
    float data_a[9] = {3, 2, 1, 2, 3, 1, 2, 1, 3};
    float data_b[9] = {5, -1, 4};
    dspm::Mat A(data_a, m, n);
    dspm::Mat b(data_b, m, 1);

    dspm::Mat A_origin = dspm::Mat::ones(5);
    dspm::Mat b_origin = dspm::Mat::ones(5, 3);
    dspm::Mat A_origin_area_check = dspm::Mat::ones(5);
    dspm::Mat b_origin_area_check = dspm::Mat::ones(5, 3);

    A_origin.Copy(A, 1, 1);
    b_origin.Copy(b, 1, 1);

    // create sub-matrices
    dspm::Mat A_sub = A_origin.getROI(1, 1, m, n);
    dspm::Mat b_sub = b_origin.getROI(1, 1, m, 1);

    dspm::Mat x1 = dspm::Mat::solve(A_sub, b_sub);
    test_assert_check_area_mat_mat(A_origin_area_check, A_sub, 1, 1, "check solve, area A");
    test_assert_check_area_mat_mat(b_origin_area_check, b_sub, 1, 1, "check solve, area b");

    std::cout << "Solve result matrix: rows: " << x1.rows << ", columns: " << x1.cols << std::endl;
    std::cout << (x1 * 12).t();
    dspm::Mat x2 = dspm::Mat::roots(A_sub, b_sub);
    test_assert_check_area_mat_mat(A_origin_area_check, A_sub, 1, 1, "check solve, area A");
    test_assert_check_area_mat_mat(b_origin_area_check, b_sub, 1, 1, "check solve, area b");

    std::cout << "Roots result matrix: rows: " << x2.rows << ", columns: " << x2.cols << std::endl;
    std::cout << (x2 * 12).t();
    dspm::Mat diff_b = x1 - x2;
    std::cout << "Difference between solve() abd roots(): " << diff_b.t();
    for (int row = 0; row < diff_b.rows; row++) {
        for (int col = 0; col < diff_b.cols; col++) {
            if (fabs(diff_b(row, col)) > 0.000001) {
                TEST_ASSERT_MESSAGE (false, "Calculation is incorrect! Error more then expected!");
            }
        }
    }
}

static void test_mat_subset_inverse(void)
{
    // Test inverse()
    dspm::Mat result;
    float m_data[] = {2, 5, 7,
                      6, 3, 4,
                      5, -2, -3
                     };
    float m_result[] = {  1.0000,   -1.0000,    1.0000,
                          -38.0000,   41.0000,  -34.0000,
                          27.0000,  -29.0000,   24.0000
                       };

    result = dspm::Mat(m_data, 3, 3);

    dspm::Mat result_origin = dspm::Mat::ones(5);
    dspm::Mat result_origin_area_check = dspm::Mat::ones(5);

    result_origin.Copy(result, 1, 1);
    dspm::Mat result_sub = result_origin.getROI(1, 1, 3, 3);

    result = result_sub.inverse();
    test_assert_check_area_mat_mat(result_origin_area_check, result_sub, 1, 1, "area check inverse");

    std::cout << "inverse: " << std::endl;
    std::cout << result << std::endl;
    for (int i = 0; i < 3 * 3; i++) {
        if (std::abs(result.data[i] - m_result[i]) > 1e-4) {
            printf("Error at[%i] = %f, expected= %f, calculated = %f\n", i, std::abs(result.data[i] - m_result[i]), m_result[i], result.data[i]);
            TEST_ASSERT_MESSAGE (false, "Error in inverse() operation!\n");
        }
    }
    result = dspm::Mat(m_data, 3, 3);
    result_origin = dspm::Mat::ones(5);
    result_origin.Copy(result, 1, 1);
    result_sub = result_origin.getROI(1, 1, 3, 3);

    result = result_sub.pinv();
    test_assert_check_area_mat_mat(result_origin_area_check, result_sub, 1, 1, "area check pinv");

    std::cout << "pinv: " << std::endl;
    std::cout << result << std::endl;
    for (int i = 0; i < 3 * 3; i++) {
        if (std::abs(result.data[i] - m_result[i]) > 1e-2) {
            printf("Error at[%i] = %f, expected= %f, calculated = %f \n", i, std::abs(result.data[i] - m_result[i]), m_result[i], result.data[i]);
            TEST_ASSERT_MESSAGE (false, "Error in pinv() operation!\n");
        }
    }
}

static void test_mat_subset_normalize(void)
{
    dspm::Mat result_origin = dspm::Mat::ones(4);
    dspm::Mat result_area_check = dspm::Mat::ones(4);
    dspm::Mat result_sub = result_origin.getROI(1, 1, 2, 2);

    std::cout << "Befor normalize: " << std::endl;
    std::cout << result_sub << std::endl;
    result_sub.normalize();
    test_assert_check_area_mat_mat(result_area_check, result_sub, 1, 1, "normalize area check");
    std::cout << "normalize: " << std::endl;
    std::cout << result_sub << std::endl;

    for (int row = 0; row < result_sub.rows; row++) {
        for (int col = 0 ; col < result_sub.cols ; col++) {
            if (std::abs(result_sub(row, col) - 0.5) > dspm::Mat::abs_tol) {
                ESP_LOGE(TAG, "Error bigger then expected: %f", std::abs(result_sub(row, col) - 0.5));
                TEST_ASSERT_MESSAGE (false, "Error in normalize() operation! ");
            }
        }
    }
}

static void test_mat_subset_swap_trans_dot_clear(void)
{
    dspm::Mat mat(5, 5);
    dspm::Mat mat_area_check(5, 5);
    for (int row = 0; row < mat.rows; row++) {
        for (int col = 0; col < mat.cols; col++) {
            mat(row, col) = row + 1;
            mat_area_check(row, col) = row + 1;
        }
    }

    dspm::Mat::Rect roi_rect(1, 1, 3, 3);
    dspm::Mat mat_sub = mat.getROI(roi_rect);
    dspm::Mat mat_mat = mat.Get(roi_rect);

    // check swap rows
    mat_sub.swapRows(0, 1);
    mat_mat.swapRows(0, 1);
    test_assert_equal_mat_mat(mat_sub, mat_mat, "sub-matrix swapRows");
    test_assert_check_area_mat_mat(mat_area_check, mat_sub, 1, 1, "area check sub-matrix swapRows");

    // check transpose
    dspm::Mat mat_sub_res = mat_sub.t();
    dspm::Mat mat_mat_res = mat_mat.t();
    test_assert_equal_mat_mat(mat_mat_res, mat_sub_res, "sub-matrix transpose");
    test_assert_check_area_mat_mat(mat_area_check, mat_sub, 1, 1, "area check sub-matrix transpose");

    // check dot product
    float dot_mat = dspm::Mat::dotProduct(mat_mat, mat_mat);
    float dot_sub = dspm::Mat::dotProduct(mat_sub, mat_sub);
    TEST_ASSERT_EQUAL_FLOAT(dot_mat, dot_sub);

    // check clear
    mat_sub.clear();
    mat_mat.clear();
    test_assert_equal_mat_const(mat_sub, 0, "sub-matrix clear");
    test_assert_equal_mat_mat(mat_mat, mat_sub, "sub-matrix clear");
    test_assert_check_area_mat_mat(mat_area_check, mat_sub, 1, 1, "area check sub-matrix clear");
}


TEST_CASE("Matrix subset methods check", TAG)
{
    test_mat_subset_solve();
    test_mat_subset_inverse();
    test_mat_subset_normalize();
    test_mat_subset_swap_trans_dot_clear();
}
