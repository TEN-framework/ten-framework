// Copyright 2018-2023 Espressif Systems (Shanghai) PTE LTD
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

#include <stdexcept>
#include <string.h>
#include "mat.h"
#include "esp_log.h"

#include "dsps_math.h"
#include "dspm_matrix.h"
#include <math.h>
#include <cmath>
#include <inttypes.h>



using std::ostream;
using std::istream;
using std::endl;

namespace dspm {

float Mat::abs_tol = 1e-10;

Mat::Rect::Rect(int x, int y, int width, int height)
{
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;
}

void Mat::Rect::resizeRect(int x, int y, int width, int height)
{
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;
}

int Mat::Rect::areaRect(void)
{
    return this->width * this->height;
}

Mat::Mat(float *data, int roi_rows, int roi_cols, int stride)
{
    this->rows = roi_rows;
    this->cols = roi_cols;
    this->stride = stride;
    this->padding = stride - roi_cols;
    this->length = this->rows * this->cols;
    this->sub_matrix = true;
    this->ext_buff = true;
    this->data = data;
}

Mat::Mat(int rows, int cols)
{
    ESP_LOGD("Mat", "Mat(%i, %i)", rows, cols);
    this->rows = rows;
    this->cols = cols;
    this->sub_matrix = false;
    this->stride = cols;
    this->padding = 0;
    allocate();
    memset(this->data, 0, this->length * sizeof(float));
}

Mat::Mat(float *data, int rows, int cols)
{
    ESP_LOGD("Mat", "Mat(data, %i, %i)", rows, cols);
    this->rows = rows;
    this->cols = cols;
    this->sub_matrix = false;
    this->stride = cols;
    this->padding = 0;
    this->length = this->rows * this->cols;
    allocate();
    this->ext_buff = false;
    for (size_t i = 0; i < this->length; i++) {
        this->data[i] = data[i];
    }
}


Mat::Mat()
{
    this->rows = 1;
    this->cols = 1;
    this->sub_matrix = false;
    this->stride = cols;
    this->padding = 0;
    ESP_LOGD("Mat", "Mat()");

    allocate();
    this->data[0] = 0;
}

Mat::~Mat()
{
    ESP_LOGD("Mat", "~Mat(%i, %i), ext_buff=%i, data = %p", this->rows, this->cols, this->ext_buff, this->data);
    if (false == this->ext_buff) {
        delete[] data;
    }
}

Mat::Mat(const Mat &m)
{
    this->rows = m.rows;
    this->cols = m.cols;
    this->padding = m.padding;
    this->stride = m.stride;
    this->data = m.data;
    this->sub_matrix = m.sub_matrix;

    if (m.sub_matrix) {
        this->length = m.length;
        this->data = m.data;
        this->ext_buff = true;
    } else {
        allocate();
        memcpy(this->data, m.data, this->length * sizeof(float));
    }
}

Mat Mat::getROI(int startRow, int startCol, int roiRows, int roiCols, int stride)
{
    Mat result(this->data, roiRows, roiCols, 0);

    if ((startRow + roiRows) > this->rows) {
        return result;
    }
    if ((startCol + roiCols) > this->cols) {
        return result;
    }

    const int ptr_move = startRow * this->cols + startCol;
    float *new_data_ptr = this->data + ptr_move;

    result.data = new_data_ptr;
    result.stride = stride;
    result.padding = result.stride - result.cols;
    return result;
}

Mat Mat::getROI(const Mat::Rect &rect)
{
    return (getROI(rect.y, rect.x, rect.height, rect.width, this->cols));
}

Mat Mat::getROI(int startRow, int startCol, int roiRows, int roiCols)
{
    return (getROI(startRow, startCol, roiRows, roiCols, this->cols));
}

void Mat::Copy(const Mat &src, int row_pos, int col_pos)
{
    if ((row_pos + src.rows) > this->rows) {
        return;
    }
    if ((col_pos + src.cols) > this->cols) {
        return;
    }

    for (size_t r = 0; r < src.rows; r++) {
        memcpy(&this->data[(r + row_pos) * this->stride + col_pos], &src.data[r * src.cols], src.cols * sizeof(float));
    }
}

void Mat::CopyHead(const Mat &src)
{
    if (!this->ext_buff) {
        delete[] this->data;
    }
    this->rows = src.rows;
    this->cols = src.cols;
    this->length = src.length;
    this->padding = src.padding;
    this->stride = src.stride;
    this->data = src.data;
    this->ext_buff = src.ext_buff;
    this->sub_matrix = src.sub_matrix;
}

void Mat::PrintHead(void)
{
    std::cout << "rows     " << this->rows << std::endl;
    std::cout << "cols     " << this->cols << std::endl;
    std::cout << "lenght   " << this->length << std::endl;
    std::cout << "data     " << this->data << std::endl;
    std::cout << "ext_buff " << this->ext_buff << std::endl;
    std::cout << "sub_mat  " << this->sub_matrix << std::endl;
    std::cout << "stride   " << this->stride << std::endl;
    std::cout << "padding  " << this->padding << std::endl << std::endl;
}

Mat Mat::Get(int row_start, int row_size, int col_start, int col_size)
{
    Mat result(row_size, col_size);

    if ((row_start + row_size) > this->rows) {
        return result;
    }
    if ((col_start + col_size) > this->cols) {
        return result;
    }

    for (size_t r = 0; r < result.rows; r++) {
        memcpy(&result.data[r * result.cols], &this->data[(r + row_start) * this->stride + col_start], result.cols * sizeof(float));
    }
    return result;
}

Mat Mat::Get(const Mat::Rect &rect)
{
    return (Get(rect.y, rect.height, rect.x, rect.width));
}

Mat &Mat::operator=(const Mat &m)
{
    if (this == &m) {
        return *this;
    }

    // matrix dimensions not equal
    if (this->rows != m.rows || this->cols != m.cols) {
        // left operand is a sub-matrix - error
        if (this->sub_matrix) {
            ESP_LOGE("Mat", "operator = Error for sub-matrices: operands matrices dimensions %dx%d and %dx%d do not match", this->rows, this->cols, m.rows, m.cols);
            return *this;
        }
        if (!this->ext_buff) {
            delete[] this->data;
        }
        this->ext_buff = false;
        this->rows = m.rows;
        this->cols = m.cols;
        this->stride = this->cols;
        this->padding = 0;
        this->sub_matrix = false;
        allocate();
    }

    for (int row = 0; row < this->rows; row++) {
        memcpy(this->data + (row * this->stride), m.data + (row * m.stride), this->cols * sizeof(float));
    }
    return *this;
}

Mat &Mat::operator+=(const Mat &m)
{
    if ((this->rows != m.rows) || (this->cols != m.cols)) {
        ESP_LOGW("Mat", "operator += Error: matrices do not have equal dimensions");
        return *this;
    }

    if (this->sub_matrix || m.sub_matrix) {
        dspm_add_f32(this->data, m.data, this->data, this->rows, this->cols, this->padding, m.padding, this->padding, 1, 1, 1);
    } else {
        dsps_add_f32(this->data, m.data, this->data, this->length, 1, 1, 1);
    }
    return *this;
}

Mat &Mat::operator+=(float C)
{
    if (this->sub_matrix) {
        dspm_addc_f32(this->data, this->data, C, this->rows, this->cols, this->padding, this->padding, 1, 1);
    } else {
        dsps_addc_f32_ansi(this->data, this->data, this->length, C, 1, 1);
    }
    return *this;
}

Mat &Mat::operator-=(const Mat &m)
{
    if ((this->rows != m.rows) || (this->cols != m.cols)) {
        ESP_LOGW("Mat", "operator -= Error: matrices do not have equal dimensions");
        return *this;
    }

    if (this->sub_matrix || m.sub_matrix) {
        dspm_sub_f32(this->data, m.data, this->data, this->rows, this->cols, this->padding, m.padding, this->padding, 1, 1, 1);
    } else {
        dsps_sub_f32(this->data, m.data, this->data, this->length, 1, 1, 1);
    }
    return *this;
}

Mat &Mat::operator-=(float C)
{
    if (this->sub_matrix) {
        dspm_addc_f32(this->data, this->data, -C, this->rows, this->cols, this->padding, this->padding, 1, 1);
    } else {
        dsps_addc_f32_ansi(this->data, this->data, this->length, -C, 1, 1);
    }
    return *this;
}

Mat &Mat::operator*=(const Mat &m)
{
    if (this->cols != m.rows) {
        ESP_LOGW("Mat", "operator *= Error: matrices do not have equal dimensions");
        return *this;
    }

    if (this->sub_matrix || m.sub_matrix) {
        Mat temp = this->Get(0, this->rows, 0, this->cols);
        dspm_mult_ex_f32(temp.data, m.data, this->data, temp.rows, temp.cols, m.cols, temp.padding, m.padding, this->padding);
    } else {
        Mat temp = *this;
        dspm_mult_f32(temp.data, m.data, this->data, temp.rows, temp.cols, m.cols);
    }
    return (*this);
}

Mat &Mat::operator*=(float num)
{
    if (this->sub_matrix) {
        dspm_mulc_f32(this->data, this->data, num, this->rows, this->cols, this->padding, this->padding, 1, 1);
    } else {
        dsps_mulc_f32_ansi(this->data, this->data, this->length, num, 1, 1);
    }
    return *this;
}

Mat &Mat::operator/=(const Mat &B)
{
    if ((this->rows != B.rows) || (this->cols != B.cols)) {
        ESP_LOGW("Mat", "operator /= Error: matrices do not have equal dimensions");
        return *this;
    }

    for (int row = 0; row < this->rows; row++) {
        for (int col = 0; col < this->cols; col++) {
            (*this)(row, col) = (*this)(row, col) / B(row, col);
        }
    }
    return (*this);
}

Mat &Mat::operator/=(float num)
{
    if (this->sub_matrix) {
        dspm_mulc_f32(this->data, this->data, 1 / num, this->rows, this->cols, this->padding, this->padding, 1, 1);
    } else {
        dsps_mulc_f32_ansi(this->data, this->data, this->length, 1 / num, 1, 1);
    }
    return *this;
}

Mat Mat::operator^(int num)
{
    Mat temp(*this);
    return expHelper(temp, num);
}

void Mat::swapRows(int r1, int r2)
{
    if ((this->rows <= r1) || (this->rows <= r2)) {
        ESP_LOGW("Mat", "swapRows Error: row %d or %d out of matrix row %d range", r1, r2, this->rows);
    } else {
        for (int i = 0; i < this->cols; i++) {
            float temp = this->data[r1 * this->stride + i];
            this->data[r1 * this->stride + i] = this->data[r2 * this->stride + i];
            this->data[r2 * this->stride + i] = temp;
        }
    }
}

Mat Mat::t()
{
    Mat ret(this->cols, this->rows);
    for (int i = 0; i < this->rows; ++i) {
        for (int j = 0; j < this->cols; ++j) {
            ret(j, i) = this->data[i * this->stride + j];
        }
    }
    return ret;
}

Mat Mat::eye(int size)
{
    Mat temp(size, size);
    for (int i = 0; i < temp.rows; ++i) {
        for (int j = 0; j < temp.cols; ++j) {
            if (i == j) {
                temp(i, j) = 1;
            } else {
                temp(i, j) = 0;
            }
        }
    }
    return temp;
}

Mat Mat::ones(int size)
{
    return (ones(size, size));
}

Mat Mat::ones(int rows, int cols)
{
    Mat temp(rows, cols);
    for (int row = 0; row < temp.rows; ++row) {
        for (int col = 0; col < temp.cols; ++col) {
            temp(row, col) = 1;
        }
    }
    return temp;
}

void Mat::clear()
{
    for (int row = 0; row < this->rows; row++) {
        memset(this->data + (row * this->stride), 0, this->cols * sizeof(float));
    }
}

// Duplicate to Get method
Mat Mat::block(int startRow, int startCol, int blockRows, int blockCols)
{
    Mat result(blockRows, blockCols);
    for (int i = 0; i < blockRows; ++i) {
        for (int j = 0; j < blockCols; ++j) {
            result(i, j) = (*this)(startRow + i, startCol + j);
        }
    }
    return result;
}

void Mat::normalize(void)
{
    float sqr_norm = 0;
    for (int i = 0; i < this->rows; ++i) {
        for (int j = 0; j < this->cols; ++j) {
            sqr_norm += (*this)(i, j) * (*this)(i, j);
        }
    }
    sqr_norm = 1 / sqrtf(sqr_norm);
    *this *= sqr_norm;
}

float Mat::norm(void)
{
    float sqr_norm = 0;
    for (int i = 0; i < this->rows; ++i) {
        for (int j = 0; j < this->cols; ++j) {
            sqr_norm += (*this)(i, j) * (*this)(i, j);
        }
    }
    sqr_norm = sqrtf(sqr_norm);
    return sqr_norm;
}

Mat Mat::solve(Mat A, Mat b)
{
    // Gaussian elimination
    for (int i = 0; i < A.rows; ++i) {
        if (A(i, i) == 0) {
            // pivot 0 - error
            ESP_LOGW("Mat", "Error: the coefficient matrix has 0 as a pivot. Please fix the input and try again.");
            Mat err_result(0, 0);
            return err_result;
        }
        float a_ii = 1 / A(i, i);
        for (int j = i + 1; j < A.rows; ++j) {
            float a_ji = A(j, i) * a_ii;
            for (int k = i + 1; k < A.cols; ++k) {
                A(j, k) -= A(i, k) * a_ji;
                if ((A(j, k) < abs_tol) && (A(j, k) > -1 * abs_tol)) {
                    A(j, k) = 0;
                }
            }
            b(j, 0) -= b(i, 0) * a_ji;
            if (A(j, 0) < abs_tol && A(j, 0) > -1 * abs_tol) {
                A(j, 0) = 0;
            }
            A(j, i) = 0;
        }
    }

    // Back substitution
    Mat x(b.rows, 1);
    x((x.rows - 1), 0) = b((x.rows - 1), 0) / A((x.rows - 1), (x.rows - 1));
    if (x((x.rows - 1), 0) < abs_tol && x((x.rows - 1), 0) > -1 * abs_tol) {
        x((x.rows - 1), 0) = 0;
    }
    for (int i = x.rows - 2; i >= 0; --i) {
        float sum = 0;
        for (int j = i + 1; j < x.rows; ++j) {
            sum += A(i, j) * x(j, 0);
        }
        x(i, 0) = (b(i, 0) - sum) / A(i, i);
        if (x(i, 0) < abs_tol && x(i, 0) > -1 * abs_tol) {
            x(i, 0) = 0;
        }
    }
    return x;
}

Mat Mat::bandSolve(Mat A, Mat b, int k)
{
    // optimized Gaussian elimination
    int bandsBelow = (k - 1) / 2;
    for (int i = 0; i < A.rows; ++i) {
        if (A(i, i) == 0) {
            // pivot 0 - error
            ESP_LOGW("Mat", "Error: the coefficient matrix has 0 as a pivot. Please fix the input and try again.");
            Mat err_result(b.rows, 1);
            memset(err_result.data, 0, b.rows * sizeof(float));
            return err_result;
        }
        float a_ii = 1 / A(i, i);
        for (int j = i + 1; j < A.rows && j <= i + bandsBelow; ++j) {
            int k = i + 1;
            while ((k < A.cols) && (fabs(A(j, k)) > abs_tol)) {
                A(j, k) -= A(i, k) * (A(j, i) * a_ii);
                k++;
            }
            b(j, 0) -= b(i, 0) * (A(j, i) * a_ii);
            A(j, i) = 0;
        }
    }

    // Back substitution
    Mat x(b.rows, 1);
    x((x.rows - 1), 0) = b((x.rows - 1), 0) / A((x.rows - 1), (x.rows - 1));
    for (int i = x.rows - 2; i >= 0; --i) {
        float sum = 0;
        for (int j = i + 1; j < x.rows; ++j) {
            sum += A(i, j) * x(j, 0);
        }
        x(i, 0) = (b(i, 0) - sum) / A(i, i);
    }

    return x;
}

Mat Mat::roots(Mat A, Mat y)
{
    int n = A.cols + 1;

    Mat result(y.rows, 1);

    Mat g_m = Mat::augment(A, y);
    for (int j = 0; j < A.cols; j++) {
        float g_jj = 1 / g_m(j, j);
        for (int i = 0; i < A.cols; i++) {
            if (i != j) {
                float c = g_m(i, j) * g_jj;
                for (int k = 0; k < n; k++) {
                    g_m(i, k) = g_m(i, k) - c * g_m(j, k);
                }
            }
        }
    }
    for (int i = 0; i < A.rows; i++) {
        result(i, 0) = g_m(i, A.cols) / g_m(i, i);
    }
    return result;
}

float Mat::dotProduct(Mat a, Mat b)
{
    float sum = 0;
    for (int i = 0; i < a.rows; ++i) {
        sum += (a(i, 0) * b(i, 0));
    }
    return sum;
}

Mat Mat::augment(Mat A, Mat B)
{
    Mat AB(A.rows, A.cols + B.cols);
    for (int i = 0; i < AB.rows; ++i) {
        for (int j = 0; j < AB.cols; ++j) {
            if (j < A.cols) {
                AB(i, j) = A(i, j);
            } else {
                AB(i, j) = B(i, j - A.cols);
            }
        }
    }
    return AB;
}

Mat Mat::gaussianEliminate()
{
    Mat Ab(*this);
    int rows = Ab.rows;
    int cols = Ab.cols;
    int Acols = cols - 1;

    int i = 0; // row tracker
    int j = 0; // column tracker

    // iterate through the rows
    while (i < rows) {
        // find a pivot for the row
        bool pivot_found = false;
        while (j < Acols && !pivot_found) {
            if (Ab(i, j) != 0) { // pivot not equal to 0
                pivot_found = true;
            } else { // check for a possible swap
                int max_row = i;
                float max_val = 0;
                for (int k = i + 1; k < rows; ++k) {
                    float cur_abs = Ab(k, j) >= 0 ? Ab(k, j) : -1 * Ab(k, j);
                    if (cur_abs > max_val) {
                        max_row = k;
                        max_val = cur_abs;
                    }
                }
                if (max_row != i) {
                    Ab.swapRows(max_row, i);
                    pivot_found = true;
                } else {
                    j++;
                }
            }
        }

        // perform elimination as normal if pivot was found
        if (pivot_found) {
            for (int t = i + 1; t < rows; ++t) {
                for (int s = j + 1; s < cols; ++s) {
                    Ab(t, s) = Ab(t, s) - Ab(i, s) * (Ab(t, j) / Ab(i, j));
                    if (Ab(t, s) < abs_tol && Ab(t, s) > -1 * abs_tol) {
                        Ab(t, s) = 0;
                    }
                }
                Ab(t, j) = 0;
            }
        }

        i++;
        j++;
    }

    return Ab;
}

Mat Mat::rowReduceFromGaussian()
{
    Mat R(*this);
    int rows = R.rows;
    int cols = R.cols;

    int i = rows - 1; // row tracker
    int j = cols - 2; // column tracker

    // iterate through every row
    while (i >= 0) {
        // find the pivot column
        int k = j - 1;
        while (k >= 0) {
            if (R(i, k) != 0) {
                j = k;
            }
            k--;
        }

        // zero out elements above pivots if pivot not 0
        if (R(i, j) != 0) {
            for (int t = i - 1; t >= 0; --t) {
                for (int s = 0; s < cols; ++s) {
                    if (s != j) {
                        R(t, s) = R(t, s) - R(i, s) * (R(t, j) / R(i, j));
                        if (R(t, s) < abs_tol && R(t, s) > -1 * abs_tol) {
                            R(t, s) = 0;
                        }
                    }
                }
                R(t, j) = 0;
            }

            // divide row by pivot
            for (int k = j + 1; k < cols; ++k) {
                R(i, k) = R(i, k) / R(i, j);
                if (R(i, k) < abs_tol && R(i, k) > -1 * abs_tol) {
                    R(i, k) = 0;
                }
            }
            R(i, j) = 1;
        }

        i--;
        j--;
    }

    return R;
}

Mat Mat::pinv()
{
    Mat I = Mat::eye(this->rows);
    Mat AI = Mat::augment(*this, I);
    Mat U = AI.gaussianEliminate();
    Mat IAInverse = U.rowReduceFromGaussian();
    Mat AInverse(this->rows, this->cols);
    for (int i = 0; i < this->rows; ++i) {
        for (int j = 0; j < this->cols; ++j) {
            AInverse(i, j) = IAInverse(i, j + this->cols);
        }
    }
    return AInverse;
}

Mat Mat::cofactor(int row, int col, int n)
{
    int i = 0, j = 0;
    Mat result(n, n);
    // Looping for each element of the matrix
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            //  Copying into temporary matrix only those element
            //  which are not in given row and column
            if (r != row && c != col) {
                result(i, j++) = (*this)(r, c);

                // Row is filled, so increase row index and
                // reset col index
                if (j == this->rows - 1) {
                    j = 0;
                    i++;
                }
            }
        }
    }
    return result;
}

