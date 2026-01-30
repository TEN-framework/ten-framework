/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <sdkconfig.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include "bsp/esp-bsp.h"
#include <stdint.h>
#include <math.h>

// Buffer for reading/writing to I2S driver. Same length as SPIFFS buffer and I2S buffer, for optimal read/write performance.
// Recording audio data path:
// I2S peripheral -> I2S buffer (DMA) -> App buffer (RAM) -> SPIFFS buffer -> External SPI Flash.
// Vice versa for playback.
#define BUFFER_SIZE     (64)
#define SAMPLE_RATE     (16000) // For recording
#define DEFAULT_VOLUME  (100)

// Globals
static const char *TAG = "example";
static QueueHandle_t audio_button_q = NULL;

static void btn_handler(void *button_handle, void *usr_data)
{
    int button_pressed = (int)usr_data;
    xQueueSend(audio_button_q, &button_pressed, 0);
}

// Very simple WAV header, ignores most fields
typedef struct __attribute__((packed))
{
    uint8_t ignore_0[22];
    uint16_t num_channels;
    uint32_t sample_rate;
    uint8_t ignore_1[6];
    uint16_t bits_per_sample;
    uint8_t ignore_2[4];
    uint32_t data_size;
    uint8_t data[];
} dumb_wav_header_t;

typedef enum audio_set {
    AUDIO_VOLUME,
    AUDIO_BASS,
    AUDIO_TREBLE,
} audio_set_t;

static esp_codec_dev_handle_t spk_codec_dev = NULL;
static FILE *play_file = NULL;
// Pointer to a file that is going to be played
static const char play_filename[] = BSP_SPIFFS_MOUNT_POINT"/16bit_mono_44_1_khz.wav";

// Definition for all tasks
static void buttons_process_task(void *arg);
static void audio_read_task(void *arg);
static void audio_process_task(void *arg);


// Wave file header
static dumb_wav_header_t wav_header;
static audio_set_t current_set = AUDIO_VOLUME;

// Data for IIR filters
float iir_coeffs_lpf[5];
float iir_w_lpf[5] = {0, 0};

float iir_coeffs_hpf[5];
float iir_w_hpf[5] = {0, 0};

// IIR filters parameters
// Parameters for low-pass filter (LPF)
float lpf_gain = 0;
float lpf_qFactor = 0.5;
float lpf_freq = 0.01;

// Parameters for high-pass filter (HPF)
float hpf_gain = 0;
float hpf_qFactor = 1;
float hpf_freq = 0.15;

// Volume control definitions
float full_volume = 1;
// Volume in dB
int   full_volume_db = -12;
// Digital limiter envelope value
float full_envelope = 0;

// processing audio buffer
float processing_audio_buffer[BUFFER_SIZE] = {0};

// The triple_audio_buffer contains tree data arrays sizeof BUFFER_SIZE, for writing audio data to the codec
int16_t triple_audio_buffer[3 * BUFFER_SIZE] = {0};
// The write index shows the audio buffer that will be used to write data to the codec
int   audio_buffer_write_index = 0;
// The read index shows the audio buffer that will be used to fill data from the data source to the processing buffer
int   audio_buffer_read_index = 0;

// Semaphore to synchronize the read and write
static SemaphoreHandle_t sync_read_task;

// Convert input int16_t Q15 values array to the float values array
static void convert_short2float(int16_t *int16_data, float *float_data, int len)
{
    float multiplier = 1.0 / (float)(INT16_MAX + 1);
    for (int i = 0 ; i < len ; i++) {
        float_data[i] = (float)int16_data[i] * multiplier;
    }
}

// Convert input float values to the int16_t Q15 values array
static void convert_float2short(float *float_data, int16_t *int16_data, int len)
{
    float multiplier = (float)(INT16_MAX + 1);
    for (int i = 0 ; i < len ; i++) {
        int16_data[i] = (int16_t)((float)multiplier * (float)float_data[i]);
    }
}

// In the audio processing the valid output audio values for the floating point format should be in range +/- 1.0,
// because these values will be converted to the 0x8000 and 0x7fff int16_t values, and will be accepted by the codec.
// With additional amplification, for example  bass, treble, and for other processing, it is possible that the audio
// signal will reach the maximum values and will make an overflow at the DAC.
// To avoid this situation the digital limiter analyze the output values and control the full amplification gain.
//
void digitalLimiter(float *input_signal, float *output_signal, int signal_length, float threshold, float attack_value, float release_value, float *in_envelope)
{
    float envelope = *in_envelope;
    for (int i = 0; i < signal_length; i++) {
        // Calculate envelope
        float abs_input = fabsf(input_signal[i]);
        if (abs_input > envelope) {
            envelope = envelope * (1 - attack_value) + attack_value * abs_input;
        } else {
            envelope = envelope * (1 - release_value) + release_value * abs_input;
        }

        // Apply compression
        if (envelope > threshold) {
            output_signal[i] = input_signal[i] * (threshold / envelope);
        } else {
            output_signal[i] = input_signal[i];
        }
    }
    *in_envelope = envelope;
}

