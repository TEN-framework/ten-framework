/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <dirent.h>
#include <math.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_dsp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <malloc.h>

// Amount of audio channels
#define I2S_CHANNEL_NUM     (2)
// Microphone Sample rate
#define SAMPLE_RATE     (10000)

#define BITS_PER_CHANNEL     16
// Input buffer size
#define BUFFER_PROCESS_SIZE 512

static const char *TAG = "main";

// Buffer to process output spectrum
static float result_data[BUFFER_PROCESS_SIZE];

// Microphone read task
static void microphone_read_task(void *arg)
{
    esp_codec_dev_handle_t mic_codec_dev = NULL;
    // Init board microphone
    mic_codec_dev = bsp_audio_codec_microphone_init();
    if (mic_codec_dev == NULL) {
        ESP_LOGE(TAG, "Not possible to initialize microphone!");
        return;
    }

    // Init esp-dsp library to use fft functionality
    esp_err_t ret = dsps_fft2r_init_sc16(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to initialize FFT esp-dsp from library!");
        return;
    }

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = SAMPLE_RATE,
        .channel = I2S_CHANNEL_NUM,
        .channel_mask = 0,
        .bits_per_sample = BITS_PER_CHANNEL,
    };

    int result = esp_codec_dev_open(mic_codec_dev, &fs);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to open microphone!");
        return;
    }
    // Set input microphone gain (from 1 to 100)
    ESP_LOGI(TAG, "Adjust microphone input volume in the code here...");
    result |= esp_codec_dev_set_in_gain(mic_codec_dev, 20.0);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to set up microphone gain!");
        return;
    }

    int audio_chunksize = BUFFER_PROCESS_SIZE;

    // Allocate audio buffer and check for result
    int16_t *audio_buffer = (int16_t *)memalign(16, (audio_chunksize + 16) * sizeof(int16_t) * I2S_CHANNEL_NUM);
    // Allocate buffer for window
    int16_t *wind_buffer = (int16_t *)memalign(16, (audio_chunksize + 16) * sizeof(int16_t) * I2S_CHANNEL_NUM);
    // Generate window and convert it to int16_t
    dsps_wind_blackman_harris_f32(result_data, audio_chunksize);
    for (int i = 0 ; i < audio_chunksize; i++) {
        wind_buffer[i * 2 + 0] = (int16_t)(result_data[i] * 32767);
        wind_buffer[i * 2 + 1] = wind_buffer[i * 2 + 0];
    }

    while (true) {

        // Read audio data from I2S bus
        result = esp_codec_dev_read(mic_codec_dev, audio_buffer, audio_chunksize * sizeof(int16_t) * I2S_CHANNEL_NUM);
        // Multiply input stream with window coefficients
        dsps_mul_s16_ansi(audio_buffer, wind_buffer, audio_buffer, audio_chunksize * 2, 1, 1, 1, 15);

        // Call FFT bit reverse
        dsps_fft2r_sc16_ae32(audio_buffer, audio_chunksize);
        dsps_bit_rev_sc16_ansi(audio_buffer, audio_chunksize);
        // Convert spectrum from two input channels to two
        // spectrums for two channels.
        dsps_cplx2reC_sc16(audio_buffer, audio_chunksize);

        // The output data array presented as moving average for input in dB
        for (int i = 0 ; i < audio_chunksize ; i++) {
            float spectrum_sqr = audio_buffer[i * 2 + 0] * audio_buffer[i * 2 + 0] + audio_buffer[i * 2 + 1] * audio_buffer[i * 2 + 1];
            float spectrum_dB = 10 * log10f(0.1 + spectrum_sqr);
            // Multiply with sime coefficient for better view data on screen
            spectrum_dB = 4 * spectrum_dB;
            // Apply moving average of spectrum
            result_data[i] = 0.8 * result_data[i] + 0.2 * spectrum_dB;
        }
        vTaskDelay(10);
    }
}

// Screen image width
#define X_AXIS_SIZE (320)
// Screen image height
#define Y_AXIS_SIZE (240)

static uint8_t screen_rgb_data[X_AXIS_SIZE * Y_AXIS_SIZE * LV_IMG_PX_SIZE_ALPHA_BYTE];

static const lv_img_dsc_t img_screen_rgb = {
    .header.always_zero = 0,
    .header.w = X_AXIS_SIZE,
    .header.h = Y_AXIS_SIZE,
    .data_size = X_AXIS_SIZE * Y_AXIS_SIZE * LV_IMG_PX_SIZE_ALPHA_BYTE,
    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .data = screen_rgb_data,
};

// The function convert value to RGB565 color value
static int8_t colors[3][3] = { {0, 0, 31}, {0, 63, 0}, {31, 0, 0} };
static uint16_t convert_to_rgb(uint8_t minval, uint8_t maxval, int8_t val)
{
    uint16_t result;

    float i_f = (float)(val - minval) / (float)(maxval - minval) * 2;

    int Ii = i_f;
    float If = i_f - Ii;

    int8_t *c1 = colors[Ii];
    int8_t *c2 = colors[Ii + 1];
    uint16_t res_colors[3];

    res_colors[0] = c1[0] + If * (c2[0] - c1[0]);
    res_colors[1] = c1[1] + If * (c2[1] - c1[1]);
    res_colors[2] = c1[2] + If * (c2[2] - c1[2]);
    result = res_colors[2] | (res_colors[1] << 5) | (res_colors[0] << 11);
    return result;
}

