#include "xvf3800.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ai_agent.h"
#include "common.h"
#include "string.h"

static const char *TAG = "XVF3800";
static xvf3800_handle_t *g_handle = NULL;
static TaskHandle_t g_monitor_task = NULL;

esp_err_t xvf3800_init(xvf3800_handle_t *handle, i2c_port_t i2c_port)
{
    if (!handle) return ESP_ERR_INVALID_ARG;

    handle->i2c_port = i2c_port;
    handle->i2c_addr = XVF3800_I2C_ADDR;
    handle->resource_id_gpio = XVF3800_RESOURCE_ID_GPIO;  // Default value

    ESP_LOGI(TAG, "Initializing XVF3800 at I2C address 0x%02X", handle->i2c_addr);
    ESP_LOGI(TAG, "Using XMOS control protocol (resource-based commands)");

    // Try to detect XVF3800 - simple probe
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ XVF3800 detected at address 0x%02X", handle->i2c_addr);

        // Scan for valid Resource IDs
        ESP_LOGI(TAG, "Scanning for valid GPIO Resource ID...");
        bool found_valid_resource = false;

        for (uint8_t test_resid = 0x00; test_resid <= 0x20; test_resid++) {
            // Try to read GPI_VALUE_ALL with this resource ID
            uint8_t write_buf[3];
            write_buf[0] = test_resid;
            write_buf[1] = XVF3800_CMD_GPI_VALUE_ALL | 0x80;  // Read command
            write_buf[2] = 5;  // Expected reply: 1 status + 4 data bytes

            uint8_t read_buf[5] = {0};

            esp_err_t scan_ret = i2c_master_write_read_device(
                handle->i2c_port,
                handle->i2c_addr,
                write_buf,
                3,
                read_buf,
                5,
                pdMS_TO_TICKS(50)
            );

            if (scan_ret == ESP_OK && read_buf[0] == XVF3800_STATUS_SUCCESS) {
                ESP_LOGW(TAG, "✓✓✓ FOUND valid Resource ID: 0x%02X (status=0x%02X)",
                         test_resid, read_buf[0]);
                ESP_LOGW(TAG, "    GPIO bitmap: 0x%02X%02X%02X%02X",
                         read_buf[4], read_buf[3], read_buf[2], read_buf[1]);
                found_valid_resource = true;

                // Save the correct resource ID to handle
                handle->resource_id_gpio = test_resid;

                if (test_resid != XVF3800_RESOURCE_ID_GPIO) {
                    ESP_LOGW(TAG, "    ⚠️  Auto-updated Resource ID from 0x%02X to 0x%02X",
                             XVF3800_RESOURCE_ID_GPIO, test_resid);
                    ESP_LOGW(TAG, "    For permanent fix, update xvf3800.h:");
                    ESP_LOGW(TAG, "    #define XVF3800_RESOURCE_ID_GPIO 0x%02X", test_resid);
                } else {
                    ESP_LOGI(TAG, "    ✓ Resource ID 0x%02X matches header definition", test_resid);
                }

                // Found it, can break early
                break;
            }
        }

        if (!found_valid_resource) {
            ESP_LOGE(TAG, "✗ No valid GPIO Resource ID found in range 0x00-0x20");
            ESP_LOGE(TAG, "  XVF3800 may need firmware initialization first");
        }

    } else {
        ESP_LOGW(TAG, "✗ XVF3800 probe failed: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "This may be normal - device may need specific initialization");
    }

    return ESP_OK;  // Return OK even if probe fails
}

/**
 * Write a control command to XVF3800
 * Format: Resource_ID + Command_ID + Length + Payload (optional)
 * Then read status byte
 */