float Mat::det(int n)
{
    //  Base case : if matrix contains single element
    if (n == 1) {
        return (*this)(0, 0);
    }

    float det = 1.0;
    Mat *temp = new Mat(n, n);
    *temp = *this;

    for (int i = 0; i < n; i++) {
        int pivot = i;
        for (int j = i + 1; j < n; j++) {
            if (std::abs((*temp)(j, i)) > std::abs((*temp)(pivot, i))) {
                pivot = j;
            }
        }
        if (pivot != i) {
            temp->swapRows(i, pivot);
            det *= -1;
        }
        if ((*temp)(i, i) == 0) {
            return 0;
        }
        det *= (*temp)(i, i);
        for (int j = i + 1; j < n; j++) {
            float factor = (*temp)(j, i) / (*temp)(i, i);
            for (int k = i + 1; k < n; k++) {
                (*temp)(j, k) -= factor * (*temp)(i, k);
            }
        }
    }
    delete temp;
    return det;
}

Mat Mat::adjoint()
{
    Mat adj(this->rows, this->cols);
    if (this->rows == 1) {
        adj(0, 0) = 1;
        return adj;
    }

    // temp is used to store cofactors of A(,)
    int sign = 1;
    Mat temp(this->rows, this->cols);

    for (int i = 0; i < this->rows; i++) {
        for (int j = 0; j < this->cols; j++) {
            // Get cofactor of A(i,j)
            temp = this->cofactor( i, j, this->rows);

            // sign of adj(j,i) positive if sum of row
            // and column indexes is even.
            sign = ((i + j) % 2 == 0) ? 1 : -1;

            // Interchanging rows and columns to get the
            // transpose of the cofactor matrix
            adj(j, i) = (sign) * (temp.det(this->rows - 1));
        }
    }
    return adj;
}