static void audio_process_task(void *arg)
{
    // Init codeac and apply the initial volume to maximum
    spk_codec_dev = bsp_audio_codec_speaker_init();
    assert(spk_codec_dev);
    esp_codec_dev_set_out_vol(spk_codec_dev, DEFAULT_VOLUME);

    // Open file and het the WAV header to set up the sample frequency, amount of channels and resolution
    play_file = fopen(play_filename, "rb");
    if (play_file == NULL) {
        ESP_LOGW(TAG, "%s file does not exist!", play_filename);
    }
    // Read WAV header
    if (fread((void *)&wav_header, 1, sizeof(wav_header), play_file) != sizeof(wav_header)) {
        ESP_LOGW(TAG, "Error in reading file");
        return;
    }

    ESP_LOGI(TAG, "Number of channels: %" PRIu16 "", wav_header.num_channels);
    ESP_LOGI(TAG, "Bits per sample: %" PRIu16 "", wav_header.bits_per_sample);
    ESP_LOGI(TAG, "Sample rate: %" PRIu32 "", wav_header.sample_rate);
    ESP_LOGI(TAG, "Data size: %" PRIu32 "", wav_header.data_size);
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = wav_header.sample_rate,
        .channel = wav_header.num_channels,
        .bits_per_sample = wav_header.bits_per_sample,
    };
    if (spk_codec_dev != NULL) {
        int result = esp_codec_dev_open(spk_codec_dev, &fs);
    }
    // Calculate initial volume value
    full_volume = exp10f((float)full_volume_db / 20);
    // Calculate initial state for LPF
    dsps_biquad_gen_lowShelf_f32(iir_coeffs_lpf, lpf_freq, lpf_gain, lpf_qFactor);
    // Calculate initial state for HPF
    dsps_biquad_gen_highShelf_f32(iir_coeffs_hpf, hpf_freq, hpf_gain, hpf_qFactor);

    BaseType_t ret = xTaskCreate(buttons_process_task, "buttons_process_task", 4096, NULL, 4, NULL);
    assert(ret == pdPASS);

    sync_read_task = xSemaphoreCreateCounting(1, 0);

    ret = xTaskCreate(audio_read_task, "audio_read_task", 4096, NULL, 7, NULL);
    assert(ret == pdPASS);
    vTaskDelay(1);

    ESP_LOGW(TAG, "To select volume/bass/treble please use the 'Set' button. And adjust the value with +/- buttons.");

    for (;;) {
        /* Get data from SPIFFS and send it to codec */
        int16_t *wav_bytes = &triple_audio_buffer[audio_buffer_write_index * BUFFER_SIZE];
        // Write samples to audio codec
        esp_codec_dev_write(spk_codec_dev, wav_bytes, BUFFER_SIZE * sizeof(int16_t));
        audio_buffer_write_index++;
        if (audio_buffer_write_index >= 3) {
            audio_buffer_write_index = 0;
        }
        // Check the triple buffer overflow
        if (audio_buffer_write_index == audio_buffer_read_index) {
            // Call delay to switch the task to fill the buffer
            vTaskDelay(1);
            // Check and indicate overflow status
            if (audio_buffer_write_index == audio_buffer_read_index) {
                ESP_LOGW(TAG, "Audio buffer overflow!");
            }
        }
        // Generate synt event to read task:
        xSemaphoreGive(sync_read_task);
    }
}

// The audio_read_task is responsible to read data from the file and fill it to the audio buffer.
// The audio buffer is places inside of triple buffer, to avoid overflow situation in case if
// the processing task busy for a while.
static void audio_read_task(void *arg)
{
    while (1) {
        // Wait the sync semaphore
        if (xSemaphoreTake(sync_read_task, 100)) {
            // Get the pointer to the current audio buffer
            int16_t *wav_buffer = &triple_audio_buffer[audio_buffer_read_index * BUFFER_SIZE];
            // Read the data from the file
            uint32_t bytes_read_from_spiffs = fread(wav_buffer, sizeof(int16_t), BUFFER_SIZE, play_file);
            // Convert input samples from int16 to float for processing
            convert_short2float((int16_t *)wav_buffer, processing_audio_buffer, BUFFER_SIZE);
            // Apply bass
            dsps_biquad_f32(processing_audio_buffer, processing_audio_buffer, BUFFER_SIZE, iir_coeffs_lpf, iir_w_lpf);
            // Apply treble
            dsps_biquad_f32(processing_audio_buffer, processing_audio_buffer, BUFFER_SIZE, iir_coeffs_hpf, iir_w_hpf);
            // Apply voluve
            dsps_mulc_f32_ansi(processing_audio_buffer, processing_audio_buffer, BUFFER_SIZE, full_volume, 1, 1);
            // Apply limiter
            digitalLimiter(processing_audio_buffer, processing_audio_buffer, BUFFER_SIZE, 0.5, 0.5, 0.0001, &full_envelope);
            // Convert from float to int16 for audio codec
            convert_float2short(processing_audio_buffer, (int16_t *)wav_buffer, BUFFER_SIZE);

            if (bytes_read_from_spiffs != BUFFER_SIZE) {
                // Rewind the file and read the WAV header
                rewind(play_file);
                fread((void *)&wav_header, 1, sizeof(wav_header), play_file);
                // Read data to the audio buffer
                bytes_read_from_spiffs = fread(wav_buffer, sizeof(int16_t), BUFFER_SIZE, play_file);
            }
            audio_buffer_read_index++;
            if (audio_buffer_read_index >= 3) {
                audio_buffer_read_index = 0;
            }
        } else {
            // Error in case of timeout
            ESP_LOGE(TAG, "Audio timeout!");
        }
    }
}

