/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "esp_log.h"
#include "bsp/m5stack_core_s3.h"
#include "esp_dsp.h"
#include "cube_matrix.h"
#include "esp_logo.h"
#include "esp_text.h"
#include "graphics_support.h"
#include "image_to_3d_matrix.h"

static const char *TAG = "3d-kalman";

dspm::Mat perspective_matrix(MATRIX_SIZE, MATRIX_SIZE);

extern "C" void app_main();

#define M5_CUBE_SIDE (BSP_LCD_V_RES / 4)

// X Y Z coordinates of the cube centered to (0, 0, 0)
const float m5_cube_vectors_3d[CUBE_POINTS][MATRIX_SIZE] =
    //      X           Y           Z      W
{   {-M5_CUBE_SIDE, -M5_CUBE_SIDE, -M5_CUBE_SIDE, 1},              // -1, -1, -1
    {-M5_CUBE_SIDE, -M5_CUBE_SIDE,  M5_CUBE_SIDE, 1},              // -1, -1,  1
    {-M5_CUBE_SIDE,  M5_CUBE_SIDE, -M5_CUBE_SIDE, 1},              // -1,  1, -1
    {-M5_CUBE_SIDE,  M5_CUBE_SIDE,  M5_CUBE_SIDE, 1},              // -1,  1,  1
    { M5_CUBE_SIDE, -M5_CUBE_SIDE, -M5_CUBE_SIDE, 1},              //  1, -1, -1
    { M5_CUBE_SIDE, -M5_CUBE_SIDE,  M5_CUBE_SIDE, 1},              //  1, -1,  1
    { M5_CUBE_SIDE,  M5_CUBE_SIDE, -M5_CUBE_SIDE, 1},              //  1,  1, -1
    { M5_CUBE_SIDE,  M5_CUBE_SIDE,  M5_CUBE_SIDE, 1}               //  1,  1,  1
};

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
    image->matrix = m5_cube_vectors_3d;
    image->matrix_len = ((sizeof(m5_cube_vectors_3d)) / sizeof(float)) / MATRIX_SIZE;
}

lv_style_t style_red;
lv_style_t style_blue;
lv_display_t *display = NULL;
/**
 * @brief Initialize display
 */
lv_obj_t **objs;
lv_point_precise_t *points;


static i2c_master_bus_handle_t i2c_handle;
static i2c_master_dev_handle_t bmi270_h = NULL;
#define BSP_BMI270_ADDR     0x69
#define BSP_BMM150_ADDR     0x10

// i2c read/write buffers
static uint8_t i2c_read_buffer[32];
static uint8_t i2c_write_buffer[32];

// Definitions for bmi270 registers
#define BMI270_IF_CONF 0x6B  // Auxiliary I2C
#define BMI270_AUX_DEV_ID 0x4B  // Auxiliary I2C Device address
#define BMI270_PWR_CONF 0x7C
#define BMI270_PWR_CTRL 0x7D  // Auxiliary I2C Device address
#define BMI270_CMD 0x7E // CMD
#define BMI270_AUX_IF_CONFIG 0x4C  // Auxiliary I2C configuration register
#define BMI270_AUX_READ_ADDR 0x4D    // Read from auxiliary device
#define BMI270_AUX_WRITE_ADDR 0x4E   // Write to auxiliary device
#define BMI270_AUX_WRITE_DATA 0x4F   // Write to auxiliary device
#define BMI270_AUX_STATUS          0x03
#define BMI270_AUX_DATA0           0x04
#define BMI270_ACC_DATA0           0x0C
#define BMI270_GYR_DATA0           0x12
#define BMI270_ACC_CONF            0x40
#define BMI270_ACC_RANGE           0x41
#define BMI270_GYR_CONF            0x42
#define BMI270_GYR_RANGE           0x43
#define BMI270_AUX_CONF            0x44
#define BMI270_INIT_CTRL           0x59
#define BMI270_INIT_DATA           0x5e
#define BMI270_INTERNAL_STATUS               0x21