Mat Mat::inverse()
{
    Mat result(this->rows, this->cols);
    // Find determinant of matrix
    float det = this->det(this->rows);
    if (det == 0) {
        //std::cout << "Singular matrix, can't find its inverse";
        return result;
    }

    // Find adjoint
    Mat adj = this->adjoint();

    // Find Inverse using formula "inverse(A) = adj(A)/det(A)"
    for (int i = 0; i < this->rows; i++)
        for (int j = 0; j < this->cols; j++) {
            result(i, j) = adj(i, j) / float(det);
        }

    return result;
}

void Mat::allocate()
{
    this->ext_buff = false;
    this->length = this->rows * this->cols;
    data = new float[this->length];
    ESP_LOGD("Mat", "allocate(%i) = %p", this->length, this->data);
}

Mat Mat::expHelper(const Mat &m, int num)
{
    if (num == 0) {
        return Mat::eye(m.rows);
    } else if (num == 1) {
        return m;
    } else if (num % 2 == 0) {  // num is even
        return expHelper(m * m, num / 2);
    } else {                    // num is odd
        return m * expHelper(m * m, (num - 1) / 2);
    }
}

Mat operator+(const Mat &m1, const Mat &m2)
{
    if ((m1.rows != m2.rows) || (m1.cols != m2.cols)) {
        ESP_LOGW("Mat", "operator + Error: matrices do not have equal dimensions");
        Mat err_ret;
        return err_ret;
    }

    if (m1.sub_matrix || m2.sub_matrix) {
        Mat temp(m1.rows, m2.cols);
        dspm_add_f32(m1.data, m2.data, temp.data, m1.rows, m1.cols, m1.padding, m2.padding, temp.padding, 1, 1, 1);
        return temp;
    } else {
        Mat temp(m1);
        return (temp += m2);
    }
}

