/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "esp_log.h"
#include "ssd1306.h"
#include "bsp/esp-bsp.h"
#include "esp_dsp.h"
#include "cube_matrix.h"
#include "esp_logo.h"
#include "esp_text.h"
#include "graphics_support.h"
#include "image_to_3d_matrix.h"

static ssd1306_handle_t ssd1306_dev = NULL;
static bool button_pressed = true;

dspm::Mat perspective_matrix(MATRIX_SIZE, MATRIX_SIZE);

extern "C" void app_main();

/**
 * @brief Initialize 3d image structure
 *
 * Assigns a 3d image to be displayed to the 3d image structure based on the Kconfig menu result.
 * The Kconfig menu is operated by a user
 *
 * @param image: 3d image structure
 */
static void init_3d_matrix_struct(image_3d_matrix_t *image)
{
#ifdef CONFIG_3D_OBJECT_ESP_LOGO
    image->matrix = image_3d_matrix_esp_logo;
    image->matrix_len = ((sizeof(image_3d_matrix_esp_logo)) / sizeof(float)) / MATRIX_SIZE;
    ESP_LOGI("3D image demo", "Selected 3D image - ESP Logo");
#elif CONFIG_3D_OBJECT_CUSTOM
    image->matrix = image_to_3d_matrix_custom;
    image->matrix_len = ((sizeof(image_to_3d_matrix_custom)) / sizeof(float)) / MATRIX_SIZE;
    ESP_LOGI("3D image demo", "Selected 3D image - User's custom image");
#elif CONFIG_3D_OBJECT_CUBE
    image->matrix = cube_vectors_3d;
    image->matrix_len = ((sizeof(cube_vectors_3d)) / sizeof(float)) / MATRIX_SIZE;
    ESP_LOGI("3D image demo", "Selected 3D image - 3D cube");
#endif
}

/**
 * @brief Initialize display
 */
static void app_ssd1306_init(void)
{
    ssd1306_dev = ssd1306_create((i2c_port_t)BSP_I2C_NUM, SSD1306_I2C_ADDRESS);
    ssd1306_clear_screen(ssd1306_dev, 0x00);
    ssd1306_refresh_gram(ssd1306_dev);
}

/**
 * @brief Display a 3d image
 *
 * If the object is the 3d cube, connect the projected cube points by lines and display the lines
 * For any other 3d object lit pixels on the display from provided XY coordinates
 *
 * @param projected_image: 3d matrix from Mat class after projection
 */
static void display_3d_image(dspm::Mat projected_image)
{
    ssd1306_clear_screen(ssd1306_dev, 0);

    if (OBJECT_3D_CUBE) {
        // For the 3D cube, only the 6 points of the cube are transformed
        // Cube edges, connecting transformed 3D cube points are connected with lines here
        for (uint8_t cube_point = 0; cube_point < CUBE_EDGES; cube_point++) {
            ssd1306_draw_line(ssd1306_dev,
                              (int16_t)projected_image(cube_dict_line_begin[cube_point], 0),
                              (int16_t)projected_image(cube_dict_line_begin[cube_point], 1),
                              (int16_t)projected_image(cube_dict_line_end[cube_point], 0),
                              (int16_t)projected_image(cube_dict_line_end[cube_point], 1));
        }
    } else {
        // Every other 3D image is drawn here pixel by pixel
        for (uint32_t pixel = 0; pixel < projected_image.rows; pixel++ ) {
            ssd1306_fill_point(ssd1306_dev, projected_image(pixel, 0), projected_image(pixel, 1), 1);
        }
    }
    ssd1306_refresh_gram(ssd1306_dev);
}

/**
 * @brief Display ESPRESSIF text
 *
 * To demonstrate usage of the translation and scaling matrices
 */