// Definitions for bmi150 registers
#define BMM150_REG_POWER_CONTROL    0x4B
#define BMM150_SHIP_ID              0x40
#define BMM150_DATA0                0x42

// Basic fuctions to access bmi270 and bmm150
esp_err_t read_bmm150_data(uint8_t addr, uint8_t *data, int length)
{
    i2c_write_buffer[0] = BMI270_AUX_READ_ADDR;
    i2c_write_buffer[1] = addr;
    esp_err_t err = i2c_master_transmit(bmi270_h, i2c_write_buffer, 2, 1000);

    i2c_write_buffer[0] = BMI270_AUX_STATUS;
    err = i2c_master_transmit_receive(bmi270_h, i2c_write_buffer, 1, data, length, 1000);

    i2c_write_buffer[0] = BMI270_AUX_DATA0;
    err = i2c_master_transmit_receive(bmi270_h, i2c_write_buffer, 1, data, length, 1000);
    return err;
}

esp_err_t write_bmm150_data(uint8_t addr, uint8_t *data, int length)
{
    i2c_write_buffer[0] = BMI270_AUX_WRITE_DATA;
    i2c_write_buffer[1] = data[0];
    esp_err_t err = i2c_master_transmit(bmi270_h, i2c_write_buffer, 2, 1000);

    i2c_write_buffer[0] = BMI270_AUX_WRITE_ADDR;
    i2c_write_buffer[1] = addr;
    err = i2c_master_transmit(bmi270_h, i2c_write_buffer, 2, 1000);

    return err;
}

esp_err_t write_bmi270_data(uint8_t addr, uint8_t *data, int length)
{
    if (length < 32) {
        i2c_write_buffer[0] = BMI270_AUX_WRITE_DATA;
        for (size_t i = 0; i < length; i++) {
            i2c_write_buffer[1 + i] = data[i];
        }
        esp_err_t err = i2c_master_transmit(bmi270_h, i2c_write_buffer, 1 + length, 1000);
        return err;
    }

    uint8_t *temp_data = (uint8_t *)malloc(length + 4);
    for (size_t i = 0; i < length; i++) {
        temp_data[1 + i] = data[i];
    }
    temp_data[0] = addr;

    esp_err_t err = i2c_master_transmit(bmi270_h, temp_data, 1 + length, 1000);
    free(temp_data);
    return err;
}

esp_err_t read_bmi270_data(uint8_t addr, uint8_t *data, int length)
{

    i2c_write_buffer[0] = addr;
    esp_err_t err = i2c_master_transmit_receive(bmi270_h, i2c_write_buffer, 1, data, length, 1000);
    return err;
}

esp_err_t write_bmi270_reg(uint8_t addr, uint8_t data)
{

    i2c_write_buffer[0] = addr;
    i2c_write_buffer[1] = data;
    esp_err_t err = i2c_master_transmit(bmi270_h, i2c_write_buffer, 2, 1000);
    return err;
}

uint8_t read_bmi270_reg(uint8_t addr, esp_err_t *err)
{

    i2c_write_buffer[0] = addr;
    *err = i2c_master_transmit_receive(bmi270_h, i2c_write_buffer, 1, &i2c_write_buffer[16], 1, 1000);
    return i2c_write_buffer[16];
}

extern "C" uint8_t bmi270_context_config_file[];
extern "C" const int bmi270_context_config_file_size;

int16_t sensors_data[32];

/**
 * @brief Initialize the application.
 *
 * This function initialize the display 3D points and chips: mbi270 and bmm150
 *
 */

