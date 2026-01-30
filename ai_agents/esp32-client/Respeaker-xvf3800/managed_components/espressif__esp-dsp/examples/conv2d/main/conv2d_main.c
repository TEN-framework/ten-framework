#include <stdio.h>
#include <malloc.h>
#include "esp_dsp.h"
#include "dsp_tests.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Start Example.");

    int max_N = 100;

    float *data1 = (float *)memalign(16, max_N * sizeof(float));
    float *data2 = (float *)memalign(16, max_N * sizeof(float));
    float *data3 = (float *)memalign(16, max_N * sizeof(float));

    image2d_t image1 = {data1, 1, 1, 8, 8, 8, 8};   // Image 8x8
    image2d_t image2 = {data2, 1, 1, 4, 4, 4, 4};   // Image 4x4
    image2d_t image3 = {data3, 1, 1, 10, 10, 0, 0}; // Image 10x10

    for (int i = 0 ; i < max_N ; i++) {
        data1[i] = 0;
        data2[i] = 0;
        data3[i] = 0;
    }

    for (int y = 0 ; y < image1.stride_y / image1.step_y ; y++) {
        for (int x = 0 ; x < image1.stride_x / image1.step_x ; x++) {
            data1[y * image1.stride_x * image1.step_y + x * image1.step_x] = 1;
        }
    }
    for (int y = 0 ; y < image2.stride_y / image2.step_y ; y++) {
        for (int x = 0 ; x < image2.stride_x / image2.step_x ; x++) {
            data2[y * image2.stride_x * image2.step_y + x * image2.step_x] = 1;
        }
    }

    dspi_conv_f32(&image1, &image2, &image3);

    ESP_LOGI(TAG, "2D Convolution reuslt.");

    for (int y = 0 ; y < image3.size_y; y++) {
        printf("[%2i .. %2i, %2i]:  ", 0,  image3.size_x, y);
        for (int x = 0 ; x < image3.size_x; x++) {
            printf("%2.0f, ", data3[y * image3.stride_x * image3.step_y + x * image3.step_x]);
        }
        printf("\n");
    }

    free(data1);
    free(data2);
    free(data3);

}
