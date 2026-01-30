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

#include "ekf_imu13states.h"

ekf_imu13states::ekf_imu13states() : ekf(13, 18),
    mag0(3, 1),
    accel0(3, 1)
{
    this->NUMU = 3;
}

ekf_imu13states::~ekf_imu13states()
{
}

void ekf_imu13states::Init()
{
    mag0.data[0] = 1;
    mag0.data[1] = 0;
    mag0.data[2] = 0;

    mag0 /= mag0.norm();

    accel0.data[0] = 0;
    accel0.data[1] = 0;
    accel0.data[2] = 1;

    accel0 /= accel0.norm();

    this->Q.Copy(0.1 * dspm::Mat::eye(3), 0, 0);
    this->Q.Copy(0.0001 * dspm::Mat::eye(3), 3, 3);
    this->Q.Copy(0.0001 * dspm::Mat::eye(3), 6, 6);
    this->Q.Copy(0.0001 * dspm::Mat::eye(3), 9, 9);
    this->Q.Copy(0.00001 * dspm::Mat::eye(3), 12, 12);
    this->Q.Copy(0.00001 * dspm::Mat::eye(3), 15, 15);

    this->X.data[0] = 1; // Init quaternion
    this->X.data[7] = 1; // Initial magnetometer vector
}

dspm::Mat ekf_imu13states::StateXdot(dspm::Mat &x, float *u)
{
    float wx = u[0] - x(4, 0); // subtract the biases on gyros
    float wy = u[1] - x(5, 0);
    float wz = u[2] - x(6, 0);

    float w[] = {wx, wy, wz};
    dspm::Mat q = dspm::Mat(x.data, 4, 1);

    // qdot = Q * w
    dspm::Mat Omega = 0.5 * SkewSym4x4(w);
    dspm::Mat qdot = Omega * q;
    dspm::Mat Xdot(this->NUMX, 1);
    Xdot *= 0;
    Xdot.Copy(qdot, 0, 0);
    // dwbias = 0
    // dMang_Ampl = 0
    // dMang_offset = 0
    return Xdot;
}

void ekf_imu13states::LinearizeFG(dspm::Mat &x, float *u)
{
    float w[3] = {(u[0] - x(4, 0)), (u[1] - x(5, 0)), (u[2] - x(6, 0))}; // subtract the biases on gyros
    // float w[3] = {u[0], u[1], u[2]}; // subtract the biases on gyros

    this->F *= 0; // Initialize F and G matrixes.
    this->G *= 0;

    // dqdot / dq - skey matrix
    F.Copy(0.5 * ekf::SkewSym4x4(w), 0, 0);

    // dqdot/dvector
    dspm::Mat dq = -0.5 * qProduct(x.data);
    dspm::Mat dq_q = dq.Get(0, 4, 1, 3);

    // dqdot / dnw
    G.Copy(dq_q, 0, 0);
    // dqdot / dwbias
    F.Copy(dq_q, 0, 4);

    dspm::Mat rotm = -1 * this->quat2rotm(x.data); // Convert quat to rotation matrix

    G.Copy(rotm, 7, 6);
    G.Copy(dspm::Mat::eye(3), 4, 3);   // random noise wbias
    G.Copy(dspm::Mat::eye(3), 7, 12);  // random noise magnetometer amplitude
    G.Copy(dspm::Mat::eye(3), 10, 9);  // magnetometer offset constant
    G.Copy(dspm::Mat::eye(3), 10, 15); // random noise offset constant
}

void ekf_imu13states::Test()
{
    dspm::Mat test_x(7, 1);
    for (size_t i = 0; i < 7; i++) {
        test_x(i, 0) = i;
    }
    printf("Allocate data = %i\n", this->NUMU);
    float *test_u = new float[this->NUMU];
    for (size_t i = 0; i < this->NUMU; i++) {
        test_u[i] = i;
    }
    dspm::Mat result_StateXdot = StateXdot(test_x, test_u);
    delete[] test_u;
}

void ekf_imu13states::TestFull(bool enable_att)
{
    int total_N = 2048;
    float pi = std::atan(1) * 4;
    float gyro_err_data[] = {0.1, 0.2, 0.3}; // static constatnt error
    dspm::Mat gyro_err(gyro_err_data, 3, 1);
    float R[10];
    for (size_t i = 0; i < 10; i++) {
        R[i] = 0.01;
    }

    float accel0_data[] = {0, 0, 1};
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
    for (size_t n = 1; n < total_N * 3; n++) {
        if ((n % 1000) == 0) {
            std::cout << "Loop " << n << " from " << total_N * 16;
            std::cout << ", State data : " << this->X.t();
        }
        gyro_data *= 0; // reset gyro value
        if ((n >= (total_N / 2)) && (n < total_N * 12)) {
            gyro_data(0, 0) = 1 / pi * std::cos(-pi / 2 + pi / 2 * count * 2 / (total_N / 10));
            gyro_data(1, 0) = 2 / pi * std::cos(-pi / 2 + pi / 2 * count * 2 / (total_N / 10));
            gyro_data(2, 0) = 3 / pi * std::cos(-pi / 2 + pi / 2 * count * 2 / (total_N / 10));
            count++;
        }
        dspm::Mat gyro_sample = gyro_data + gyro_err;

        gyro_data *= dt;
        Re = this->eul2rotm(gyro_data.data); // Calculate rotation to gyro angel
        Rm = Rm * Re;                        // Rotate original matrix
        dspm::Mat attitude = ekf::rotm2quat(Rm);
        // We have to rotate accel and magn to the opposite direction
        dspm::Mat accel_data = Rm.t() * accel0;
        dspm::Mat magn_data = Rm.t() * magn0;

        dspm::Mat accel_norm = accel_data / accel_data.norm();
        dspm::Mat magn_norm = magn_data / magn_data.norm();

        float input_u[] = {gyro_sample(0, 0), gyro_sample(1, 0), gyro_sample(2, 0)};
        // Process input values to new state
        this->Process(input_u, dt);
        dspm::Mat q_norm(this->X.data, 4, 1);
        q_norm /= q_norm.norm();

        if (true == enable_att) {
            this->UpdateRefMeasurement(accel_norm.data, magn_norm.data, attitude.data, R);
        } else {
            this->UpdateRefMeasurement(accel_norm.data, magn_norm.data, R);
        }
    }
    std::cout << "Final State data : " << this->X.t() << std::endl;
}

