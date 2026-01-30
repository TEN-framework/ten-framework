/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "unity.h"
#include "esp_log.h"

#include "esp_attr.h"
#include "dsp_tests.h"
#include "test_mat_common.h"

void test_assert_equal_mat_mat(dspm::Mat &m_expected, dspm::Mat &m_actual, const char *message)
{
    for (int row = 0; row < m_expected.rows; row++) {
        for (int col = 0; col < m_expected.cols; col++) {
            TEST_ASSERT_EQUAL_FLOAT_MESSAGE(m_expected(row, col), m_actual(row, col), message);
        }
    }
}

void test_assert_equal_mat_const(dspm::Mat &m_actual, float num, const char *message)
{
    for (int row = 0; row < m_actual.rows; row++) {
        for (int col = 0; col < m_actual.cols; col++) {
            TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num, m_actual(row, col), message);
        }
    }
}

void test_assert_check_area_mat_mat(dspm::Mat &m_origin, dspm::Mat &m_modified, int start_row, int start_col, const char *message)
{
    float *m_origin_ptr = m_origin.data;
    float *m_modified_ptr = m_modified.data;

    // set ptr of modified matrix back to the beginning
    const int ptr_shift = (start_row * m_origin.cols) + start_col;
    m_modified_ptr -= ptr_shift;
    const int end_of_matrix_space = m_origin.length - m_modified.length - ptr_shift - ((m_modified.rows - 1) * m_modified.padding);

    // original matrix area before the sub-matrix
    for (int index = 0; index < ptr_shift; index++) {
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(*m_origin_ptr, *m_modified_ptr, message);
        m_origin_ptr++;
        m_modified_ptr++;
    }

    // in and between the sub-matrix area
    for (int row = 0; row < m_modified.rows; row++) {
        // The actual sub-matrix (accessed area)
        for (int mat_col = 0; mat_col < m_modified.cols; mat_col++) {
            m_origin_ptr++;
            m_modified_ptr++;
        }

        // padding area
        if (row != (m_modified.rows - 1)) {     // skip padding after last row
            for (int padd_col = 0; padd_col < m_modified.padding; padd_col++) {
                TEST_ASSERT_EQUAL_FLOAT_MESSAGE(*m_origin_ptr, *m_modified_ptr, message);
                m_origin_ptr++;
                m_modified_ptr++;
            }
        }
    }

    // original matrix area after the sub-matrix
    for (int index = 0; index < end_of_matrix_space; index++) {
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(*m_origin_ptr, *m_modified_ptr, message);
        m_origin_ptr++;
        m_modified_ptr++;
    }
}
