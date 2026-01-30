/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <malloc.h>
#include "mpu6050.h"
#include "ssd1306.h"
#include "mag3110.h"
#include "fbm320.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_idf_version.h" // for backward compatibility of esp-timer
#include "nvs.h"
#include "nvs_flash.h"
#include "graphics_support.h"
#include "cube_matrix.h"
#include "esp_dsp.h"
#include "ekf_imu13states.h"
#include "esp_logo.h"
#include "image_to_3d_matrix.h"

#define STORAGE_NAMESPACE "kalman_filter"
#define USE_BAROMETER 0

static ssd1306_handle_t ssd1306_dev = NULL;
static mpu6050_handle_t mpu6050_dev = NULL;
static mag3110_handle_t mag3110_dev = NULL;
static fbm320_handle_t fbm320_dev = NULL;

static bool kalman_filter_calibrated = false;
static bool board_inactive = true;

dspm::Mat perspective_matrix(MATRIX_SIZE, MATRIX_SIZE);

extern "C" void app_main();

/**
 * @brief Initialize magnetometer
 */
static void app_mag3110_init(void)
{
    esp_err_t ret;
    mag3110_dev = mag3110_create((i2c_port_t)BSP_I2C_NUM);
    mag3110_start_raw(mag3110_dev, MAG3110_DR_OS_80_16);
    assert(ESP_OK == ret);
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
 * @brief Initialize accelerometer and gyroscope
 */
static void mpu6050_init(void)
{
    esp_err_t ret;
    mpu6050_dev = mpu6050_create((i2c_port_t)BSP_I2C_NUM, MPU6050_I2C_ADDRESS);
    ret = mpu6050_config(mpu6050_dev, ACCE_FS_8G, GYRO_FS_2000DPS);
    assert(ESP_OK == ret);
    ret = mpu6050_wake_up(mpu6050_dev);
    assert(ESP_OK == ret);
}

/**
 * @brief Initialize pressure sensor
 */
static void app_fbm320_init(void)
{
    esp_err_t ret;
    fbm320_dev = fbm320_create((i2c_port_t)BSP_I2C_NUM, FBM320_I2C_ADDRESS_1);
    ret = fbm320_init(fbm320_dev);
    assert(ESP_OK == ret);
}

/**
 * @brief Initialize NVS flash memory
 */
static void init_nvs_flash_memory(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
}

/**
 * @brief Initialize 3d image structure
 *
 * Assigns a 3d image to be displayed to the 3d image structure based on the Kconfig menu result.
 * The Kconfig menu is operated by a user
 *
 * @param image: pointer to 3d image structure
 * @param ekf13: kalman filter object
 */
static void init_3d_matrix_struct(image_3d_matrix_kalman_t *image, ekf_imu13states *ekf13)
{
#ifdef CONFIG_3D_OBJECT_ESP_LOGO
    image->matrix = image_3d_matrix_esp_logo;
    image->matrix_len = ((sizeof(image_3d_matrix_esp_logo)) / sizeof(float)) / MATRIX_SIZE;
    ESP_LOGI("Kalman filter demo", "Selected 3D image - ESP Logo");
#elif CONFIG_3D_OBJECT_CUSTOM
    image->matrix = image_to_3d_matrix_custom;
    image->matrix_len = ((sizeof(image_to_3d_matrix_custom)) / sizeof(float)) / MATRIX_SIZE;
    ESP_LOGI("Kalman filter demo", "Selected 3D image - User's custom image");
#elif CONFIG_3D_OBJECT_CUBE
    image->matrix = cube_vectors_3d;
    image->matrix_len = ((sizeof(cube_vectors_3d)) / sizeof(float)) / MATRIX_SIZE;
    ESP_LOGI("Kalman filter demo", "Selected 3D image - 3D cube");
#endif
    image->ekf13 = ekf13;
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
 * @brief Draw a 3d image
 *
 * Updates 3d matrices and prepares the final 3d matrix to be displayed on the display.
 * Board inactivity check - decides which board mode to display (active or inactive), based on the board movements.
 *
 * @param ekf13: kalman filter object
 * @param transformed_image: 3d matrix holding a 3d image after transformation
 * @param projected_image: 3d matrix holding a 3d image after projection
 * @param matrix_3d: 3d matrix holding the original 3d image, without any transformation
 */
static void draw_3d_image(ekf_imu13states *ekf13, dspm::Mat &transformed_image, dspm::Mat &projected_image, dspm::Mat &matrix_3d)
{
    static const float movement_treshold = 0.0001;      // threshold to decide between Idle and Active state of the board
    static float inactive_rotation = 0;                 // rotation angle (in degrees) for Idle state
    static unsigned int inactivity_count = 0;
    static const unsigned int inactivity_count_treshold = 75;
    static unsigned int inactivity_check = 0;           // activity of the board is being checked once in N calls of the function
    static float prev_state_arr[3] = {0, 0, 0};         // holds the previous state of the Euler angles, to compare the diff

    dspm::Mat T = dspm::Mat::eye(MATRIX_SIZE);          // Transformation matrix
    dspm::Mat R1 = ekf::quat2rotm(ekf13->X.data);       // matrix(3x1) that holds x, y, z rotation data
    dspm::Mat eul_angles = ekf::rotm2eul(R1);

    // check if the board is active or not every N calls of the function
    if (!(inactivity_check++ % 10)) {
        dspm::Mat prev_state_mat(prev_state_arr, 3, 1);
        dspm::Mat diff = eul_angles - prev_state_mat;
        prev_state_mat = eul_angles;

        float max_diff = fabs(diff(0, 0) * diff(1, 0) * diff(2, 0));

        // wake-up the board if the current board movement crosses the threshold
        if (board_inactive && (max_diff > movement_treshold)) {
            board_inactive = false;
            ESP_LOGI("Board status", "board put to active mode");
        }

        // if the board is awake, and the current movement of the board is lower than the threshold - the board is
        // being moved with - run the inactivity_counter
        // after some time (if the movement of the board has been lower than the threshold) put the board to idle mode
        else if (!board_inactive && (max_diff < movement_treshold)) {
            if (inactivity_count > inactivity_count_treshold) {
                board_inactive = true;
                inactivity_count = 0;
                ESP_LOGI("Board status", "board put to idle mode");
                update_perspective_matrix(perspective_matrix, 90);
            }
            inactivity_count++;
        }

        // if the board is awake and the current movement of the board is higher than the threshold clear the inactivity_counter
        else if (!board_inactive && (max_diff >= movement_treshold)) {
            inactivity_count = 0;
        }
    }

    if (board_inactive) {
        // board idle state - display a rotating cube
        update_rotation_matrix(T, inactive_rotation += 3.0, 10.0, 10.0);
    } else {
        // board active state - 3D object follows movements of the board
        eul_angles(2, 0) = -eul_angles(2, 0);
        dspm::Mat R = ekf::eul2rotm(eul_angles.data);

        // Enlarge rotation matrix from 3x3 to 4x4
        // Copy rotation matrix R(3x3) to transformation matrix T_m(4x4)
        for (int row = 0; row < R.rows; row++) {
            for (int col = 0; col < R.cols; col++) {
                T(row, col) = R(row, col);
            }
        }
    }

    // explanation for the matrix multiplication is for the 3D cube scenario, applies for all of the objects
    // where matrix rows for the transformed image and the projected image are set according to the specific 3d object

    // matrix mul cube_matirx(8x4) * transformation_matrix(4x4) = transformed_cube(8x4)
    transformed_image = matrix_3d * T;
    // matrix mul transformed_cube(8x4) * perspective_matrix(4x4) = projected_cube(8x4)
    projected_image = transformed_image * perspective_matrix;
    display_3d_image(projected_image);
}

/**
 * @brief Kalman filter RTOS task (or ESP timer callback)
 *
 * Takes IMU sensors measurements to be processed by the Kalman filter
 * Function is used as:
 *      RTOS task - during normal Kalman filter operation
 *      ESP Timer callback function - during Kalman filter calibration process
 *
 * @param arg: pointer to RTOS task arguments, 3d image structure in this case
 */
static void kalman_filter_task(void *arg)
{
    mpu6050_acce_value_t acce_sample;
    mpu6050_gyro_value_t gyro_sample;
    mag3110_result_t mag_sample;

    image_3d_matrix_kalman_t *kalman_filter_args = (image_3d_matrix_kalman_t *)arg;
    ekf_imu13states *ekf13 = kalman_filter_args->ekf13;
    dspm::Mat transformed_image(kalman_filter_args->matrix_len, MATRIX_SIZE);    // 3D image matrix after transformation
    dspm::Mat projected_image(kalman_filter_args->matrix_len, MATRIX_SIZE);      // 3D image matrix after projection
    dspm::Mat matrix_3d((float *)kalman_filter_args->matrix[0], kalman_filter_args->matrix_len, MATRIX_SIZE);

    // Covariance matrix for Kalman filter, set specifically for this development board IMU sensors
    float R_m[10] = {0.0001, 0.0001, 0.0001, 0.0001, 0.0001, 0.0001, 0.000001, 0.000001, 0.000001, 0.000001};
    update_perspective_matrix(perspective_matrix, 90);

    while (1) {
        // dt calculation
        static float prev_time = 0;
        const float current_time = dsp_get_cpu_cycle_count();
        float dt = 0;

        // Crystal count difference conversion to Dt time constant
        if (current_time > prev_time) {
            dt = current_time - prev_time;
            dt = dt / 240000000.0;
        }
        prev_time = current_time;

        // Get all the sensors values
        mpu6050_get_acce(mpu6050_dev, &acce_sample);
        mpu6050_get_gyro(mpu6050_dev, &gyro_sample);
        mag3110_get_magnetic_induction(mag3110_dev, &mag_sample);

        // Make arrays from the sensors values
        float gyro_input_arr[3] =  {gyro_sample.gyro_x,  gyro_sample.gyro_y,  gyro_sample.gyro_z};
        float accel_input_arr[3] = {acce_sample.acce_x,  acce_sample.acce_y,  acce_sample.acce_z};
        float mag_input_arr[3] =   {(float)mag_sample.x, (float)mag_sample.y, (float)mag_sample.z};

        // Accel and Mag data to Mat class
        dspm::Mat gyro_input_mat(gyro_input_arr, 3, 1);
        dspm::Mat accel_input_mat(accel_input_arr, 3, 1);
        dspm::Mat mag_input_mat(mag_input_arr, 3, 1);

        // Normalize vectors
        dspm::Mat accel_norm = accel_input_mat / accel_input_mat.norm();
        dspm::Mat magn_norm = mag_input_mat / mag_input_mat.norm();
        gyro_input_mat *= DEG_TO_RAD;

        ekf13->Process(gyro_input_mat.data, dt);
        ekf13->UpdateRefMeasurementMagn(accel_norm.data, magn_norm.data, R_m);

        if (kalman_filter_calibrated) {
            // Use the function as RTOS task for the filter calculation
            draw_3d_image(ekf13, transformed_image, projected_image, matrix_3d);
            vTaskDelay(20 / portTICK_PERIOD_MS);
        } else {
            // Use the function as a callback for kalman_filter_calibration_timer
            break;
        }
    }
}

/**
 * @brief Kalman filter calibration procedure
 *
 * The Kalman filter must be calibrated before the very first run. The state of the Kalman filter is saved
 * into NVS after the calibration.
 * The calibration is run, only if no Kalman filter state is saved in the NVS. Which occurs after erasing
 * the flash memory. Power cycling the board does not remove the Kalman filter state from the NVS.
 *
 * @param image: pointer to 3d image structure
 */
static void kalman_filter_calibration(image_3d_matrix_kalman_t *image)
{
    ekf_imu13states *ekf13 = image->ekf13;
    esp_err_t ret;
    nvs_handle_t nvs_handle_kalman;
    size_t state_vectors_size = 13 * 14;
    float *state_vectors = (float *)malloc(state_vectors_size * sizeof(float));

    ret = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle_kalman);
    if (ret != ESP_OK) {
        ESP_LOGE("NVS error", "(%s) opening NVS!\n", esp_err_to_name(ret));
        assert(ESP_OK == ret);
    }

    // Read previously saved blob, if available
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    ret = nvs_get_blob(nvs_handle_kalman, "state_vectors", NULL, &required_size);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE("NVS error", "(%s) reading data from NVS!\n", esp_err_to_name(ret));
        assert(ESP_OK == ret);
    }

    if (required_size > 0) {
        ESP_LOGI("Kalman filter demo", "Filter state vectors present in the NVS");
        ESP_LOGI("Kalman filter demo", "Loading state vectors into the filter structure");

        size_t state_vectors_size_addr = state_vectors_size * sizeof(float);
        ret = nvs_get_blob(nvs_handle_kalman, "state_vectors", state_vectors, &state_vectors_size_addr);
        if (ret != ESP_OK) {
            ESP_LOGE("NVS error", "(%s) reading data from NVS!\n", esp_err_to_name(ret));
            assert(ESP_OK == ret);
        }

        for (int i = 0; i < state_vectors_size; i++) {
            if (i < state_vectors_size - 13) {
                ekf13->P.data[i] = state_vectors[i];
            } else {
                ekf13->X.data[i - (state_vectors_size - 13)] = state_vectors[i];
            }
        }

        ESP_LOGI("Kalman filter demo", "State vectors loaded from the NVS");
        nvs_close(nvs_handle_kalman);

    } else {
        ESP_LOGI("Kalman filter demo", "Filter state vectors not present in the NVS");
        const float kalman_timer_period_us = 100000;

        // ESP timer for the Kalman filter calibration
        const esp_timer_create_args_t kalman_calibration_timer_config = {
            .callback = kalman_filter_task,
            .arg = image,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "kalman_filter_calibration_timer",
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)
            .skip_unhandled_events = true,
#endif
        };

        esp_timer_handle_t kalman_calibration_timer = NULL;
        ret = esp_timer_create(&kalman_calibration_timer_config, &kalman_calibration_timer);
        assert(ESP_OK == ret);
        ssd1306_clear_screen(ssd1306_dev, 0x00);

        ESP_LOGI("Kalman filter demo", "Starting Kalman filter calibration loop");

        ssd1306_clear_screen(ssd1306_dev, 0x00);
        ssd1306_draw_string(ssd1306_dev, 0, 16, (const uint8_t *)"Kalman filter", 16, 1);
        ssd1306_draw_string(ssd1306_dev, 0, 32, (const uint8_t *)"calibration", 16, 1);
        ssd1306_refresh_gram(ssd1306_dev);

        ret = esp_timer_start_periodic(kalman_calibration_timer, kalman_timer_period_us);
        assert(ESP_OK == ret);
        vTaskDelay(100000 / portTICK_PERIOD_MS);

        ret = esp_timer_stop(kalman_calibration_timer);
        assert(ESP_OK == ret);
        ret = esp_timer_delete(kalman_calibration_timer);
        assert(ESP_OK == ret);
        ESP_LOGI("Kalman filter demo", "Exiting Kalman filter calibration loop");
        ssd1306_draw_string(ssd1306_dev, 0, 48, (const uint8_t *)"Done!", 16, 1);
        ssd1306_refresh_gram(ssd1306_dev);
        vTaskDelay(100 / portTICK_PERIOD_MS);

        dspm::Mat estimated_error(&ekf13->X.data[4], 3, 1);

        ESP_LOGI("Kalman filter demo", "Estimated gyroscope bias error [deg/sec]: %.6f\t%.6f\t%.6f",
                 estimated_error.data[0], estimated_error.data[1], estimated_error.data[2]);

        for (int i = 0; i < state_vectors_size; i++) {
            if (i < state_vectors_size - 13) {
                state_vectors[i] = ekf13->P.data[i];
            } else {
                state_vectors[i] = ekf13->X.data[i - (state_vectors_size - 13)];
            }
        }

        ret = nvs_set_blob(nvs_handle_kalman, "state_vectors", state_vectors, state_vectors_size * sizeof(float));
        assert(ESP_OK == ret);
        ret = nvs_commit(nvs_handle_kalman);
        assert(ESP_OK == ret);
        nvs_close(nvs_handle_kalman);
        ESP_LOGI("Kalman filter demo", "State vectors saved to the NVS");
    }
    // Set the initial state of the X vector
    ekf13->X(0, 0) = 1;
    ekf13->X(0, 1) = 0;
    ekf13->X(0, 2) = 0;
    ekf13->X(0, 3) = 0;
    free(state_vectors);
    kalman_filter_calibrated = true;
}