void ekf_imu13states::UpdateRefMeasurement(float *accel_data, float *magn_data, float R[6])
{
    dspm::Mat quat(this->X.data, 4, 1);
    dspm::Mat H = 0 * dspm::Mat(6, this->NUMX);
    dspm::Mat Re = this->quat2rotm(quat.data).t();

    // dAccel/dq
    dspm::Mat dAccel_dq = ekf::dFdq_inv(this->accel0, quat);
    H.Copy(dAccel_dq, 3, 0);

    // dMagn/dq
    dspm::Mat magn(&this->X.data[7], 3, 1);
    dspm::Mat magn_offset(&this->X.data[10], 3, 1);
    dspm::Mat dMagn_dq = ekf::dFdq_inv(magn, quat);
    H.Copy(dMagn_dq, 0, 0);

    dspm::Mat expected_magn = Re * magn + magn_offset;
    dspm::Mat expected_accel = Re * this->accel0;

    float measured_data[6];
    float expected_data[6];
    for (size_t i = 0; i < 3; i++) {
        measured_data[i] = magn_data[i];
        expected_data[i] = expected_magn.data[i];
        measured_data[i + 3] = accel_data[i];
        expected_data[i + 3] = expected_accel.data[i];
    }

    this->Update(H, measured_data, expected_data, R);
    quat /= quat.norm();
}

void ekf_imu13states::UpdateRefMeasurementMagn(float *accel_data, float *magn_data, float R[6])
{
    dspm::Mat quat(this->X.data, 4, 1);
    dspm::Mat H = 0 * dspm::Mat(6, this->NUMX);
    dspm::Mat Re = this->quat2rotm(quat.data).t();

    // We include these two line to update magnetometer initial state
    H.Copy(Re, 0, 7);
    H.Copy(dspm::Mat::eye(3), 0, 10);

    // dAccel/dq
    dspm::Mat dAccel_dq = ekf::dFdq_inv(this->accel0, quat);
    H.Copy(dAccel_dq, 3, 0);

    // dMagn/dq
    dspm::Mat magn(&this->X.data[7], 3, 1);
    dspm::Mat magn_offset(&this->X.data[10], 3, 1);
    dspm::Mat dMagn_dq = ekf::dFdq_inv(magn, quat);
    H.Copy(dMagn_dq, 0, 0);

    dspm::Mat expected_magn = Re * magn + magn_offset;
    dspm::Mat expected_accel = Re * this->accel0;

    float measured_data[6];
    float expected_data[6];
    for (size_t i = 0; i < 3; i++) {
        measured_data[i] = magn_data[i];
        expected_data[i] = expected_magn.data[i];
        measured_data[i + 3] = accel_data[i];
        expected_data[i + 3] = expected_accel.data[i];
    }

    this->Update(H, measured_data, expected_data, R);
    quat /= quat.norm();
}

void ekf_imu13states::UpdateRefMeasurement(float *accel_data, float *magn_data, float *attitude, float R[10])
{
    dspm::Mat quat(this->X.data, 4, 1);
    dspm::Mat H = 0 * dspm::Mat(10, this->NUMX);
    dspm::Mat Re = this->quat2rotm(quat.data).t();

    H.Copy(Re, 0, 7);
    H.Copy(dspm::Mat::eye(3), 0, 10);
    // dAccel/dq
    dspm::Mat dAccel_dq = ekf::dFdq_inv(this->accel0, quat);
    H.Copy(dAccel_dq, 3, 0);
    // dMagn/dq
    dspm::Mat magn(&this->X.data[7], 3, 1);
    dspm::Mat magn_offset(&this->X.data[10], 3, 1);
    dspm::Mat dMagn_dq = ekf::dFdq_inv(magn, quat);
    H.Copy(dMagn_dq, 0, 0);

    // dq/dq
    H.Copy(dspm::Mat::eye(4), 6, 1);

    dspm::Mat expected_magn = Re * magn + magn_offset;
    dspm::Mat expected_accel = Re * this->accel0;

    float measured_data[10];
    float expected_data[10];
    for (size_t i = 0; i < 3; i++) {
        measured_data[i] = magn_data[i];
        expected_data[i] = expected_magn.data[i];
        measured_data[i + 3] = accel_data[i];
        expected_data[i + 3] = expected_accel.data[i];
    }
    for (size_t i = 0; i < 4; i++) {
        measured_data[i + 6] = attitude[i];
        expected_data[i + 6] = this->X.data[i];
    }

    this->Update(H, measured_data, expected_data, R);
    quat /= quat.norm();
}