// Init screen with blue values
static void spectrum2d_picture_init()
{
    for (int y = 0 ; y < img_screen_rgb.header.h ; y++) {
        for (int x = 0 ; x < img_screen_rgb.header.w ; x++) {
            screen_rgb_data[(y * img_screen_rgb.header.w + x)*LV_IMG_PX_SIZE_ALPHA_BYTE + 0] = 0x0;
            screen_rgb_data[(y * img_screen_rgb.header.w + x)*LV_IMG_PX_SIZE_ALPHA_BYTE + 1] = 0x1f;
            screen_rgb_data[(y * img_screen_rgb.header.w + x)*LV_IMG_PX_SIZE_ALPHA_BYTE + 2] = 0xff;
        }
    }
}

// Add spectrum data to the screen
static void spectrum2d_picture()
{
    for (int y = 0 ; y < (img_screen_rgb.header.h - 1) ; y++) {
        for (int x = 0 ; x < img_screen_rgb.header.w ; x++) {
            for (int i = 0 ; i < LV_IMG_PX_SIZE_ALPHA_BYTE ; i++) {
                screen_rgb_data[(y * img_screen_rgb.header.w + x)*LV_IMG_PX_SIZE_ALPHA_BYTE + i] = screen_rgb_data[((y + 1) * img_screen_rgb.header.w + x) * LV_IMG_PX_SIZE_ALPHA_BYTE + i];
            }
        }
    }

    // Add left channel to the screen
    // The order of the values inverted
    for (int x = 0 ; x < img_screen_rgb.header.w / 2 ; x++) {
        // Get inverted index value
        int in_index = img_screen_rgb.header.w / 2 - x - 1;
        float data = result_data[in_index];

        // Limit input data
        if (data > 127) {
            data = 127;
        }
        if (data < 0) {
            data = 0;
        }

        // Convert input value in dB to the color
        uint16_t color_val = convert_to_rgb(0, 128, data);
        // Split 16 bit value to two bytes, to change the bytes order
        uint8_t *ref_val = (uint8_t *)&color_val;
        int out_index = x;
        screen_rgb_data[((img_screen_rgb.header.h - 1)*img_screen_rgb.header.w + out_index)*LV_IMG_PX_SIZE_ALPHA_BYTE + 0] = ref_val[1];
        screen_rgb_data[((img_screen_rgb.header.h - 1)*img_screen_rgb.header.w + out_index)*LV_IMG_PX_SIZE_ALPHA_BYTE + 1] = ref_val[0];
        // Set alpha value
        screen_rgb_data[((img_screen_rgb.header.h - 1)*img_screen_rgb.header.w + out_index)*LV_IMG_PX_SIZE_ALPHA_BYTE + 2] = 0xff;
    }

    // Add right channel to the screen
    for (int x = 0 ; x < img_screen_rgb.header.w / 2 ; x++) {
        // Get index of right channel
        int in_index = BUFFER_PROCESS_SIZE / 2 + x;
        float data = result_data[in_index];

        // Limit input data
        if (data > 127) {
            data = 127;
        }
        if (data < 0) {
            data = 0;
        }

        // Convert input value in dB to the color
        uint16_t color_val = convert_to_rgb(0, 128, data);
        // Split 16 bit value to two bytes, to change the bytes order
        uint8_t *ref_val = (uint8_t *)&color_val;
        int out_index = img_screen_rgb.header.w / 2 + x;
        screen_rgb_data[((img_screen_rgb.header.h - 1)*img_screen_rgb.header.w + out_index)*LV_IMG_PX_SIZE_ALPHA_BYTE + 0] = ref_val[1];
        screen_rgb_data[((img_screen_rgb.header.h - 1)*img_screen_rgb.header.w + out_index)*LV_IMG_PX_SIZE_ALPHA_BYTE + 1] = ref_val[0];
        // Set alpha value
        screen_rgb_data[((img_screen_rgb.header.h - 1)*img_screen_rgb.header.w + out_index)*LV_IMG_PX_SIZE_ALPHA_BYTE + 2] = 0xff;
    }
}

static void image_display_task(void *arg)
{
    // LV_IMG_DECLARE(img_screen_rgb);
    lv_obj_t *img1 = lv_img_create(lv_scr_act());
    lv_img_set_src(img1, &img_screen_rgb);
    spectrum2d_picture_init();
    lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);

    for (;;) {
        // Update image with new spectrum values
        spectrum2d_picture();
        // Update screen with new image
        lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);
        // Free CPU for a while
        vTaskDelay(1);
    }
}

void app_main(void)
{
    /* Initialize I2C (for touch and audio) */
    bsp_i2c_init();

    /* Initialize display and LVGL */
    bsp_display_start();

    /* Set display brightness to 100% */
    bsp_display_backlight_on();

    int ret_val = xTaskCreatePinnedToCore(&microphone_read_task, "Microphone read Task", 8 * 1024, NULL, 3, NULL, 0);
    if (ret_val != pdPASS) {
        ESP_LOGE(TAG, "Not possible to allocate microphone task, ret_val = %i", ret_val);
        return;
    }

    ret_val = xTaskCreatePinnedToCore(&image_display_task, "Draw task", 10 * 1024, NULL, 5, NULL, 1);
    if (ret_val != pdPASS) {
        ESP_LOGE(TAG, "Not possible to allocate microphone task, ret_val= %i", ret_val);
        return;
    }
}