/**
 * @brief RTOS task to periodically save the filter state
 *
 * The Kalman filter state is periodically saved into the NVS, each 5 min. So a recent Kalman filter state
 * could be loaded into the Kalman filter object after a power cycle.
 *
 * @param arg: pointer to RTOS task arguments, Kalman filter object in this case
 */
static void save_state_vectors_task(void *arg)
{
    esp_err_t ret;
    size_t state_vectors_size = 13 * 14;
    nvs_handle_t nvs_handle_kalman;
    ekf_imu13states *ekf13 = (ekf_imu13states *)arg;
    vTaskDelay((60000 * 5) / portTICK_PERIOD_MS);    //  5min

    while (1) {
        float *state_vectors = (float *)malloc(state_vectors_size * sizeof(float));

        ret = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle_kalman);
        if (ret != ESP_OK) {
            ESP_LOGE("NVS error", "(%s) opening NVS!\n", esp_err_to_name(ret));
            assert(ESP_OK == ret);
        }

        for (int i = 0; i < state_vectors_size; i++) {
            if (i < state_vectors_size - 13) {
                state_vectors[i] = ekf13->P.data[i];
            } else {
                state_vectors[i] = ekf13->X.data[i - (state_vectors_size - 13)];
            }
        }

        ret = nvs_set_blob(nvs_handle_kalman, "state_vectors", state_vectors, state_vectors_size * sizeof(float));
        if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE("NVS error", "(%s) writing data to NVS!\n", esp_err_to_name(ret));
            assert(ESP_OK == ret);
        }

        ret = nvs_commit(nvs_handle_kalman);
        if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE("NVS error", "(%s) commiting data to NVS!\n", esp_err_to_name(ret));
            assert(ESP_OK == ret);
        }
        nvs_close(nvs_handle_kalman);
        ESP_LOGI("Kalman filter demo", "State vectors saved to NVS");
        free(state_vectors);
        vTaskDelay((60000 * 5) / portTICK_PERIOD_MS);    //  Save the State vectors each 5 min
    }
}

