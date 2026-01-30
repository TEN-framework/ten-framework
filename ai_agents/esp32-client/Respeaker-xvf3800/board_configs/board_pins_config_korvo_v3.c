/*
 * ESP32-S3-Korvo-2-V3 Pin Configuration
 *
 * 使用方法：
 * cp board_configs/board_pins_config_korvo_v3.c \
 *    /Users/qiuyanli/esp/esp-adf/components/audio_board/esp32_s3_korvo2_v3/board_pins_config.c
 */

#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>
#include "board.h"
#include "audio_error.h"
#include "audio_mem.h"
#include "soc/soc_caps.h"

static const char *TAG = "ESP32_S3_KORVO_2";

esp_err_t get_i2c_pins(i2c_port_t port, i2c_config_t *i2c_config)
{
    AUDIO_NULL_CHECK(TAG, i2c_config, return ESP_FAIL);
    if (port == I2C_NUM_0 || port == I2C_NUM_1) {
        i2c_config->sda_io_num = GPIO_NUM_17;  // Korvo-2-V3
        i2c_config->scl_io_num = GPIO_NUM_18;  // Korvo-2-V3
    } else {
        i2c_config->sda_io_num = -1;
        i2c_config->scl_io_num = -1;
        ESP_LOGE(TAG, "i2c port %d is not supported", port);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_i2s_pins(int port, board_i2s_pin_t *i2s_config)
{
    AUDIO_NULL_CHECK(TAG, i2s_config, return ESP_FAIL);
    if (port == 0) {
        i2s_config->bck_io_num = GPIO_NUM_9;    // Korvo-2-V3
        i2s_config->ws_io_num = GPIO_NUM_45;    // Korvo-2-V3
        i2s_config->data_out_num = GPIO_NUM_8;  // Korvo-2-V3
        i2s_config->data_in_num = GPIO_NUM_10;  // Korvo-2-V3
        i2s_config->mck_io_num = GPIO_NUM_16;   // Korvo-2-V3
    } else if (port == 1) {
        i2s_config->bck_io_num = -1;
        i2s_config->ws_io_num = -1;
        i2s_config->data_out_num = -1;
        i2s_config->data_in_num = -1;
        i2s_config->mck_io_num = -1;
    } else {
        memset(i2s_config, -1, sizeof(board_i2s_pin_t));
        ESP_LOGE(TAG, "i2s port %d is not supported", port);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_spi_pins(spi_bus_config_t *spi_config, spi_device_interface_config_t *spi_device_interface_config)
{
    AUDIO_NULL_CHECK(TAG, spi_config, return ESP_FAIL);
    AUDIO_NULL_CHECK(TAG, spi_device_interface_config, return ESP_FAIL);
    spi_config->mosi_io_num = -1;
    spi_config->miso_io_num = -1;
    spi_config->sclk_io_num = -1;
    spi_config->quadwp_io_num = -1;
    spi_config->quadhd_io_num = -1;
    spi_device_interface_config->spics_io_num = -1;
    ESP_LOGW(TAG, "SPI interface is not supported");
    return ESP_OK;
}

int8_t get_sdcard_intr_gpio(void) { return SDCARD_INTR_GPIO; }
int8_t get_sdcard_open_file_num_max(void) { return SDCARD_OPEN_FILE_NUM_MAX; }
int8_t get_sdcard_power_ctrl_gpio(void) { return SDCARD_PWR_CTRL; }
int8_t get_headphone_detect_gpio(void) { return HEADPHONE_DETECT; }
int8_t get_pa_enable_gpio(void) { return PA_ENABLE_GPIO; }
int8_t get_input_rec_id(void) { return BUTTON_REC_ID; }
int8_t get_input_mode_id(void) { return BUTTON_MODE_ID; }
int8_t get_input_set_id(void) { return BUTTON_SET_ID; }
int8_t get_input_play_id(void) { return BUTTON_PLAY_ID; }
int8_t get_input_volup_id(void) { return BUTTON_VOLUP_ID; }
int8_t get_input_voldown_id(void) { return BUTTON_VOLDOWN_ID; }
int8_t get_green_led_gpio(void) { return GREEN_LED_GPIO; }
int8_t get_blue_led_gpio(void) { return BLUE_LED_GPIO; }
int8_t get_es8311_mclk_src(void) { return ES8311_MCLK_SOURCE; }
