/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "bsp/m5stack_core_s3.h"
#include "esp_dsp.h"
#include "cube_matrix.h"
#include "esp_logo.h"
#include "image_to_3d_matrix.h"
#include "esp_lcd_types.h"


static const char *TAG = "3D image demo";
dspm::Mat perspective_matrix(MATRIX_SIZE, MATRIX_SIZE);
image_3d_matrix_t image;

extern "C" void app_main();

/**
 * @brief Initialize 3d image structure
 *
 * Assigns a 3d image to be displayed to the 3d image structure
 *
 * @param image: 3d image structure
 */
static void init_3d_matrix_struct(image_3d_matrix_t *image)
{
    image->matrix = image_3d_matrix_esp_logo;
    image->matrix_len = ((sizeof(image_3d_matrix_esp_logo)) / sizeof(float)) / MATRIX_SIZE;
    ESP_LOGI(TAG, "Selected 3D image - ESP Logo");
}

lv_display_t *display = NULL;
lv_color16_t color_data[BSP_LCD_H_RES * BSP_LCD_V_RES];

/**
 * @brief Display a 3d image
 *
 * Fill the display buffer with points from Logo matrix
 *
 * @param projected_image: 3d matrix from Mat class after projection
 */
static void display_3d_image(dspm::Mat projected_image)
{
    for (int i = 0 ; i < projected_image.rows ; i++) {
        int x = projected_image(i, 0);
        int y = projected_image(i, 1);

        for (size_t xx = 0; xx < 4; xx++) {
            for (size_t yy = 0; yy < 4; yy++) {
                int index = (y + yy) * BSP_LCD_H_RES + (x + xx);
                color_data[index].blue  = 0x00;
                color_data[index].red   = 0x1f;
                color_data[index].green = 0x00;
            }
        }
    }
}

/**
 * @brief RTOS task to draw a 3d image.
 *
 * Updates 3d matrices, prepares the final 3d matrix to be displayed on the display
 *
 * @param arg: pointer to RTOS task arguments, 3d image structure in this case
 */

static void draw_3d_image_task(void *arg)
{
    float rot_y = 0;
    const float angle_increment_y = 4;
    image_3d_matrix_t *image = (image_3d_matrix_t *)arg;

    dspm::Mat T0 = dspm::Mat::eye(MATRIX_SIZE);                     // Transformation matrix
    dspm::Mat transformed_image(image->matrix_len, MATRIX_SIZE);    // 3D image matrix after transformation
    dspm::Mat projected_image(image->matrix_len, MATRIX_SIZE);      // 3D image matrix after projection
    dspm::Mat matrix_3d((float *)image->matrix[0], image->matrix_len, MATRIX_SIZE);

    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(display);
    int size = BSP_LCD_H_RES * BSP_LCD_V_RES;

    while (1) {

        rot_y -= angle_increment_y;
        if (rot_y <= 0) {
            rot_y += 360;
        }

        // Apply rotation in all the axes to the transformation matrix
        update_rotation_matrix(T0, 0, rot_y, 0);
        // Scale input matrix to fit the display
        update_scaling_matrix(T0, true, 4, 4, 1);
        // Apply translation to the transformation matrix
        update_translation_matrix(T0, true, ((float)BSP_LCD_H_RES / 2), ((float)BSP_LCD_V_RES / 2), 0);

        dspm::Mat result_image = matrix_3d * T0 * perspective_matrix;
        // Clear buffer before update
        memset(color_data, 0xff, size * 2);
        // Fill the display buffer with result matrix data
        display_3d_image(result_image);
        // Swap colors to the display format
        lv_draw_sw_rgb565_swap(color_data, size);
        // Update the display
        esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, color_data);
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

extern "C" lv_display_t *display_init(void);

void app_main(void)
{
    ESP_LOGI(TAG, "Start the 3D graphics application");
    ekf_imu13states *ekf13 = new ekf_imu13states();
    ekf13->Init();

    init_perspective_matrix(perspective_matrix);
    init_3d_matrix_struct(&image);

    // Init the display
    display = display_init();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    xTaskCreate(draw_3d_image_task, "draw_3d_image", 16384, &image, 3, NULL);
    ESP_LOGI(TAG, "Showing 3D image");
}
