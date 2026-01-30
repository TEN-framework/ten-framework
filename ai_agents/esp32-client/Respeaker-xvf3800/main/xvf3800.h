#ifndef XVF3800_H
#define XVF3800_H

#include "esp_err.h"
#include "driver/i2c.h"
#include <stdint.h>
#include <stdbool.h>

// XVF3800 I2C address (confirmed from XMOS documentation)
#define XVF3800_I2C_ADDR 0x2C

// XVF3800 Resource IDs (these may need adjustment based on firmware)
// Note: Actual values need to be confirmed from device_control_shared.h
#define XVF3800_RESOURCE_ID_GPIO    0x08  // Estimated - GPIO/IO config resource
#define XVF3800_RESOURCE_ID_DFU     0xF0  // Confirmed from documentation

// XVF3800 Command IDs for GPIO (estimated values, may need adjustment)
// Based on io_config_cmds.yaml structure mentioned in programming guide
#define XVF3800_CMD_GPI_INDEX       0x01  // Set/get pin index for GPI read
#define XVF3800_CMD_GPI_VALUE       0x02  // Get current logic level of selected GPI
#define XVF3800_CMD_GPI_VALUE_ALL   0x03  // Get all GPI values as bitmap

// GPIO Pin definitions (based on XVF3800 Datasheet Table 5.8)
// P_BUTTON_0 (Pin 11) = Mute button
// P_BUTTON_1 (Pin 32) = Action button (SET key on ReSpeaker)
#define XVF3800_GPI_MUTE_BUTTON     0     // P_BUTTON_0 - Mute button (GPI index 0)
#define XVF3800_GPI_ACTION_BUTTON   1     // P_BUTTON_1 - Action/SET button (GPI index 1)

// Control command return status (from control_ret_t enum)
#define XVF3800_STATUS_SUCCESS      0x00
#define XVF3800_STATUS_ERROR        0x01

// Button state
typedef enum {
    XVF3800_BUTTON_RELEASED = 1,  // High when released (from Seeed wiki)
    XVF3800_BUTTON_PRESSED = 0    // Low when pressed
} xvf3800_button_state_t;

typedef struct {
    i2c_port_t i2c_port;
    uint8_t i2c_addr;
    uint8_t resource_id_gpio;  // Dynamically discovered GPIO resource ID
} xvf3800_handle_t;

/**
 * @brief Initialize XVF3800 device
 *
 * @param handle Handle to store device context
 * @param i2c_port I2C port number
 * @return ESP_OK on success
 */
esp_err_t xvf3800_init(xvf3800_handle_t *handle, i2c_port_t i2c_port);

/**
 * @brief Read all GPI states as a bitmap (更简单的方法)
 *
 * @param handle Device handle
 * @param bitmap Output: 32-bit bitmap where bit n = GPI[n] state
 * @return ESP_OK on success
 */
esp_err_t xvf3800_read_gpi_all(xvf3800_handle_t *handle, uint32_t *bitmap);

/**
 * @brief Read GPI (button) state using XVF3800 control protocol
 *
 * @param handle Device handle
 * @param pin_index GPIO pin index (e.g., 0 for mute, 1 for action)
 * @param state Output: button state (1=released, 0=pressed)
 * @return ESP_OK on success
 */
esp_err_t xvf3800_read_gpi(xvf3800_handle_t *handle, uint8_t pin_index, uint8_t *state);

/**
 * @brief Read mute button state
 *
 * @param handle Device handle
 * @param is_pressed Output: true if button is pressed
 * @return ESP_OK on success
 */
esp_err_t xvf3800_read_mute_button(xvf3800_handle_t *handle, bool *is_pressed);

/**
 * @brief Start button monitoring task
 *
 * This creates a FreeRTOS task that polls the mute button and triggers
 * callbacks when button state changes.
 *
 * @param handle Device handle
 * @return ESP_OK on success
 */
esp_err_t xvf3800_start_button_monitor(xvf3800_handle_t *handle);

/**
 * @brief Stop button monitoring task
 */
void xvf3800_stop_button_monitor(void);

#endif // XVF3800_H
