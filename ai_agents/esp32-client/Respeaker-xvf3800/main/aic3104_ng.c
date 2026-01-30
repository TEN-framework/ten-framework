#include "aic3104_ng.h"
#include "esp_log.h"

static const char *TAG = "AIC3104_NG";

// I2C bus scanner to detect devices
esp_err_t aic3104_i2c_scan(i2c_port_t i2c_port)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    int found = 0;

    // Common addresses to check explicitly
    uint8_t important_addrs[] = {0x18, 0x21, 0x2C};
    const char *names[] = {"AIC3104", "PCAL6416A", "XVF3800"};

    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            // Check if it's a known device
            const char *device_name = "Unknown";
            for (int i = 0; i < 3; i++) {
                if (addr == important_addrs[i]) {
                    device_name = names[i];
                    break;
                }
            }
            ESP_LOGW(TAG, "  Found device at address 0x%02X (%s)", addr, device_name);
            found++;
        }
    }

    if (found == 0) {
        ESP_LOGW(TAG, "No I2C devices detected on bus");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Found %d I2C device(s)", found);
    return ESP_OK;
}

esp_err_t aic3104_ng_init(aic3104_ng_t *ctx, int i2c_port, int sda_gpio, int scl_gpio, uint32_t speed_hz)
{
    if (!ctx) return ESP_ERR_INVALID_ARG;

    ctx->i2c_port = i2c_port;
    ctx->sda_gpio = sda_gpio;
    ctx->scl_gpio = scl_gpio;
    ctx->speed_hz = speed_hz ? speed_hz : 100000;

    // Try to delete existing I2C driver (ignore errors if not installed)
    i2c_driver_delete(i2c_port);  // Silently ignore errors

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = ctx->speed_hz,
    };

    esp_err_t ret = i2c_param_config(i2c_port, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(i2c_port, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C initialized: port=%d SDA=GPIO%d SCL=GPIO%d speed=%lu Hz",
             i2c_port, sda_gpio, scl_gpio, (unsigned long)ctx->speed_hz);

    // Scan I2C bus to detect devices
    ret = aic3104_i2c_scan(i2c_port);

    return ret;  // Return scan result
}

esp_err_t aic3104_ng_write(aic3104_ng_t *ctx, uint8_t reg, uint8_t val)
{
    if (!ctx) return ESP_ERR_INVALID_STATE;

    uint8_t buf[2] = { reg, val };
    return i2c_master_write_to_device(ctx->i2c_port, AIC3104_ADDR, buf, sizeof(buf), pdMS_TO_TICKS(50));
}

esp_err_t aic3104_ng_read(aic3104_ng_t *ctx, uint8_t reg, uint8_t *val)
{
    if (!ctx || !val) return ESP_ERR_INVALID_ARG;

    return i2c_master_write_read_device(ctx->i2c_port, AIC3104_ADDR, &reg, 1, val, 1, pdMS_TO_TICKS(50));
}

esp_err_t aic3104_ng_probe(aic3104_ng_t *ctx, uint8_t *page_val_out)
{
    if (!ctx) return ESP_ERR_INVALID_ARG;

    ESP_LOGW(TAG, "probe: write page 0");
    esp_err_t ret = aic3104_ng_write(ctx, AIC3104_PAGE_CTRL, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write page failed: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t v = 0xFF;
    ret = aic3104_ng_read(ctx, AIC3104_PAGE_CTRL, &v);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read page failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGW(TAG, "probe ok: page reg=0x%02X", v);
    if (page_val_out) *page_val_out = v;
    return ESP_OK;
}

esp_err_t aic3104_ng_setup_default(aic3104_ng_t *ctx)
{
    if (!ctx) return ESP_ERR_INVALID_ARG;

    esp_err_t ret;

    // page 0
    ret = aic3104_ng_write(ctx, AIC3104_PAGE_CTRL, 0x00);
    if (ret != ESP_OK) return ret;

    // 0dB DAC
    ret = aic3104_ng_write(ctx, AIC3104_LEFT_DAC_VOLUME, 0x00);
    if (ret != ESP_OK) return ret;
    ret = aic3104_ng_write(ctx, AIC3104_RIGHT_DAC_VOLUME, 0x00);
    if (ret != ESP_OK) return ret;

    // outputs to 0dB, unmuted, powered up (same values as your Arduino sketch)
    ret = aic3104_ng_write(ctx, AIC3104_HPLOUT_LEVEL, 0x0D);
    if (ret != ESP_OK) return ret;
    ret = aic3104_ng_write(ctx, AIC3104_HPROUT_LEVEL, 0x0D);
    if (ret != ESP_OK) return ret;

    ret = aic3104_ng_write(ctx, AIC3104_LEFT_LOP_LEVEL, 0x0B);
    if (ret != ESP_OK) return ret;
    ret = aic3104_ng_write(ctx, AIC3104_RIGHT_LOP_LEVEL, 0x0B);
    if (ret != ESP_OK) return ret;

    ESP_LOGW(TAG, "default setup applied");
    return ESP_OK;
}
