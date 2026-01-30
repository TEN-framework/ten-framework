#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dsp_common.h"

#include "dspm_mult.h"
extern void xt_iss_profile_disable();
extern void xt_iss_profile_enable();

#define M 4
#define N 8
#define K 16

const    int m = M;
const    int n = N;
const    int k = K;

float A[M][N];
float B[N][K];
float C[M][K];
float C_compare[M][K];

void test_mmult()
{

    float *A_ptr = (float *)A;
    float *B_ptr = (float *)B;
    float *C_ptr = (float *)C;
    float *Cc_ptr = (float *)C_compare;

    for (int i = 0 ; i < m * n; i++) {
        A_ptr[i] = i;
        B_ptr[i] = i;
    }
    for (int i = 0 ; i < m ; i++) {
        for (int j = 0 ; j < k ; j++) {
            C_compare[i][j] = 0;
            for (int s = 0 ; s < n ; s++) {
                C_compare[i][j] += A[i][s] * B[s][j];
            }
            C[i][j] = -1;
        }
    }
    xt_iss_profile_enable();
    dspm_mult_f32_ae32(A_ptr, B_ptr, Cc_ptr, m, n, k);
    dspm_mult_f32_aes3(A_ptr, B_ptr, C_ptr, m, n, k);
    xt_iss_profile_disable();

    for (int i = 0 ; i < m ; i++) {
        for (int j = 0 ; j < k ; j++) {
            printf("[%i][%i] calc=%f, expected =%f\n", i, j, C[i][j], C_compare[i][j]);
        }
    }
    // Compare and check results
    for (int i = 0 ; i < m * k ; i++) {
        if (Cc_ptr[i] != C_ptr[i]) {
            printf("Error - C_ptr= %f, Cc_ptr= %f \n", C_ptr[i], Cc_ptr[i]);
            return;
        }
    }

    printf("Test Pass!\n");
}
