#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dsp_common.h"

#include "dsps_fir.h"

float x[1024];
float y[1024];
float y_compare[1024];

float coeffs[256];
float delay[256];
float delay_compare[256];


void test_fir()
{
    int len = sizeof(x) / sizeof(float);
    int fir_len = sizeof(coeffs) / sizeof(float);

    fir_f32_t fir1;
    fir_f32_t fir2;
    for (int i = 0 ; i < fir_len ; i++) {
        coeffs[i] = i;
    }

    for (int i = 0 ; i < len ; i++) {
        x[i] = 0;
    }
    x[0] = 1;

    for (int i = 0 ; i < fir_len ; i++) {
        coeffs[i] = i;
    }

    for (int i = 0 ; i < len ; i++) {
        x[i] = i;
    }
    x[0] = 1;
    dsps_fir_init_f32(&fir1, coeffs, delay, fir_len / 4);
    dsps_fir_init_f32(&fir2, coeffs, delay_compare, fir_len);

    xt_iss_profile_enable();
    dsps_fir_f32_aes3(&fir1, x, y, len);
    dsps_fir_f32_ansi(&fir2, x, y_compare, len);
    xt_iss_profile_disable();

    printf("Test Pass!\n");
}