Mat operator+(const Mat &m, float C)
{
    if (m.sub_matrix) {
        Mat temp(m.rows, m.cols);
        dspm_addc_f32(m.data, temp.data, C, m.rows, m.cols, m.padding, temp.padding, 1, 1);
        return temp;
    } else {
        Mat temp(m);
        return (temp += C);
    }
}

bool operator==(const Mat &m1, const Mat &m2)
{
    if ((m1.cols != m2.cols) || (m1.rows != m2.rows)) {
        return false;
    }

    for (int row = 0; row < m1.rows; row++) {
        for (int col = 0; col < m1.cols; col++) {
            if (m1(row, col) != m2(row, col)) {
                ESP_LOGW("Mat", "operator == Error: %i %i, m1.data=%f, m2.data=%f \n", row, col, m1(row, col), m2(row, col));
                return false;
            }
        }
    }

    return true;
}

Mat operator-(const Mat &m1, const Mat &m2)
{
    if ((m1.rows != m2.rows) || (m1.cols != m2.cols)) {
        ESP_LOGW("Mat", "operator - Error: matrices do not have equal dimensions");
        Mat err_ret;
        return err_ret;
    }

    if (m1.sub_matrix || m2.sub_matrix) {
        Mat temp(m1.rows, m1.cols);
        dspm_sub_f32(m1.data, m2.data, temp.data, m1.rows, m1.cols, m1.padding, m2.padding, temp.padding, 1, 1, 1);
        return temp;
    } else {
        Mat temp(m1);
        return (temp -= m2);
    }
}

