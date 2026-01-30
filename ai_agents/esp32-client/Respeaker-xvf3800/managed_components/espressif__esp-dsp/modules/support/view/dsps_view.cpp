#include "dsps_view.h"
#include <math.h>
#include "esp_log.h"
#include <limits>
#include <inttypes.h>


void dsps_view(const float *data, int32_t len, int width, int height, float min, float max, char view_char)
{
    uint8_t *view_data = new uint8_t[width * height];
    float *view_data_min = new float[width];
    float *view_data_max = new float[width];
    //

    for (int y = 0; y < height ; y++) {
        for (int x = 0 ; x < width ; x++) {
            view_data[y * width + x] = ' ';
        }
    }
    for (int i = 0 ; i < width ; i++) {
        view_data_min[i] = max;
        view_data_max[i] = min;
    }
    float x_step = (float)(width) / (float)len;
    float y_step = (float)(height - 1) / (max - min);
    float data_min = std::numeric_limits<float>::max();
    float data_max = std::numeric_limits<float>::min();
    int min_pos = 0;
    int max_pos = 0;

    for (int i = 0 ; i < len ; i++) {
        int x_pos = i * x_step;
        if (data[i] < view_data_min[x_pos]) {
            view_data_min[x_pos] = data[i];
        }
        if (data[i] > view_data_max[x_pos]) {
            view_data_max[x_pos] = data[i];
        }

        if (view_data_min[x_pos] < min) {
            view_data_min[x_pos] = min;
        }
        if (view_data_max[x_pos] > max) {
            view_data_max[x_pos] = max;
        }
        ESP_LOGD("view", "for i=%i, x_pos=%i,  max=%f, min=%f, data=%f", i, x_pos, view_data_min[x_pos], view_data_max[x_pos], data[i]);
        if (data[i] > data_max) {
            data_max = data[i];
            max_pos = i;
        }
        if (data[i] < data_min) {
            data_min = data[i];
            min_pos = i;
        }
    }
    ESP_LOGI("view", "Data min[%i] = %f, Data max[%i] = %f", min_pos, data_min, max_pos, data_max);
    ESP_LOGD("view", "y_step = %f", y_step);
    for (int x = 0 ; x < width ; x++) {
        int y_count = (view_data_max[x] - view_data_min[x]) * y_step + 1;
        ESP_LOGD("view", "For x= %i y_count=%i  ,min =%f, max=%f, ... ", x, y_count, view_data_min[x], view_data_max[x]);
        for (int y = 0 ; y < y_count ; y++) {
            int y_pos = (max - view_data_max[x]) * y_step + y;
            ESP_LOGD("view", " %i, ", y_pos);
            view_data[y_pos * width + x] = view_char;
        }
        ESP_LOGD("view", " ");
    }

    // Simple output
    // for (int i=0 ; i< len ; i++)
    // {
    //     float x_step = (float)(width-1)/(float)len;
    //     float y_step = (float)(height-1)/(max - min);
    //     int x_pos = i*x_step;
    //     int y_pos = data[i]*y_step;
    //     if (data[i] >= max) y_pos = 0;
    //     if (data[i] <= min) y_pos = height-1;
    //     view_data[y_pos*width + x_pos] = view_char;
    //     printf("For data[%i]=%f, x_pos%i, y_pos=%i\n", i, data[i], x_pos, y_pos);
    // }
    // printf("\n");
    printf(" ");
    for (int x = 0 ; x < width ; x++) {
        printf("_");
    }
    printf("\n");
    for (int y = 0; y < height ; y++) {
        printf("%i", y % 10);
        for (int x = 0 ; x < width ; x++) {
            printf("%c", view_data[y * width + x]);
        }
        printf("|\n");
    }
    printf(" ");
    for (int x = 0 ; x < width ; x++) {
        printf("%i", x % 10);
    }
    printf("\n");
    ESP_LOGI("view", "Plot: Length=%i, min=%f, max=%f", (int)len, min, max);
    delete[] view_data;
    delete[] view_data_min;
    delete[] view_data_max;
}

void dsps_view_s16(const int16_t *data, int32_t len, int width, int height, float min, float max, char view_char)
{
    float *view_data = new float[len];
    for (size_t i = 0; i < len; i++) {
//        view_data[i] = ((float)data[i])/32768.0f;
        view_data[i] = data[i];
        view_data[i] /= 32768;
    }
    dsps_view(view_data, len, width, height, min, max, view_char);
    delete[] view_data;
}

void dsps_view_spectrum(const float *data, int32_t len, float min, float max)
{
    dsps_view(data, len, 64, 10, min, max, '|');
}