/**
 * @brief ROTS task to read a pressure
 *
 * Pressure is measured periodically to better average out a stable value of pressure
 *
 * @param arg: pointer to RTOS task arguments, pointer to the pressure variable in this case
 */
static void get_pressure_task(void *arg)
{
    int32_t real_p, real_t;
    float *pressure_ptr = (float *)arg;
    float pressure_global = *pressure_ptr;

    int call_count = 0;
    int call_count1 = 0;
    float pressure_initial = pressure_global;
    float last_pressure = pressure_global;
    float baro_image = pressure_global;
    bool changed = false;

    while (1) {
        if (ESP_OK == fbm320_get_data(fbm320_dev, FBM320_MEAS_PRESS_OSR_4096, &real_t, &real_p)) {
            pressure_global = (0.999 * pressure_global) + (0.001 * ((float)real_p) / 1000.0);
            call_count1++;
        }

        if (board_inactive) {
            pressure_initial = pressure_global;
            last_pressure = pressure_global;
            baro_image = pressure_global;
        } else {
            call_count++;
            if (fabs(pressure_global - last_pressure) > 0.0005) {        // cahnge more than 0.5 Pa
                last_pressure = pressure_global;
                baro_image = (0.9 * baro_image) + (0.1 * last_pressure);
                changed = true;
            }

            if ((!(call_count % 10)) && changed) {
                float fov = 90 + (1000.0 * ((float)(pressure_initial - baro_image)));
                update_perspective_matrix(perspective_matrix, fov);
                changed = false;
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief read the initial value of pressure
 */
float get_initial_pressure(void)
{
    float pressure;
    int32_t real_p, real_t;
    int averagning_loop_count = 500;

    ESP_LOGI("Barometer", "Averagining initial barometer pressure");
    ssd1306_clear_screen(ssd1306_dev, 0x00);
    ssd1306_draw_string(ssd1306_dev, 0, 16, (const uint8_t *)"Barometer", 16, 1);
    ssd1306_draw_string(ssd1306_dev, 0, 32, (const uint8_t *)"averaging", 16, 1);
    ssd1306_refresh_gram(ssd1306_dev);

    // take the first pressure measurement
    while (1) {
        if (ESP_OK == fbm320_get_data(fbm320_dev, FBM320_MEAS_PRESS_OSR_4096, &real_t, &real_p)) {
            pressure = ((float)real_p) / 1000.0;        // pressure in kPa
            break;
        }
    }

    // average first N measurements
    while (averagning_loop_count--) {
        if (ESP_OK == fbm320_get_data(fbm320_dev, FBM320_MEAS_PRESS_OSR_4096, &real_t, &real_p)) {
            pressure = (0.999 * pressure) + (0.001 * ((float)real_p) / 1000.0);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }

    ESP_LOGI("Barometer", "Initial value set");
    ssd1306_draw_string(ssd1306_dev, 0, 48, (const uint8_t *)"Done!", 16, 1);
    ssd1306_refresh_gram(ssd1306_dev);
    return (pressure);
}

void app_main(void)
{
    image_3d_matrix_kalman_t image;
    ekf_imu13states *ekf13 = new ekf_imu13states();
    ekf13->Init();

    // Init all board components
    bsp_i2c_init();
    app_ssd1306_init();         // display init
    mpu6050_init();             // gyro, acc init
    app_mag3110_init();         // magnetometer init
    app_fbm320_init();          // barometer init
    bsp_leds_init();            // LEDs init
    init_nvs_flash_memory();    // Non-Volatile Storage

    init_perspective_matrix(perspective_matrix);
    init_3d_matrix_struct(&image, ekf13);
    kalman_filter_calibration(&image);

    // Use a barometer for measuring the altitude
    if (USE_BAROMETER) {
        float init_pressure = get_initial_pressure();
        xTaskCreate(get_pressure_task, "get_pressure_task", 2048 * 4, &init_pressure, 4, NULL);
    } else {
        ESP_LOGI("Barometer", "disabled");
    }

    xTaskCreate(kalman_filter_task, "kalman_filter_task", 2048 * 4, &image, 5, NULL);
    xTaskCreate(save_state_vectors_task, "save_state_vectors", 2048, ekf13, 6, NULL);

    while (1) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
