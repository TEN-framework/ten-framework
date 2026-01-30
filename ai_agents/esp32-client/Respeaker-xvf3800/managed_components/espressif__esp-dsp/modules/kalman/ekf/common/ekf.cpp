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

#include "ekf.h"
#include <float.h>

ekf::ekf(int x, int w) : NUMX(x),
    NUMW(w),
    X(*new dspm::Mat(x, 1)),

    F(*new dspm::Mat(x, x)),
    G(*new dspm::Mat(x, w)),
    P(*new dspm::Mat(x, x)),
    Q(*new dspm::Mat(w, w))
{

    this->P *= 0;
    this->Q *= 0;
    this->X *= 0;
    this->X.data[0] = 1; // direction to 0
    this->HP = new float[this->NUMX];
    this->Km = new float[this->NUMX];
    for (size_t i = 0; i < this->NUMX; i++) {
        this->HP[i] = 0;
        this->Km[i] = 0;
    }
}

ekf::~ekf()
{
    delete &X;
    delete &F;
    delete &G;
    delete &P;
    delete &Q;

    delete this->HP;
    delete this->Km;
}

void ekf::Process(float *u, float dt)
{
    this->LinearizeFG(this->X, (float *)u);
    this->RungeKutta(this->X, u, dt);
    this->CovariancePrediction(dt);
}

void ekf::RungeKutta(dspm::Mat &x, float *U, float dt)
{

    float dt2 = dt / 2.0f;

    dspm::Mat Xlast = x;          // make a working copy
    dspm::Mat K1 = StateXdot(x, U); // k1 = f(x, u)
    x = Xlast + (K1 * dt2);

    dspm::Mat K2 = StateXdot(x, U); // k2 = f(x + 0.5*dT*k1, u)
    x = Xlast + K2 * dt2;

    dspm::Mat K3 = StateXdot(x, U); // k3 = f(x + 0.5*dT*k2, u)
    x = Xlast + K3 * dt;

    dspm::Mat K4 = StateXdot(x, U); // k4 = f(x + dT * k3, u)

    // Xnew = X + dT * (k1 + 2 * k2 + 2 * k3 + k4) / 6
    x = Xlast + (K1 + 2.0f * K2 + 2.0f * K3 + K4) * (dt / 6.0f);
}

dspm::Mat ekf::SkewSym4x4(float w[3])
{
    //={    0,  -w[0],  -w[1],  -w[2],
    //   w[0],      0,   w[2],  -w[1],
    //   w[1],  -w[2],      0,   w[0],
    //   w[2],   w[1],  -w[0],     0 };

    dspm::Mat result(4, 4);
    result.data[0] = 0;
    result.data[1] = -w[0];
    result.data[2] = -w[1];
    result.data[3] = -w[2];

    result.data[4] = w[0];
    result.data[5] = 0;
    result.data[6] = w[2];
    result.data[7] = -w[1];

    result.data[8] = w[1];
    result.data[9] = -w[2];
    result.data[10] = 0;
    result.data[11] = w[0];

    result.data[12] = w[2];
    result.data[13] = w[1];
    result.data[14] = -w[0];
    result.data[15] = 0;

    return result;
}

dspm::Mat ekf::qProduct(float *q)
{
    dspm::Mat result(4, 4);

    result.data[0] = q[0];
    result.data[1] = -q[1];
    result.data[2] = -q[2];
    result.data[3] = -q[3];

    result.data[4] = q[1];
    result.data[5] = q[0];
    result.data[6] = -q[3];
    result.data[7] = q[2];

    result.data[8] = q[2];
    result.data[9] = q[3];
    result.data[10] = q[0];
    result.data[11] = -q[1];

    result.data[12] = q[3];
    result.data[13] = -q[2];
    result.data[14] = q[1];
    result.data[15] = q[0];

    return result;
}

void ekf::CovariancePrediction(float dt)
{
    dspm::Mat f = this->F * dt;

    f = f + dspm::Mat::eye(this->NUMX);

    dspm::Mat f_t = f.t();
    this->P = ((f * this->P) * f_t) + (dt * dt) * ((G * Q) * G.t());
}