static void app_init(void)
{
    lv_style_init(&style_red);
    lv_style_init(&style_blue);

    lv_style_set_line_color(&style_red, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_line_width(&style_red, 10);
    lv_style_set_line_rounded(&style_red, false);
    lv_style_set_line_color(&style_blue, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_line_width(&style_blue, 10);
    lv_style_set_line_rounded(&style_blue, false);

    objs = (lv_obj_t **)malloc(CUBE_EDGES * sizeof(lv_obj_t *));
    points = (lv_point_precise_t *)malloc(CUBE_EDGES * 2 * sizeof(lv_point_precise_t));

    for (uint8_t cube_point = 0; cube_point < CUBE_EDGES / 2; cube_point++) {

        objs[cube_point] = lv_line_create(lv_screen_active());
        lv_obj_add_style(objs[cube_point], &style_red, 0);
    }
    for (uint8_t cube_point = CUBE_EDGES / 2; cube_point < CUBE_EDGES; cube_point++) {

        objs[cube_point] = lv_line_create(lv_screen_active());
        lv_obj_add_style(objs[cube_point], &style_blue, 0);
    }

    // Init the bmi270 and bmm150 chips
    esp_err_t err = i2c_master_get_bus_handle(CONFIG_BSP_I2C_NUM, &i2c_handle);

    const i2c_device_config_t bmi270_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BSP_BMI270_ADDR,
        .scl_speed_hz = 400000,
    };
    err = i2c_master_bus_add_device(i2c_handle, &bmi270_config, &bmi270_h);
    vTaskDelay(10);
    // Read chip ID to check the connection
    i2c_write_buffer[0] = 0x00;
    err = i2c_master_transmit_receive(bmi270_h, i2c_write_buffer, 1, i2c_read_buffer, 1, 1000);
    ESP_LOGI(TAG, "bmi270 ChipID = 0x%2.2x (should be 0x24), err = %2.2x", i2c_read_buffer[0], err);

    i2c_write_buffer[0] = BMI270_IF_CONF;
    i2c_write_buffer[1] = 0x20;
    err = i2c_master_transmit(bmi270_h, i2c_write_buffer, 2, 1000);

    i2c_write_buffer[0] = BMI270_IF_CONF;
    err = i2c_master_transmit_receive(bmi270_h, i2c_write_buffer, 1, i2c_read_buffer, 1, 1000);

    i2c_write_buffer[0] = BMI270_PWR_CTRL;
    i2c_write_buffer[1] = 0x0f;
    err = i2c_master_transmit(bmi270_h, i2c_write_buffer, 2, 1000);

    i2c_write_buffer[0] = BMI270_PWR_CTRL;
    err = i2c_master_transmit_receive(bmi270_h, i2c_write_buffer, 1, i2c_read_buffer, 1, 1000);

    i2c_write_buffer[0] = BMI270_AUX_IF_CONFIG;
    i2c_write_buffer[1] = 0x80;
    err = i2c_master_transmit(bmi270_h, i2c_write_buffer, 2, 1000);

    i2c_write_buffer[0] = BMI270_AUX_IF_CONFIG;
    err = i2c_master_transmit_receive(bmi270_h, i2c_write_buffer, 1, i2c_read_buffer, 1, 1000);
    // vTaskDelay(1);

    err = read_bmm150_data(BMM150_REG_POWER_CONTROL, i2c_read_buffer, 1);
    i2c_read_buffer[0] = 1;
    write_bmm150_data(BMM150_REG_POWER_CONTROL, i2c_read_buffer, 1);
    read_bmm150_data(BMM150_REG_POWER_CONTROL, i2c_read_buffer, 1);
    vTaskDelay(1);

    err = read_bmm150_data(BMM150_SHIP_ID, i2c_read_buffer, 1);
    ESP_LOGI(TAG, "bmm150 chip ID = 0x%2.2x (should be 0x32), err = %2.2x", i2c_read_buffer[0], err);

    err = read_bmm150_data(0x4c, i2c_read_buffer, 1);
    i2c_read_buffer[0] = 0x3 << 3;
    write_bmm150_data(0x4c, i2c_read_buffer, 1);
    err = read_bmm150_data(0x4c, i2c_read_buffer, 1);
    vTaskDelay(1);

    write_bmi270_reg(BMI270_PWR_CONF, 0);
    ESP_LOGI(TAG, "bmi270 status = %2.2x", read_bmi270_reg(BMI270_INTERNAL_STATUS, &err));
    vTaskDelay(10 / portTICK_PERIOD_MS);

    write_bmi270_reg(BMI270_INIT_CTRL, 0x00);
    err = write_bmi270_data(BMI270_INIT_DATA, bmi270_context_config_file, bmi270_context_config_file_size);
    write_bmi270_reg(BMI270_INIT_CTRL, 0x01);
    ESP_LOGI(TAG, "bmi270 status = %2.2x", read_bmi270_reg(BMI270_INTERNAL_STATUS, &err));

    write_bmi270_reg(BMI270_PWR_CTRL, 0x0f);
    write_bmi270_reg(BMI270_ACC_CONF, 0xA6);
    write_bmi270_reg(BMI270_GYR_CONF, 0xA6);
    write_bmi270_reg(BMI270_PWR_CONF, 0x02);
    write_bmi270_reg(BMI270_AUX_CONF, 0x07);
    write_bmi270_reg(BMI270_ACC_RANGE, 0x03);
    write_bmi270_reg(BMI270_GYR_RANGE, 0x00);

    i2c_write_buffer[0] = BMI270_IF_CONF;
    i2c_write_buffer[1] = 0x00;
    err = i2c_master_transmit(bmi270_h, i2c_write_buffer, 2, 1000);

    i2c_write_buffer[0] = BMI270_AUX_READ_ADDR;
    i2c_write_buffer[1] = BMM150_DATA0;
    err = i2c_master_transmit(bmi270_h, i2c_write_buffer, 2, 1000);

    i2c_write_buffer[0] = BMI270_IF_CONF;
    i2c_write_buffer[1] = 0x20;
    err = i2c_master_transmit(bmi270_h, i2c_write_buffer, 2, 1000);

    i2c_write_buffer[0] = BMI270_AUX_IF_CONFIG;
    i2c_write_buffer[1] = 0x03;
    err = i2c_master_transmit(bmi270_h, i2c_write_buffer, 2, 1000);

    ESP_LOGI(TAG, "bmi270 initialization is done");
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
    // For the 3D cube, only the 6 points of the cube are transformed
    // Cube edges, connecting transformed 3D cube points are connected with lines here
    bsp_display_lock(10000);
    for (uint8_t cube_point = 0; cube_point < CUBE_EDGES; cube_point++) {
        points[cube_point * 2 + 0].x = (int16_t)projected_image(cube_dict_line_begin[cube_point], 0);
        points[cube_point * 2 + 0].y = (int16_t)projected_image(cube_dict_line_begin[cube_point], 1);
        points[cube_point * 2 + 1].x = (int16_t)projected_image(cube_dict_line_end[cube_point], 0);
        points[cube_point * 2 + 1].y = (int16_t)projected_image(cube_dict_line_end[cube_point], 1);
        lv_line_set_points(objs[cube_point], &points[cube_point * 2 + 0], 2);
        lv_obj_set_pos(objs[cube_point], 0, 0);
    }
    bsp_display_unlock();
}

ekf_imu13states *ekf13 = NULL;

/**
 * @brief RTOS task to draw a 3d image.
 *
 * Updates 3d matrices, prepares the final 3d matrix to be displayed on the display
 *
 * @param arg: pointer to RTOS task arguments, 3d image structure in this case
 */
static void draw_3d_image_task(void *arg)
{
    image_3d_matrix_t *image = (image_3d_matrix_t *)arg;

    dspm::Mat T = dspm::Mat::eye(MATRIX_SIZE);                      // Transformation matrix
    dspm::Mat transformed_image(image->matrix_len, MATRIX_SIZE);    // 3D image matrix after transformation
    dspm::Mat projected_image(image->matrix_len, MATRIX_SIZE);      // 3D image matrix after projection

    dspm::Mat matrix_3d((float *)image->matrix[0], image->matrix_len, MATRIX_SIZE);

    float dt = 0;
    static float prev_time = 0;
    float current_time = dsp_get_cpu_cycle_count();
    float R_m[6] = {0.01, 0.01, 0.01, 0.01, 0.01, 0.01};

    float magn_norm = 1;
    while (1) {

        esp_err_t err;

        // Calculate dt for kalman filter
        current_time = dsp_get_cpu_cycle_count();
        if (current_time > prev_time) {
            dt = current_time - prev_time;
            dt = dt / 240000000.0;
        }
        prev_time = current_time;


        // Read and convert data from bmi270 and bmm150 sensors
        err = read_bmi270_data(BMI270_AUX_DATA0, (uint8_t *)sensors_data, 20);
        float accel[3];
        float gyro[3];
        float magn[3];
        for (size_t i = 0; i < 3; i++) {
            magn[i] = sensors_data[i];
            accel[i] = sensors_data[4 + i];
            gyro[i] = sensors_data[7 + i];
            /* code */
        }

        // We have to apply this because initial direction of sensors
        magn[1] =  -magn[1];
        accel[1] =  -accel[1];
        gyro[1] =  -gyro[1];

        magn[2] =  -magn[2];
        accel[2] =  -accel[2];
        gyro[2] =  -gyro[2];

        dspm::Mat gyro_input_mat(gyro, 3, 1);
        dspm::Mat accel_input_mat(accel, 3, 1);
        dspm::Mat mag_input_mat(magn, 3, 1);

        // Accelerometer has 166 max range fit to the int16 value
        accel_input_mat = accel_input_mat / 32768 * 16;
        if (magn_norm < mag_input_mat.norm()) {
            magn_norm = mag_input_mat.norm();
        }
        mag_input_mat = (1 / magn_norm) * mag_input_mat;

        // range 2000 gedree/sec fit to the int16 range
        gyro_input_mat *= (2000 * DEG_TO_RAD / 32768);

        ekf13->Process(gyro_input_mat.data, dt);
        ekf13->UpdateRefMeasurementMagn(accel_input_mat.data, mag_input_mat.data, R_m);

        // Convert directin quaternion to rotation matrix
        dspm::Mat R1 = ekf::quat2rotm(ekf13->X.data).t();       // matrix(3x1) that holds x, y, z rotation data
        // Convert rotation matrix to Euler angels
        dspm::Mat eul_angles = ekf::rotm2eul(R1);
        // Apply radian to degree
        eul_angles *= RAD_TO_DEG;
        // Apply rotation in all the axes to the transformation matrix
        update_rotation_matrix(T, eul_angles(0, 0), eul_angles(1, 0), eul_angles(2, 0));
        // Apply translation to the transformation matrix
        update_translation_matrix(T, true, ((float)BSP_LCD_H_RES / 2), ((float)BSP_LCD_V_RES / 2), 0);

        // matrix mul cube_matirx(8x4) * transformation_matrix(4x4) = transformed_cube(8x4)
        transformed_image = matrix_3d * T;
        // matrix mul transformed_cube(8x4) * perspective_matrix(4x4) = projected_cube(8x4)
        projected_image = transformed_image * perspective_matrix;

        display_3d_image(projected_image);
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}
image_3d_matrix_t image;
void app_main(void)
{
    ekf13 = new ekf_imu13states();
    ekf13->Init();

    // Init all board components
    display = bsp_display_start();
    init_perspective_matrix(perspective_matrix);
    init_3d_matrix_struct(&image);
    app_init();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    xTaskCreate(draw_3d_image_task, "draw_3d_image", 16384, &image, 4, NULL);
    ESP_LOGI(TAG, "Showing 3D image");

}