static esp_err_t xvf3800_write_cmd(xvf3800_handle_t *handle, uint8_t resource_id,
                                    uint8_t cmd_id, uint8_t *payload, uint8_t payload_len)
{
    if (!handle) return ESP_ERR_INVALID_ARG;

    // Total transfer size = Resource ID (1) + Command ID (1) + Length (1) + Payload
    uint8_t total_size = 3 + payload_len;
    uint8_t write_buf[64];  // Max command size from docs

    write_buf[0] = resource_id;
    write_buf[1] = cmd_id;
    write_buf[2] = total_size;

    if (payload && payload_len > 0) {
        memcpy(&write_buf[3], payload, payload_len);
    }

    // Write command
    esp_err_t ret = i2c_master_write_to_device(
        handle->i2c_port,
        handle->i2c_addr,
        write_buf,
        3 + payload_len,
        pdMS_TO_TICKS(100)
    );

    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "Write cmd failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Read status byte
    uint8_t status = 0xFF;
    ret = i2c_master_read_from_device(
        handle->i2c_port,
        handle->i2c_addr,
        &status,
        1,
        pdMS_TO_TICKS(100)
    );

    if (ret == ESP_OK) {
        if (status != XVF3800_STATUS_SUCCESS) {
            ESP_LOGD(TAG, "Command returned status: 0x%02X", status);
            return ESP_FAIL;
        }
    }

    return ret;
}

/**
 * Read a value from XVF3800 using control protocol
 * Format: Resource_ID + (Command_ID | 0x80) + Expected_Reply_Length
 * Then repeated START and read Status + Payload
 */
static esp_err_t xvf3800_read_cmd(xvf3800_handle_t *handle, uint8_t resource_id,
                                   uint8_t cmd_id, uint8_t *response, uint8_t response_len)
{
    if (!handle || !response) return ESP_ERR_INVALID_ARG;

    // Prepare read command: Resource ID + (CMD | 0x80) + Expected length
    uint8_t write_buf[3];
    write_buf[0] = resource_id;
    write_buf[1] = cmd_id | 0x80;  // OR with 0x80 to specify READ
    write_buf[2] = response_len + 1;  // +1 for status byte

    // Use write-read combined transaction
    uint8_t read_buf[65];  // Status + max payload

    esp_err_t ret = i2c_master_write_read_device(
        handle->i2c_port,
        handle->i2c_addr,
        write_buf,
        3,
        read_buf,
        response_len + 1,
        pdMS_TO_TICKS(100)
    );

    if (ret == ESP_OK) {
        uint8_t status = read_buf[0];
        if (status == XVF3800_STATUS_SUCCESS) {
            memcpy(response, &read_buf[1], response_len);
            ESP_LOGD(TAG, "Read cmd OK, status=0x%02X, data[0]=0x%02X", status, read_buf[1]);
        } else {
            ESP_LOGD(TAG, "Read cmd status error: 0x%02X", status);
            return ESP_FAIL;
        }
    } else {
        // Detailed I2C error diagnosis
        const char* error_type = "UNKNOWN";
        if (ret == ESP_ERR_TIMEOUT) error_type = "TIMEOUT";
        else if (ret == ESP_FAIL) error_type = "NACK/BUS_ERROR";
        else if (ret == ESP_ERR_INVALID_STATE) error_type = "INVALID_STATE";

        ESP_LOGI(TAG, "🔍 I2C error detail: %s (%s) - ResID:0x%02X Cmd:0x%02X",
                 esp_err_to_name(ret), error_type, resource_id, cmd_id);
    }

    return ret;
}

