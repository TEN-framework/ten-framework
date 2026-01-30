#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dsp_common.h"

#include "dspm_mult.h"
#include "dsps_fft2r.h"



extern void xt_iss_profile_disable();
extern void xt_iss_profile_enable();

bool dsp_is_power_of_two(int x)
{
    return (x != 0) && ((x & (x - 1)) == 0);
}

int dsp_power_of_two(int x)
{
    for (size_t i = 0; i < 16; i++) {
        x = x >> 1;
        if (0 == x) {
            return i;
        }
    }
    return 0;
}

#define test_size 128

static float data[test_size * 2];
static float check_data[test_size * 2];
static float data_test[test_size * 2];

void test_fft2r()
{

    int N = sizeof(data) / sizeof(float) / 2;
    int check_bin = 32;
    float check_ampl = 2000;
    for (int i = 0 ; i < N ; i++) {
        data[i * 2 + 0] = check_ampl * sinf(M_PI / N * check_bin * 2 * i) / (N / 2);
        data[i * 2 + 1] = 2;
    }
    for (int i = 0 ; i < N * 2 ; i++) {
        check_data[i] = data[i];
        data_test[i] = -1;
    }

    // Init FFT tables
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, 4096);
    TEST_ESP_OK(ret);
    int N_check = N;
    // table = (uint16_t *)dsps_fft2r_rev_tables_fc32[6];
    // table_size = dsps_fft2r_rev_tables_fc32_size[6];

    xt_iss_profile_enable();
    dsps_fft2r_fc32(data, N_check);
//    dsps_bit_rev_lookup_fc32_ae32(data, N_check, dsps_fft2r_rev_tables_fc32[6]);
    dsps_fft2r_fc32_aes3_(check_data, N_check, dsps_fft_w_table_fc32);
//    dsps_bit_rev_lookup_fc32_ae32(data, N_check, dsps_fft2r_rev_tables_fc32[6]);
    xt_iss_profile_disable();

    for (int i = 0 ; i < N_check ; i++) {
        if (abs(check_data[i] - data[i]) == 0) {
            printf("Data[%i] =%8.4f, %8.4f, %f \n", i, data[i], check_data[i], check_data[i] - data[i]);
        } else {
            printf("ERROR: Data[%i] =%f, %f, %f \n", i, data[i], check_data[i], check_data[i] - data[i]);
        }
    }


    printf("Test Pass!\n");
}