Mat operator-(const Mat &m, float C)
{
    if (m.sub_matrix) {
        Mat temp(m.rows, m.cols);
        dspm_addc_f32(m.data, temp.data, -C, m.rows, m.cols, m.padding, temp.padding, 1, 1);
        return temp;
    } else {
        Mat temp(m);
        return (temp -= C);
    }
}

Mat operator*(const Mat &m1, const Mat &m2)
{
    if (m1.cols != m2.rows) {
        ESP_LOGW("Mat", "operator * Error: matrices do not have correct dimensions");
        Mat err_ret;
        return err_ret;
    }
    Mat temp(m1.rows, m2.cols);

    if (m1.sub_matrix || m2.sub_matrix) {
        dspm_mult_ex_f32(m1.data, m2.data, temp.data, m1.rows, m1.cols, m2.cols, m1.padding, m2.padding, temp.padding);
    } else {
        dspm_mult_f32(m1.data, m2.data, temp.data, m1.rows, m1.cols, m2.cols);
    }

    return temp;

}

Mat operator*(const Mat &m, float num)
{
    if (m.sub_matrix) {
        Mat temp(m.rows, m.cols);
        dspm_mulc_f32(m.data, temp.data, num, m.rows, m.cols, m.padding, temp.padding, 1, 1);
        return temp;
    } else {
        Mat temp(m);
        return (temp *= num);
    }
}