void ekf::Update(dspm::Mat &H, float *measured, float *expected, float *R)
{
    float HPHR, Error;
    dspm::Mat Y(measured, H.rows, 1);
    dspm::Mat Z(expected, H.rows, 1);

    for (int m = 0; m < H.rows; m++) {
        for (int j = 0; j < this->NUMX; j++) {
            // Find Hp = H*P
            HP[j] = 0;
        }
        for (int k = 0; k < this->NUMX; k++) {
            for (int j = 0; j < this->NUMX; j++) {
                // Find Hp = H*P
                HP[j] += H(m, k) * P(k, j);
            }
        }
        HPHR = R[m]; // Find  HPHR = H*P*H' + R
        for (int k = 0; k < this->NUMX; k++) {
            HPHR += HP[k] * H(m, k);
        }
        float invHPHR = 1.0f / HPHR;
        for (int k = 0; k < this->NUMX; k++) {
            Km[k] = HP[k] * invHPHR; // find K = HP/HPHR
        }
        for (int i = 0; i < this->NUMX; i++) {
            // Find P(m)= P(m-1) + K*HP
            for (int j = i; j < NUMX; j++) {
                P(i, j) = P(j, i) = P(i, j) - Km[i] * HP[j];
            }
        }

        Error = Y(m, 0) - Z(m, 0);
        for (int i = 0; i < this->NUMX; i++) {
            // Find X(m)= X(m-1) + K*Error
            X(i, 0) = X(i, 0) + Km[i] * Error;
        }
    }
}

void ekf::UpdateRef(dspm::Mat &H, float *measured, float *expected, float *R)
{
    dspm::Mat h_t = H.t();
    dspm::Mat S = H * P * h_t; // +diag(R);
    for (size_t i = 0; i < H.rows; i++) {
        S(i, i) += R[i];
    }

    dspm::Mat S_ = S.pinv(); // 1 / S

    dspm::Mat K = (P * h_t) * S_;
    this->P = (dspm::Mat::eye(this->NUMX) - K * H) * P;

    dspm::Mat Y(measured, H.rows, 1);
    dspm::Mat Z(expected, H.rows, 1);

    dspm::Mat Err = Y - Z;
    this->X += (K * Err);
}

dspm::Mat ekf::quat2rotm(float q[4])
{
    float q0 = q[0];
    float q1 = q[1];
    float q2 = q[2];
    float q3 = q[3];
    dspm::Mat Rm(3, 3);

    Rm(0, 0) = q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3;
    Rm(1, 0) = 2.0f * (q1 * q2 + q0 * q3);
    Rm(2, 0) = 2.0f * (q1 * q3 - q0 * q2);
    Rm(0, 1) = 2.0f * (q1 * q2 - q0 * q3);
    Rm(1, 1) = (q0 * q0 - q1 * q1 + q2 * q2 - q3 * q3);
    Rm(2, 1) = 2.0f * (q2 * q3 + q0 * q1);
    Rm(0, 2) = 2.0f * (q1 * q3 + q0 * q2);
    Rm(1, 2) = 2.0f * (q2 * q3 - q0 * q1);
    Rm(2, 2) = (q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3);

    return Rm;
}

dspm::Mat ekf::quat2eul(const float q[4])
{
    dspm::Mat result(3, 1);
    float R13, R11, R12, R23, R33;
    float q0s = q[0] * q[0];
    float q1s = q[1] * q[1];
    float q2s = q[2] * q[2];
    float q3s = q[3] * q[3];

    R13 = 2.0f * (q[1] * q[3] + q[0] * q[2]);
    R11 = q0s + q1s - q2s - q3s;
    R12 = -2.0f * (q[1] * q[2] - q[0] * q[3]);
    R23 = -2.0f * (q[2] * q[3] - q[0] * q[1]);
    R33 = q0s - q1s - q2s + q3s;

    result.data[1] = (asinf(R13));
    result.data[2] = (atan2f(R12, R11));
    result.data[0] = (atan2f(R23, R33));
    return result;
}

