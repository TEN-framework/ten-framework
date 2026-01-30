/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <malloc.h>
#include <math.h>
#include "graphics_support.h"


void init_perspective_matrix(dspm::Mat &P_m)
{
    const float fov = 90;                           // field of view in degrees
    const float near = 0.0001;
    const float far = 1;

    const float S = 1 / (tan((fov / 2) * DEG_TO_RAD));

    // Initialize matrix to zero
    for (int row = 0; row < P_m.rows; row++) {
        for (int col = 0; col < P_m.cols; col++) {
            P_m(row, col) = 0;
        }
    }

    P_m(0, 0) = S;
    P_m(1, 1) = S;
    P_m(2, 2) = -far / (far - near);
    P_m(3, 2) = (-far * near) / (far - near);
    P_m(2, 3) = -1;
    P_m(3, 3) = 1;
}

void update_perspective_matrix(dspm::Mat &P_m, float fov)
{
    const float near = 0.0001;
    const float far = 1;

    const float S = 1 / (tan((fov / 2) * DEG_TO_RAD));

    // Initialize matrix to zero
    for (int row = 0; row < P_m.rows; row++) {
        for (int col = 0; col < P_m.cols; col++) {
            P_m(row, col) = 0;
        }
    }

    P_m(0, 0) = S;
    P_m(1, 1) = S;
    P_m(2, 2) = -far / (far - near);
    P_m(3, 2) = (-far * near) / (far - near);
    P_m(2, 3) = -1;
    P_m(3, 3) = 1;

    P_m(3, 0) = (float)SSD1606_X_CENTER;
    P_m(3, 1) = (float)SSD1606_Y_CENTER;
}

void update_scaling_matrix(dspm::Mat &T_m, bool keep_diagonal, float scale_x, float scale_y, float scale_z)
{
    if (keep_diagonal) {                 // Multiply the diagonal values of the transformation matrix with the scaling
        T_m(0, 0) *= scale_x;            // (in case a rotation is already applied)
        T_m(1, 1) *= scale_y;
        T_m(2, 2) *= scale_z;
    } else {
        T_m(0, 0) = scale_x;
        T_m(1, 1) = scale_y;
        T_m(2, 2) = scale_z;
    }
}

void update_translation_matrix(dspm::Mat &T_m, bool row, float move_x, float move_y, float move_z)
{
    if (row) {                      // update values in 4-th row, if translation matrix is the second multiplier
        T_m(3, 0) = move_x;
        T_m(3, 1) = move_y;
        T_m(3, 2) = move_z;
    } else {                        // update values in 4-th collum, if translation matrix is the first multiplier
        T_m(0, 3) = move_x;
        T_m(1, 3) = move_y;
        T_m(2, 3) = move_z;
    }
}


void update_rotation_matrix(dspm::Mat &T_m, float rot_x, float rot_y, float rot_z)
{
    dspm::Mat rotation_data(3, 1);                      // matrix(3x1) that holds x, y, z rotation data
    rotation_data(0, 0) = DEG_TO_RAD * rot_x;           // rotation data x
    rotation_data(1, 0) = DEG_TO_RAD * rot_y;           // rotation data y
    rotation_data(2, 0) = DEG_TO_RAD * rot_z;           // rotation data z

    // Create and populate rotation matrix R(3x3). Then inverse it
    dspm::Mat R = ekf::eul2rotm(rotation_data.data).t();

    // Enlarge rotation matrix from 3x3 to 4x4
    for (int row = 0; row < R.rows; row++) {            // Copy rotation matrix R(3x3) to transformation matrix T_m(4x4)
        for (int col = 0; col < R.cols; col++) {
            T_m(row, col) = R(row, col);
        }
    }
}

void print_matrix(dspm::Mat matrix)
{
    for (int rows = 0; rows < matrix.rows; rows++) {
        for (int cols = 0; cols < matrix.cols; cols++) {
            std::cout << matrix(rows, cols) << ", \t";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl << std::endl;
}