static void dispaly_esp_text(void)
{
    image_3d_matrix_t esp_text;
    esp_text.matrix = image_3d_array_esp_text;
    esp_text.matrix_len = ((sizeof(image_3d_array_esp_text)) / sizeof(float)) / MATRIX_SIZE;
    int16_t shift_x = -SSD1606_X_CENTER;

    dspm::Mat T = dspm::Mat::eye(MATRIX_SIZE);                      // Transformation matrix
    dspm::Mat transformed_image(esp_text.matrix_len, MATRIX_SIZE);  // 3D image matrix after transformation
    dspm::Mat matrix_3d((float *)esp_text.matrix[0], esp_text.matrix_len, MATRIX_SIZE);

    ESP_LOGI("3D image demo", "Showing ESP text");

    for (int i = 0; i < 52; i++) {
        update_translation_matrix(T, true, (float)shift_x, (float)SSD1606_Y_CENTER, 0);
        transformed_image = matrix_3d * T;

        ssd1306_clear_screen(ssd1306_dev, 0);
        for (uint32_t point = 0; point < transformed_image.rows; point++ ) {
            ssd1306_fill_point(ssd1306_dev, transformed_image(point, 0), transformed_image(point, 1), 1);
        }
        ssd1306_refresh_gram(ssd1306_dev);
        vTaskDelay(50 / portTICK_PERIOD_MS);

        shift_x += 5;
    }

    ssd1306_clear_screen(ssd1306_dev, 0);
    ssd1306_draw_bitmap(ssd1306_dev, 0, 24, &image_bmp_array_esp_text[0], 128, 24);
    ssd1306_refresh_gram(ssd1306_dev);

    update_translation_matrix(T, true, (float)SSD1606_X_CENTER, (float)SSD1606_Y_CENTER, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    float scale = 1;
    for (int i = 0; i < 20; i++) {
        update_scaling_matrix(T, false, scale, scale, 1);
        transformed_image = matrix_3d * T;

        ssd1306_clear_screen(ssd1306_dev, 0);
        for (uint32_t point = 0; point < transformed_image.rows; point++ ) {
            ssd1306_fill_point(ssd1306_dev, transformed_image(point, 0), transformed_image(point, 1), 1);
        }
        ssd1306_refresh_gram(ssd1306_dev);
        vTaskDelay(50 / portTICK_PERIOD_MS);

        if (i < 10) {
            scale -= 0.05;
        } else {
            scale += 0.05;
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
    float rot_y = 0, rot_x = 0;
    const float angle_increment = 4;
    image_3d_matrix_t *image = (image_3d_matrix_t *)arg;

    dspm::Mat T = dspm::Mat::eye(MATRIX_SIZE);                      // Transformation matrix
    dspm::Mat transformed_image(image->matrix_len, MATRIX_SIZE);    // 3D image matrix after transformation
    dspm::Mat projected_image(image->matrix_len, MATRIX_SIZE);      // 3D image matrix after projection
    dspm::Mat matrix_3d((float *)image->matrix[0], image->matrix_len, MATRIX_SIZE);

    if (OBJECT_3D_CUBE) {
        rot_x = 45;
    }

    while (1) {
        if (button_pressed) {
            rot_y += angle_increment;
            if (rot_y >= 360) {
                rot_y -= 360;
            }
        } else {
            rot_y -= angle_increment;
            if (rot_y <= 0) {
                rot_y += 360;
            }
        }

        // Apply rotation in all the axes to the transformation matrix
        update_rotation_matrix(T, rot_x, rot_y, 0);
        // Apply translation to the transformation matrix
        update_translation_matrix(T, true, ((float)SSD1606_X_CENTER), ((float)SSD1606_Y_CENTER), 0);

        // explanation for the matrix multiplication is for the 3D cube scenario, applies for all of the objects
        // where matrix rows for the transformed image and the projected image are set according to the specific 3d object

        // matrix mul cube_matirx(8x4) * transformation_matrix(4x4) = transformed_cube(8x4)
        transformed_image = matrix_3d * T;
        // matrix mul transformed_cube(8x4) * perspective_matrix(4x4) = projected_cube(8x4)
        projected_image = transformed_image * perspective_matrix;

        display_3d_image(projected_image);
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    static bool button_prev_val = false;
    image_3d_matrix_t image;
    ekf_imu13states *ekf13 = new ekf_imu13states();
    ekf13->Init();

    // Init all board components
    bsp_i2c_init();
    app_ssd1306_init();                         // display init
    bsp_leds_init();                            // LEDs init
    bsp_i2c_set_clk_speed(I2C_CLK_600KHZ);      // Set I2C to 600kHz

    init_perspective_matrix(perspective_matrix);
    init_3d_matrix_struct(&image);

    dispaly_esp_text();
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    xTaskCreate(draw_3d_image_task, "draw_3d_image", 2048, &image, 4, NULL);
    ESP_LOGI("3D image demo", "Showing 3D image");

    while (1) {
        if (bsp_button_get()) {
            button_pressed = !button_pressed;
        }

        if (button_prev_val != button_pressed) {
            button_prev_val = button_pressed;
            if (button_pressed) {
                bsp_led_set(BSP_LED_AZURE, true);
            } else {
                bsp_led_set(BSP_LED_AZURE, false);
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
