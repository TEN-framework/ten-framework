// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dsp_platform.h"
#include "esp_log.h"

#include "esp_dsp.h"
#include "ekf_imu13states.h"

static const char *TAG = "main";

extern "C" void app_main();

// This example reproduce system with gyroscope, accelerometer, and magnetometer
// True gyroscope values will be transformed and applied to the rotation and reference measurements.
void app_main()
{
    ekf_imu13states *ekf13 = new  ekf_imu13states();
    ekf13->Init();
    ESP_LOGI(TAG, "Start Example.");

    // Set up some initial values to emulate and calculate system values
    int total_N = 3000;
    // Pi value
    float pi = std::atan(1) * 4;

    // gyroscope bias error
    float gyro_err_data[] = {0.1, 0.2, 0.3}; // static constatnt error
    dspm::Mat gyro_err(gyro_err_data, 3, 1);


    // Measurement noise covariance values for diagonal covariance matrix.
    // For the real system these values could be adjusted!
    // These calues depends on how noisy the measurement.
    //
    float R[10];
    for (size_t i = 0; i < 10; i++) {
        R[i] = 0.01;
    }

    // Reference vectors
    float accel0_data[] = {0, 0, 1};
    // In real system magnetometer vector will have different value and direction
    // The EKF will calculate them. This value is used as initial state.
    float magn0_data[] = {1, 0, 0};

    dspm::Mat accel0(accel0_data, 3, 1);
    dspm::Mat magn0(magn0_data, 3, 1);

    float dt = 0.01;

    dspm::Mat gyro_data(3, 1);
    int count = 0;

    // Initial rotation matrix
    dspm::Mat Rm = dspm::Mat::eye(3);
    dspm::Mat Re = dspm::Mat::eye(3);

    gyro_err *= 1;

    std::cout << "Gyro error: " << gyro_err.t() << std::endl;
    std::cout << "Calibration phase started: " << std::endl;
    for (size_t n = 1; n < total_N * 16; n++) {
        if ((n % 1000) == 0) {
            std::cout << "Loop " << n << " from " << total_N * 16;
            std::cout << ", State data : " << ekf13->X.t();
        }
        //
        // This part of the loop related to the system emulation
        //

        // Generate gyro values for system emulation
        gyro_data *= 0; // reset gyro value
        if ((n >= (total_N / 2)) && (n < total_N * 12)) {
            gyro_data(0, 0) = 1 / pi * std::cos(-pi / 2 + pi / 2 * count * 2 / (total_N / 10));
            gyro_data(1, 0) = 2 / pi * std::cos(-pi / 2 + pi / 2 * count * 2 / (total_N / 10));
            gyro_data(2, 0) = 3 / pi * std::cos(-pi / 2 + pi / 2 * count * 2 / (total_N / 10));
            count++;
        }
        dspm::Mat gyro_sample = gyro_data + gyro_err;

        gyro_data *= dt;
        // Calculate rotation for the last time interval
        Re = ekf::eul2rotm(gyro_data.data);
        // Ally rotation to the system rotation matrix
        Rm = Rm * Re;
        // Convert rotation matrix to the system attitude quaternion
        dspm::Mat attitude = ekf::rotm2quat(Rm);
        // We have to rotate accel and magn to the opposite direction
        dspm::Mat accel_data = Rm.t() * accel0;
        dspm::Mat magn_data = Rm.t() * magn0;

        dspm::Mat accel_norm = accel_data / accel_data.norm();
        dspm::Mat magn_norm = magn_data / magn_data.norm();

        //
        // This part of the loop related to the real system
        // Here gyro_sample values must be replaced by measured gyroscope values
        // and accel_norm and magn_norm should be real measured accel and magn values
        // The dt in this case should be real time difference in seconds between samples
        // Fill the input control values with measured gyro values
        float input_u[] = {gyro_sample(0, 0), gyro_sample(1, 0), gyro_sample(2, 0)};
        // Process input values to new state
        ekf13->Process(input_u, dt);
        dspm::Mat q_norm(ekf13->X.data, 4, 1);
        q_norm /= q_norm.norm();
        // Correct state and calculate gyro and magnetometer values.
        // Here accel_norm and magn_norm should be real measured accel and magn values
        ekf13->UpdateRefMeasurementMagn(accel_norm.data, magn_norm.data, R);
    }
    std::cout << "Calibration phase finished." << std::endl << std::endl;

    std::cout << "Regular calculation started:" << std::endl;

    // Reset rotation nmatrix
    Rm = dspm::Mat::eye(3);
    Re = dspm::Mat::eye(3);

    count = 0;
    // Set initial state
    ekf13->X(0, 0) = 1;
    ekf13->X(0, 1) = 0;
    ekf13->X(0, 2) = 0;
    ekf13->X(0, 3) = 0;

    for (size_t n = 1; n < total_N * 16; n++) {
        if ((n % 1000) == 0) {
            std::cout << "Loop " << n << " from " << total_N * 16;
            std::cout << ", State data : " << ekf13->X.t();
        }
        //
        // This part of the loop related to the system emulation
        //

        // Generate gyro values for system emulation
        gyro_data *= 0; // reset gyro value
        if ((n >= (total_N / 2)) && (n < total_N * 12)) {
            gyro_data(0, 0) = 1 / pi * std::cos(-pi / 2 + pi / 2 * count * 2 / (total_N / 10));
            gyro_data(1, 0) = 2 / pi * std::cos(-pi / 2 + pi / 2 * count * 2 / (total_N / 10));
            gyro_data(2, 0) = 3 / pi * std::cos(-pi / 2 + pi / 2 * count * 2 / (total_N / 10));
            count++;
        }
        dspm::Mat gyro_sample = gyro_data + gyro_err;

        gyro_data *= dt;
        // Calculate rotation for the last time interval
        Re = ekf::eul2rotm(gyro_data.data);
        // Ally rotation to the system rotation matrix
        Rm = Rm * Re;
        // Convert rotation matrix to the system attitude quaternion
        dspm::Mat attitude = ekf::rotm2quat(Rm);
        // We have to rotate accel and magn to the opposite direction
        dspm::Mat accel_data = Rm.t() * accel0;
        dspm::Mat magn_data = Rm.t() * magn0;

        dspm::Mat accel_norm = accel_data / accel_data.norm();
        dspm::Mat magn_norm = magn_data / magn_data.norm();

        //
        // This part of the loop related to the real system
        // Here gyro_sample values must be replaced by measured gyroscope values
        // and accel_norm and magn_norm should be real measured accel and magn values
        // The dt in this case should be real time difference in seconds between samples
        // Fill the input control values with measured gyro values
        float input_u[] = {gyro_sample(0, 0), gyro_sample(1, 0), gyro_sample(2, 0)};
        // Process input values to new state
        ekf13->Process(input_u, dt);
        dspm::Mat q_norm(ekf13->X.data, 4, 1);
        q_norm /= q_norm.norm();
        // Correct state and calculate gyro and magnetometer values.
        // Here accel_norm and magn_norm should be real measured accel and magn values
        ekf13->UpdateRefMeasurement(accel_norm.data, magn_norm.data, R);
    }

    std::cout << "Final State data : " << ekf13->X.t();
    dspm::Mat estimated_error(&ekf13->X.data[4], 3, 1);
    std::cout << "Estimated error : " << estimated_error.t();
    std::cout << "Difference between real and estimated errors : " << (gyro_err - estimated_error).t() << std::endl;

    std::cout << "Expected Euler angels (degree) : " << (180 / pi * ekf::quat2eul(ekf::rotm2quat(Rm).data)).t();
    std::cout << "Calculated Euler angels (degree) : " << (180 / pi * ekf::quat2eul(ekf13->X.data)).t() << std::endl;
}
