// template file - template_img_to_3d.h
// arrays declarations will be modified by python script ImgTo3D.py

#pragma once

#include <stdint.h>
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t TEMPLATE_ARRAY_BMP_IMAGE[NUM_OF_BYTES];
extern const float TEMPLATE_ARRAY_IMAGE_TO_3D_MATRIX[NUM_OF_POINTS][4];

#ifdef __cplusplus
}
#endif