dspm::Mat ekf::eul2rotm(float xyz[3])
{
    dspm::Mat result(3, 3);
    float Cx = std::cos(xyz[0]);
    float Sx = std::sin(xyz[0]);
    float Cy = std::cos(xyz[1]);
    float Sy = std::sin(xyz[1]);
    float Cz = std::cos(xyz[2]);
    float Sz = std::sin(xyz[2]);

    result(0, 0) = Cy * Cz;
    result(0, 1) = -Cy * Sz;
    result(0, 2) = Sy;

    result(1, 0) = Cz * Sx * Sy + Cx * Sz;
    result(1, 1) = Cx * Cz - Sx * Sy * Sz;
    result(1, 2) = -Cy * Sx;

    result(2, 0) = -Cx * Cz * Sy + Sx * Sz;
    result(2, 1) = Cz * Sx + Cx * Sy * Sz;
    result(2, 2) = Cx * Cy;
    return result;
}

#ifndef FLT_EPSILON
#define FLT_EPSILON 1.192092896e-07F
#endif // FLT_EPSILON

dspm::Mat ekf::rotm2eul(dspm::Mat &rotm)
{
    dspm::Mat result(3, 1);
    float x, y, z;
//    float cy = sqrtf(rotm(2, 2) * rotm(2, 2) + rotm(2, 0) * rotm(2, 0));
    float cy = sqrtf(rotm(2, 2) * rotm(2, 2) + rotm(1, 2) * rotm(1, 2));
    if (cy > 16 * FLT_EPSILON) {
        x = -atan2f(rotm(1, 2), rotm(2, 2));
        y = -atan2f(-rotm(0, 2), (float)cy);
        z = -atan2f(rotm(0, 1), rotm(0, 0));
    } else {
        z = -atan2f(-rotm(1, 0), rotm(1, 1));
        y = -atan2f(-rotm(0, 2), (float)cy);
        x = 0;
    }
    result(0, 0) = x;
    result(1, 0) = y;
    result(2, 0) = z;
    return result;
}

static inline float SIGN(float x)
{
    return (x >= 0.0f) ? +1.0f : -1.0f;
}

dspm::Mat ekf::rotm2quat(dspm::Mat &m)
{
    float r11 = m(0, 0);
    float r12 = m(0, 1);
    float r13 = m(0, 2);
    float r21 = m(1, 0);
    float r22 = m(1, 1);
    float r23 = m(1, 2);
    float r31 = m(2, 0);
    float r32 = m(2, 1);
    float r33 = m(2, 2);
    float q0 = (r11 + r22 + r33 + 1.0f) / 4.0f;
    float q1 = (r11 - r22 - r33 + 1.0f) / 4.0f;
    float q2 = (-r11 + r22 - r33 + 1.0f) / 4.0f;
    float q3 = (-r11 - r22 + r33 + 1.0f) / 4.0f;
    if (q0 < 0.0f) {
        q0 = 0.0f;
    }
    if (q1 < 0.0f) {
        q1 = 0.0f;
    }
    if (q2 < 0.0f) {
        q2 = 0.0f;
    }
    if (q3 < 0.0f) {
        q3 = 0.0f;
    }
    q0 = sqrt(q0);
    q1 = sqrt(q1);
    q2 = sqrt(q2);
    q3 = sqrt(q3);
    if (q0 >= q1 && q0 >= q2 && q0 >= q3) {
        q0 *= +1.0f;
        q1 *= SIGN(r32 - r23);
        q2 *= SIGN(r13 - r31);
        q3 *= SIGN(r21 - r12);
    } else if (q1 >= q0 && q1 >= q2 && q1 >= q3) {
        q0 *= SIGN(r32 - r23);
        q1 *= +1.0f;
        q2 *= SIGN(r21 + r12);
        q3 *= SIGN(r13 + r31);
    } else if (q2 >= q0 && q2 >= q1 && q2 >= q3) {
        q0 *= SIGN(r13 - r31);
        q1 *= SIGN(r21 + r12);
        q2 *= +1.0f;
        q3 *= SIGN(r32 + r23);
    } else if (q3 >= q0 && q3 >= q1 && q3 >= q2) {
        q0 *= SIGN(r21 - r12);
        q1 *= SIGN(r31 + r13);
        q2 *= SIGN(r32 + r23);
        q3 *= +1.0f;
    }

    dspm::Mat res(4, 1);
    res(0, 0) = q0;
    res(1, 0) = q1;
    res(2, 0) = q2;
    res(3, 0) = q3;
    res /= res.norm();
    return res;
}