static void buttons_process_task(void *arg)
{
    while (1) {
        int btn_index = 0;
        if (xQueueReceive(audio_button_q, &btn_index, portMAX_DELAY) == pdTRUE) {
            switch (btn_index) {
            case BSP_BUTTON_SET: {
                current_set += 1;
                if (current_set > 2) {
                    current_set = AUDIO_VOLUME;
                }
                switch (current_set) {
                case AUDIO_VOLUME:
                    ESP_LOGW(TAG, "Select volume");
                    break;
                case AUDIO_BASS:
                    ESP_LOGW(TAG, "Select bass");
                    break;
                case AUDIO_TREBLE:
                    ESP_LOGW(TAG, "Select treble");
                    break;
                default:
                    break;
                }
                break;
            }
            case BSP_BUTTON_VOLDOWN: {
                switch (current_set) {
                case AUDIO_VOLUME:
                    full_volume_db -= 3;
                    if (full_volume_db < -36) {
                        full_volume_db = -36;
                    }
                    full_volume = exp10f((float)full_volume_db / 20);
                    ESP_LOGI(TAG, "Volume Down: %i dB", full_volume_db);
                    break;
                case AUDIO_BASS:
                    lpf_gain -= 1;
                    if (lpf_gain < -12) {
                        lpf_gain = -12;
                    }
                    ESP_LOGI(TAG, "Bass Down: %i", (int)lpf_gain);
                    dsps_biquad_gen_lowShelf_f32(iir_coeffs_lpf, lpf_freq, lpf_gain, lpf_qFactor);
                    break;
                case AUDIO_TREBLE:
                    hpf_gain -= 1;
                    if (hpf_gain < -12) {
                        hpf_gain = -12;
                    }
                    ESP_LOGI(TAG, "Treble Down: %i", (int)hpf_gain);
                    dsps_biquad_gen_highShelf_f32(iir_coeffs_hpf, hpf_freq, hpf_gain, hpf_qFactor);
                    break;
                default:
                    break;
                }
                break;
            }
            case BSP_BUTTON_VOLUP: {
                switch (current_set) {
                case AUDIO_VOLUME:
                    full_volume_db += 3;
                    if (full_volume_db > 0) {
                        full_volume_db = 0;
                    }
                    full_volume = exp10f((float)full_volume_db / 20);
                    ESP_LOGI(TAG, "Volume Up: %i dB", full_volume_db);
                    break;
                case AUDIO_BASS:
                    lpf_gain += 1;
                    if (lpf_gain > 12) {
                        lpf_gain = 12;
                    }
                    ESP_LOGI(TAG, "Bass Up: %i", (int)lpf_gain);
                    dsps_biquad_gen_lowShelf_f32(iir_coeffs_lpf, lpf_freq, lpf_gain, lpf_qFactor);
                    break;
                case AUDIO_TREBLE:
                    hpf_gain += 1;
                    if (hpf_gain > 12) {
                        hpf_gain = 12;
                    }
                    ESP_LOGI(TAG, "Treble Up: %i", (int)hpf_gain);
                    dsps_biquad_gen_highShelf_f32(iir_coeffs_hpf, hpf_freq, hpf_gain, hpf_qFactor);
                    break;
                default:
                    break;
                }
                break;
            }
            default:
                ESP_LOGI(TAG, "No function for this button");
                break;
            }
        }
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_spiffs_mount());

    // Create FreeRTOS tasks and queues
    audio_button_q = xQueueCreate(10, sizeof(int));
    assert (audio_button_q != NULL);

    BaseType_t ret = xTaskCreate(audio_process_task, "audio_process_task", 4096, NULL, 6, NULL);
    assert(ret == pdPASS);

    // Init audio buttons
    button_handle_t btns[BSP_BUTTON_NUM];
    ESP_ERROR_CHECK(bsp_iot_button_create(btns, NULL, BSP_BUTTON_NUM));
    for (int i = 0; i < BSP_BUTTON_NUM; i++) {
        ESP_ERROR_CHECK(iot_button_register_cb(btns[i], BUTTON_PRESS_DOWN, btn_handler, (void *) i));
    }
}