esp_err_t xvf3800_read_gpi_all(xvf3800_handle_t *handle, uint32_t *bitmap)
{
    if (!handle || !bitmap) return ESP_ERR_INVALID_ARG;

    /*
     * GPI_VALUE_ALL command returns a 32-bit bitmap where bit n = GPI[n]
     * This is simpler than GPI_INDEX + GPI_VALUE as it reads all buttons at once
     */

    uint8_t response[4] = {0};  // uint32 = 4 bytes
    esp_err_t ret = xvf3800_read_cmd(handle, handle->resource_id_gpio,
                                      XVF3800_CMD_GPI_VALUE_ALL, response, sizeof(response));

    if (ret == ESP_OK) {
        // Combine bytes into uint32 (little-endian)
        *bitmap = (uint32_t)response[0] |
                  ((uint32_t)response[1] << 8) |
                  ((uint32_t)response[2] << 16) |
                  ((uint32_t)response[3] << 24);

        ESP_LOGD(TAG, "GPI_ALL bitmap: 0x%08lX (bit0=%d, bit1=%d)",
                 (unsigned long)*bitmap,
                 (*bitmap & 0x01) ? 1 : 0,
                 (*bitmap & 0x02) ? 1 : 0);
    } else {
        ESP_LOGD(TAG, "Failed to read GPI_VALUE_ALL: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t xvf3800_read_gpi(xvf3800_handle_t *handle, uint8_t pin_index, uint8_t *state)
{
    if (!handle || !state) return ESP_ERR_INVALID_ARG;

    /* Method 1: Use GPI_VALUE_ALL and extract specific bit (推荐) */
    uint32_t bitmap = 0;
    esp_err_t ret = xvf3800_read_gpi_all(handle, &bitmap);

    if (ret == ESP_OK) {
        *state = (bitmap & (1 << pin_index)) ? 1 : 0;
        ESP_LOGD(TAG, "GPI[%d] = %d (from bitmap)", pin_index, *state);
        return ESP_OK;
    }

    /* Method 2: Use GPI_INDEX + GPI_VALUE (fallback) */
    ESP_LOGD(TAG, "Trying fallback method: GPI_INDEX + GPI_VALUE");

    /*
     * According to XMOS XVF3800 User Guide:
     * 1. Write GPI_INDEX to select the pin
     * 2. Read GPI_VALUE to get the pin state
     */

    // Step 1: Set GPI_INDEX to select pin
    ret = xvf3800_write_cmd(handle, handle->resource_id_gpio,
                            XVF3800_CMD_GPI_INDEX, &pin_index, 1);

    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "Failed to set GPI_INDEX to %d: %s", pin_index, esp_err_to_name(ret));
        return ret;
    }

    // Small delay to allow device to process
    vTaskDelay(pdMS_TO_TICKS(5));

    // Step 2: Read GPI_VALUE
    uint8_t value = 0;
    ret = xvf3800_read_cmd(handle, handle->resource_id_gpio,
                           XVF3800_CMD_GPI_VALUE, &value, 1);

    if (ret == ESP_OK) {
        *state = value;
        ESP_LOGD(TAG, "GPI[%d] = %d", pin_index, value);
    } else {
        ESP_LOGD(TAG, "Failed to read GPI_VALUE: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t xvf3800_read_mute_button(xvf3800_handle_t *handle, bool *is_pressed)
{
    if (!handle || !is_pressed) return ESP_ERR_INVALID_ARG;

    uint8_t state = 0;
    esp_err_t ret = xvf3800_read_gpi(handle, XVF3800_GPI_MUTE_BUTTON, &state);

    if (ret == ESP_OK) {
        // X1D09: High when released, Low when pressed (from Seeed docs)
        *is_pressed = (state == XVF3800_BUTTON_PRESSED);
    }

    return ret;
}

static void button_monitor_task(void *arg)
{
    xvf3800_handle_t *handle = (xvf3800_handle_t *)arg;
    bool prev_mute_pressed = false;
    bool prev_set_pressed = false;
    int poll_count = 0;
    int consecutive_errors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 10;
    const int MAX_RETRIES = 3;
    uint32_t last_valid_bitmap = 0;
    bool was_in_failure = false;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Button monitor task started");
    ESP_LOGI(TAG, "Button mapping:");
    ESP_LOGI(TAG, "  - SET button   (GPI[1]) → START AI Agent");
    ESP_LOGI(TAG, "  - MUTE button  (GPI[0]) → STOP AI Agent");
    ESP_LOGI(TAG, "I2C: 0x%02X, Resource: 0x%02X", handle->i2c_addr, handle->resource_id_gpio);
    ESP_LOGI(TAG, "========================================");

    while (1) {
        // Read all GPIO states with retry logic
        uint32_t gpio_bitmap = 0;
        esp_err_t ret = ESP_FAIL;

        // Try multiple times with small delays
        for (int retry = 0; retry < MAX_RETRIES; retry++) {
            ret = xvf3800_read_gpi_all(handle, &gpio_bitmap);
            if (ret == ESP_OK) {
                break;  // Success, exit retry loop
            }
            // Small delay before retry
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        poll_count++;

        // Check if we're recovering from failure state
        if (ret == ESP_OK && was_in_failure) {
            ESP_LOGW(TAG, "========================================");
            ESP_LOGW(TAG, "🔄 RECOVERED from I2C failure at poll #%d", poll_count);
            ESP_LOGW(TAG, "   Previous bitmap: 0x%08lX", (unsigned long)last_valid_bitmap);
            ESP_LOGW(TAG, "   Current bitmap:  0x%08lX", (unsigned long)gpio_bitmap);

            // When button is pressed, I2C fails. When button is released, I2C recovers.
            // So bitmap after recovery should match the released state (same as before).
            // The very fact that I2C failed tells us a button was pressed.
            // We can use a simple heuristic: assume buttons were pressed during failure.

            ESP_LOGW(TAG, "   💡 Button press detected via I2C failure recovery");
            ESP_LOGW(TAG, "   Triggering button press events...");

            // Extract button states from current (recovered) bitmap
            bool mute_pressed_now = !(gpio_bitmap & (1 << XVF3800_GPI_MUTE_BUTTON));
            bool set_pressed_now = !(gpio_bitmap & (1 << XVF3800_GPI_ACTION_BUTTON));

            // If button is still pressed after recovery, process the press event
            // Otherwise, assume it was a quick press-release during the failure period
            if (!mute_pressed_now && !set_pressed_now) {
                // Both buttons released - this is the expected case
                // We can't tell which button was pressed, so trigger based on AI agent state
                ESP_LOGW(TAG, "   Buttons now released - checking which action to take");

                // Heuristic: Toggle AI agent state
                // If running, assume MUTE was pressed. If stopped, assume SET was pressed.
                if (g_app.b_ai_agent_joined) {
                    ESP_LOGW(TAG, "   → Assuming MUTE button was pressed (AI Agent is running)");
                    ESP_LOGI(TAG, "→ Stopping AI Agent...");
                    ai_agent_stop();
                    g_app.b_ai_agent_joined = false;
                    ESP_LOGI(TAG, "✓ AI Agent stopped");
                    prev_mute_pressed = false;
                } else {
                    ESP_LOGW(TAG, "   → Assuming SET button was pressed (AI Agent is stopped)");
                    ESP_LOGI(TAG, "→ Starting AI Agent...");
                    ai_agent_start();
                    g_app.b_ai_agent_joined = true;
                    ESP_LOGI(TAG, "✓ AI Agent started");
                    prev_set_pressed = false;
                }
            }

            ESP_LOGW(TAG, "========================================");
            was_in_failure = false;
        }

        // DEBUG: Log failures and periodic status
        if (ret != ESP_OK) {
            if (!was_in_failure) {
                ESP_LOGW(TAG, "⚠️  I2C FAILURE started at poll #%d (button pressed?)", poll_count);
                // Log the first failure with details to help diagnose
                ESP_LOGW(TAG, "    This helps us understand WHY I2C fails:");
                ESP_LOGW(TAG, "    - TIMEOUT = XVF3800 not responding");
                ESP_LOGW(TAG, "    - NACK = XVF3800 rejected the command");
                ESP_LOGW(TAG, "    - Check DEBUG logs above for error type");
            }
            was_in_failure = true;

            if (poll_count % 10 == 0) {
                ESP_LOGI(TAG, "[Poll #%d] ret=%s (retried %d times)",
                         poll_count, esp_err_to_name(ret), MAX_RETRIES);
            }
        } else {
            // Success - save valid bitmap
            last_valid_bitmap = gpio_bitmap;

            if (poll_count % 10 == 0) {
                ESP_LOGI(TAG, "[Poll #%d] ret=%s, bitmap=0x%08lX",
                         poll_count, esp_err_to_name(ret), (unsigned long)gpio_bitmap);
            }
        }

        // Print periodic status
        if (poll_count % 50 == 0) {
            if (ret == ESP_OK) {
                bool mute_state = (gpio_bitmap & (1 << XVF3800_GPI_MUTE_BUTTON)) ? true : false;
                bool set_state = (gpio_bitmap & (1 << XVF3800_GPI_ACTION_BUTTON)) ? true : false;

                ESP_LOGI(TAG, "✓ Monitoring (poll #%d): MUTE=%s, SET=%s",
                         poll_count,
                         mute_state ? "released" : "PRESSED",
                         set_state ? "released" : "PRESSED");
                consecutive_errors = 0;
            } else {
                consecutive_errors++;
                ESP_LOGW(TAG, "✗ Button read error #%d: %s",
                         consecutive_errors, esp_err_to_name(ret));

                if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                    ESP_LOGE(TAG, "========================================");
                    ESP_LOGE(TAG, "XVF3800 NOT RESPONDING after %d errors", consecutive_errors);
                    ESP_LOGE(TAG, "Possible issues:");
                    ESP_LOGE(TAG, "  1. Resource ID 0x%02X may be incorrect", handle->resource_id_gpio);
                    ESP_LOGE(TAG, "  2. Command IDs may need adjustment");
                    ESP_LOGE(TAG, "  3. XVF3800 firmware not initialized");
                    ESP_LOGE(TAG, "  4. I2C communication problem");
                    ESP_LOGE(TAG, "Try different Resource IDs: 0x00-0x10");
                    ESP_LOGE(TAG, "========================================");

                    // Reset error counter and wait before retry
                    consecutive_errors = 0;
                    vTaskDelay(pdMS_TO_TICKS(5000));
                }
            }
        }

        if (ret == ESP_OK) {
            // Extract button states from bitmap
            // Note: Active LOW - bit=0 means pressed, bit=1 means released
            bool mute_pressed = !(gpio_bitmap & (1 << XVF3800_GPI_MUTE_BUTTON));
            bool set_pressed = !(gpio_bitmap & (1 << XVF3800_GPI_ACTION_BUTTON));

            // Detect MUTE button press (to STOP AI agent)
            // Check for button press-release cycle (not just press state)
            // This handles the case where button was pressed during I2C failure
            if (mute_pressed && !prev_mute_pressed) {
                ESP_LOGW(TAG, "========================================");
                ESP_LOGW(TAG, "MUTE BUTTON PRESSED!");
                ESP_LOGW(TAG, "========================================");

                if (g_app.b_ai_agent_joined) {
                    ESP_LOGI(TAG, "→ Stopping AI Agent...");
                    ai_agent_stop();
                    g_app.b_ai_agent_joined = false;
                    ESP_LOGI(TAG, "✓ AI Agent stopped");
                } else {
                    ESP_LOGI(TAG, "AI Agent already stopped");
                }
            }

            // Detect SET button press (to START AI agent)
            if (set_pressed && !prev_set_pressed) {
                ESP_LOGW(TAG, "========================================");
                ESP_LOGW(TAG, "SET BUTTON PRESSED!");
                ESP_LOGW(TAG, "========================================");

                if (!g_app.b_ai_agent_joined) {
                    ESP_LOGI(TAG, "→ Starting AI Agent...");
                    ai_agent_start();
                    g_app.b_ai_agent_joined = true;
                    ESP_LOGI(TAG, "✓ AI Agent started");
                } else {
                    ESP_LOGI(TAG, "AI Agent already running");
                }
            }

            // Log button release events (optional, for debugging)
            if (!mute_pressed && prev_mute_pressed) {
                ESP_LOGD(TAG, "MUTE button released");
            }
            if (!set_pressed && prev_set_pressed) {
                ESP_LOGD(TAG, "SET button released");
            }

            prev_mute_pressed = mute_pressed;
            prev_set_pressed = set_pressed;
        } else {
            // During I2C failure, we can't read buttons
            // But we'll detect state change when we recover (see "RECOVERED" block above)
        }

        // Poll every 20ms for faster button response
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t xvf3800_start_button_monitor(xvf3800_handle_t *handle)
{
    if (!handle) return ESP_ERR_INVALID_ARG;

    g_handle = handle;

    BaseType_t ret = xTaskCreate(
        button_monitor_task,
        "xvf3800_button",
        4096,
        handle,
        5,
        &g_monitor_task
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button monitor task");
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "========================================");
    ESP_LOGW(TAG, "✓ XVF3800 Button Monitor Active");
    ESP_LOGW(TAG, "Controls:");
    ESP_LOGW(TAG, "  SET button  → Start AI Agent");
    ESP_LOGW(TAG, "  MUTE button → Stop AI Agent");
    ESP_LOGW(TAG, "========================================");

    return ESP_OK;
}

void xvf3800_stop_button_monitor(void)
{
    if (g_monitor_task) {
        vTaskDelete(g_monitor_task);
        g_monitor_task = NULL;
        ESP_LOGI(TAG, "Button monitor stopped");
    }
}
