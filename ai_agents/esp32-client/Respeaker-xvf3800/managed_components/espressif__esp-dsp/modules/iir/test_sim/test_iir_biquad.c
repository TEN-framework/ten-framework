#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dsp_common.h"

#include "dsps_biquad.h"

#define N_SAMPLES 1024
int N = N_SAMPLES;
// Input test array
float d[N_SAMPLES];
// output array
float y[N_SAMPLES];
float y_ref[N_SAMPLES];

extern void xt_iss_profile_disable();
extern void xt_iss_profile_enable();
esp_err_t dsps_biquad_f32_aes3(const float *input, float *output, int len, float *coef, float *w);

void test_iir_biquad()
{
    float coeffs_lpf[5] = {0.073802,  0.147603,  0.073802,  -1.250516,  0.545723};
    float w_lpf[5] = {0, 0};
    float w_lpf_ref[5] = {0, 0};
    esp_err_t ret = ESP_OK;

    for (size_t i = 0; i < N; i++) {
        d[i] = 0;
    }
    d[0] = 1;
    xt_iss_profile_enable();
    ret = dsps_biquad_f32_ansi(d, y_ref, N, coeffs_lpf, w_lpf_ref);
    xt_iss_profile_disable();
    if (ret  != ESP_OK) {
        printf("dsps_biquad_f32 error = %i\n", ret);
        return;
    }
    xt_iss_profile_enable();
    ret = dsps_biquad_f32_aes3(d, y, N, coeffs_lpf, w_lpf);
    xt_iss_profile_disable();
    if (ret  != ESP_OK) {
        printf("dsps_biquad_f32 error = %i\n", ret);
        return;
    }
    for (size_t i = 0; i < N; i++) {
        if (((y[i] - y_ref[i]) > 0.0000001) || (y[i] - y_ref[i]) < -0.0000001) {
            printf("ERROR result[%i]: %f, expect = %f, diff=%f\n", i, y[i], y_ref[i], y[i] - y_ref[i]);
            return;

        }
    }
    printf("Test Correct!\n");

}
