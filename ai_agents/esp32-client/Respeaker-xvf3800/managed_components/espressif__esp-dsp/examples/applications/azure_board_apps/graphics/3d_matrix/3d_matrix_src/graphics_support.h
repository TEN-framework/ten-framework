/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_dsp.h"
#include "ekf_imu13states.h"


#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define DEG_TO_RAD 0.01745329252f                           // Degrees to radians conversion
#define RAD_TO_DEG 57.29577951f                             // Radians to degrees conversion
#define MATRIX_SIZE 4                                       // 4x4 matrices are used
#define SSD1306_WIDTH 128                                   // Display widh in pixels
#define SSD1306_HEIGHT 64                                   // DIsplay height
#define SSD1606_X_CENTER (SSD1306_WIDTH / 2)                // Width center point
#define SSD1606_Y_CENTER (SSD1306_HEIGHT / 2)               // Height center point

/**
 * @brief Data struct of 3d image matrix
 *
 * This structure is used to hold a 3d coordinates of a monochromatic image centered to the origin of the
 * cartesian space (0, 0, 0), that has ben processed by the image_to_3d_array.py.
 */
typedef struct image_3d_matrix_s {
    const float (*matrix)[MATRIX_SIZE];         /*!< matrix holding xyz coordinates for each point of the converted image.*/
    uint32_t matrix_len;                        /*!< image matrix length.*/
} image_3d_matrix_t;

/**
 * @brief Data struct of 3d image matrix with kalman filter object
 *
 * This structure is used to hold a 3d coordinates of a monochromatic image centered to the origin of the
 * cartesian space (0, 0, 0), that has ben processed by the image_to_3d_array.py.
 * Kalman filter object is added to the matrix, for the purpose of RTOS task arguments.
 */
typedef struct image_3d_matrix_kalman_s {
    const float (*matrix)[MATRIX_SIZE];         /*!< matrix holding xyz coordinates for each point of the converted image.*/
    uint32_t matrix_len;                        /*!< image matrix length.*/
    ekf_imu13states *ekf13;                     /*!< kalman filter object.*/
} image_3d_matrix_kalman_t;


/**
 * @brief   initialize perspective projection matrix
 *
 * Function initializes Mat class object holding perspective projection matrix.
 *
 * @param P_m: perspective projection matrix object from the Mat class
 */
void init_perspective_matrix(dspm::Mat &P_m);


/**
 * @brief   update scaling matrix
 *
 * Function updates scaling part of the transformation matrix.
 *
 * @param T_m: transformation matrix object from the Mat class
 * @param keep_diagonal: if true:  diagonal row of the transformation matrix T_m will be mulitplied with the scaling values
 *                       if false: diagonal row of the transformation matrix T_m will be substituted with new scaling values
 * @param scale_x: scaling value for x coordinate of the vector
 * @param scale_y: scaling value for y coordinate of the vector
 * @param scale_z: scaling value for z coordinate of the vector
 */
void update_scaling_matrix(dspm::Mat &T_m, bool keep_diagonal, float scale_x, float scale_y, float scale_z);


/**
 * @brief   update translation matrix
 *
 * Function updates translation part of the transformation matrix.
 *
 * @param T_m: transformation matrix object from the Mat class
 * @param row: if true:  translation values will be placed to the 3rd row of the transformation matrix T_m
 *             if false: translation values will be placed to the 3rd col of the transformation matrix T_m
 * @param move_x: translation value for x coordinate of the vector
 * @param move_y: translation value for y coordinate of the vector
 * @param move_z: translation value for z coordinate of the vector
 */
void update_translation_matrix(dspm::Mat &T_m, bool row, float move_x, float move_y, float move_z);


/**
 * @brief   update rotation matrix
 *
 * Function updates rotation part of the tranformation matrix
 *
 * @param T_m: transformation matrix object from the Mat class
 * @param rot_x: rotation angle for x direction of the vector
 * @param rot_y: rotation angle for y direction of the vector
 * @param rot_z: rotation angle for z direction of the vector
 */
void update_rotation_matrix(dspm::Mat &T_m, float rot_x, float rot_y, float rot_z);


/**
 * @brief   update perspective matrix
 *
 * Function updates perspective matrix with a new value of field of view
 *
 * @param P_m: perspective projection matrix object from the Mat class
 * @param fov: field of view in degrees
 */
void update_perspective_matrix(dspm::Mat &P_m, float fov);


/**
 * @brief   printf matrix to the terminal output
 *
 * Print matrix for debug purposes
 *
 * @param matrix: matrix to be printed
 */
void print_matrix(dspm::Mat matrix);

#ifdef __cplusplus
}
#endif
