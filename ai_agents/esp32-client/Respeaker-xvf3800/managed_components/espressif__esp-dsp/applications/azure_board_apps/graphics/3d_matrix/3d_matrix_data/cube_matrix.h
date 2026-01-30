/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "graphics_support.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define CUBE_POINTS 8
#define CUBE_EDGES 12
#define CUBE_SIDE (SSD1606_Y_CENTER / 2)

// X Y Z coordinates of the cube centered to (0, 0, 0)
const float cube_vectors_3d[CUBE_POINTS][MATRIX_SIZE] =
    //      X           Y           Z      W
{   {-CUBE_SIDE, -CUBE_SIDE, -CUBE_SIDE, 1},              // -1, -1, -1
    {-CUBE_SIDE, -CUBE_SIDE,  CUBE_SIDE, 1},              // -1, -1,  1
    {-CUBE_SIDE,  CUBE_SIDE, -CUBE_SIDE, 1},              // -1,  1, -1
    {-CUBE_SIDE,  CUBE_SIDE,  CUBE_SIDE, 1},              // -1,  1,  1
    { CUBE_SIDE, -CUBE_SIDE, -CUBE_SIDE, 1},              //  1, -1, -1
    { CUBE_SIDE, -CUBE_SIDE,  CUBE_SIDE, 1},              //  1, -1,  1
    { CUBE_SIDE,  CUBE_SIDE, -CUBE_SIDE, 1},              //  1,  1, -1
    { CUBE_SIDE,  CUBE_SIDE,  CUBE_SIDE, 1}               //  1,  1,  1
};

// Dictionary for 3d cube edges displaying
// Cube edges   cube_vectors_3d[3] <-> cube_vectors_3d[1]
//              cube_vectors_3d[3] <-> cube_vectors_3d[7]
//              cube_vectors_3d[5] <-> cube_vectors_3d[7]
//              cube_vectors_3d[5] <-> cube_vectors_3d[1]....
const uint8_t cube_dict_line_begin[CUBE_EDGES] = {3, 3, 5, 5, 2, 2, 4, 4, 3, 7, 1, 5};
const uint8_t cube_dict_line_end[CUBE_EDGES]   = {1, 7, 7, 1, 0, 6, 6, 0, 2, 6, 0, 4};

#ifdef CONFIG_3D_OBJECT_CUBE
#define OBJECT_3D_CUBE 1
#else
#define OBJECT_3D_CUBE 0
#endif

#ifdef __cplusplus
}
#endif