Mat operator*(float num, const Mat &m)
{
    return (m * num);
}

Mat operator/(const Mat &m, float num)
{
    if (m.sub_matrix) {
        Mat temp(m.rows, m.cols);
        dspm_mulc_f32(m.data, temp.data, 1 / num, m.rows, m.cols, m.padding, temp.padding, 1, 1);
        return temp;
    } else {
        Mat temp(m);
        return (temp /= num);
    }
}

Mat operator/(const Mat &A, const Mat &B)
{
    if ((A.rows != B.rows) || (A.cols != B.cols)) {
        ESP_LOGW("Mat", "Operator + Error: matrices do not have equal dimensions");
        Mat err_ret;
        return err_ret;
    }

    Mat temp(A.rows, A.cols);
    for (int row = 0; row < A.rows; row++) {
        for (int col = 0; col < A.cols; col++) {
            temp(row, col) = A(row, col) / B(row, col);
        }
    }
    return temp;
}

ostream &operator<<(ostream &os, const Mat &m)
{
    for (int i = 0; i < m.rows; ++i) {
        os << m(i, 0);
        for (int j = 1; j < m.cols; ++j) {
            os << " " << m(i, j);
        }
        os << endl;
    }
    return os;
}

ostream &operator<<(ostream &os, const Mat::Rect &rect)
{
    os << "row start " << rect.y << endl;
    os << "col start " << rect.x << endl;
    os << "row count " << rect.height << endl;
    os << "col count " << rect.width << endl;

    return os;
}

istream &operator>>(istream &is, Mat &m)
{
    for (int i = 0; i < m.rows; ++i) {
        for (int j = 0; j < m.cols; ++j) {
            is >> m(i, j);
        }
    }
    return is;
}

}