dspm::Mat ekf::dFdq(dspm::Mat &vector, dspm::Mat &q)
{
    dspm::Mat result(3, 4);
    result(0, 0) = q.data[0] * vector.data[0] - q.data[3] * vector.data[1] + q.data[2] * vector.data[2];
    result(0, 1) = q.data[1] * vector.data[0] + q.data[2] * vector.data[1] + q.data[3] * vector.data[2];
    result(0, 2) = -q.data[2] * vector.data[0] + q.data[1] * vector.data[1] + q.data[0] * vector.data[2];
    result(0, 3) = -q.data[3] * vector.data[0] - q.data[0] * vector.data[1] + q.data[1] * vector.data[2];

    result(1, 0) = q.data[3] * vector.data[0] + q.data[0] * vector.data[1] - q.data[1] * vector.data[2];
    result(1, 1) = q.data[2] * vector.data[0] - q.data[1] * vector.data[1] - q.data[0] * vector.data[2];
    result(1, 2) = q.data[1] * vector.data[0] + q.data[2] * vector.data[1] + q.data[3] * vector.data[2];
    result(1, 3) = q.data[0] * vector.data[0] - q.data[3] * vector.data[1] + q.data[2] * vector.data[2];

    result(2, 0) = -q.data[2] * vector.data[0] + q.data[1] * vector.data[1] + q.data[0] * vector.data[2];
    result(2, 1) = q.data[3] * vector.data[0] + q.data[0] * vector.data[1] - q.data[1] * vector.data[2];
    result(2, 2) = -q.data[0] * vector.data[0] + q.data[3] * vector.data[1] - q.data[2] * vector.data[2];
    result(2, 3) = q.data[1] * vector.data[0] + q.data[2] * vector.data[1] + q.data[3] * vector.data[2];

    result *= 2;
    return result;
}

dspm::Mat ekf::dFdq_inv(dspm::Mat &vector, dspm::Mat &q)
{
    dspm::Mat result(3, 4);
    result(0, 0) = q.data[0] * vector.data[0] + q.data[3] * vector.data[1] - q.data[2] * vector.data[2];
    result(0, 1) = q.data[1] * vector.data[0] + q.data[2] * vector.data[1] + q.data[3] * vector.data[2];
    result(0, 2) = -q.data[2] * vector.data[0] + q.data[1] * vector.data[1] - q.data[0] * vector.data[2];
    result(0, 3) = -q.data[3] * vector.data[0] + q.data[0] * vector.data[1] + q.data[1] * vector.data[2];

    result(1, 0) = -q.data[3] * vector.data[0] + q.data[0] * vector.data[1] + q.data[1] * vector.data[2];
    result(1, 1) = q.data[2] * vector.data[0] - q.data[1] * vector.data[1] + q.data[0] * vector.data[2];
    result(1, 2) = q.data[1] * vector.data[0] + q.data[2] * vector.data[1] + q.data[3] * vector.data[2];
    result(1, 3) = -q.data[0] * vector.data[0] - q.data[3] * vector.data[1] + q.data[2] * vector.data[2];

    result(2, 0) = q.data[2] * vector.data[0] - q.data[1] * vector.data[1] + q.data[0] * vector.data[2];
    result(2, 1) = q.data[3] * vector.data[0] - q.data[0] * vector.data[1] - q.data[1] * vector.data[2];
    result(2, 2) = q.data[0] * vector.data[0] + q.data[3] * vector.data[1] - q.data[2] * vector.data[2];
    result(2, 3) = q.data[1] * vector.data[0] + q.data[2] * vector.data[1] + q.data[3] * vector.data[2];

    result *= 2;
    return result;
}

dspm::Mat ekf::StateXdot(dspm::Mat &x, float *u)
{
    dspm::Mat U(u, this->G.cols, 1);
    dspm::Mat Xdot = (this->F * x + this->G * U);
    return Xdot;
}
