#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"

// AIC3104 I2C address
#define AIC3104_ADDR 0x18

// Registers (page 0)
#define AIC3104_PAGE_CTRL        0x00
#define AIC3104_LEFT_DAC_VOLUME  0x2B
#define AIC3104_RIGHT_DAC_VOLUME 0x2C
#define AIC3104_HPLOUT_LEVEL     0x33
#define AIC3104_HPROUT_LEVEL     0x41
#define AIC3104_LEFT_LOP_LEVEL   0x56
#define AIC3104_RIGHT_LOP_LEVEL  0x5D

typedef struct {
    i2c_port_t i2c_port;
    int sda_gpio;
    int scl_gpio;
    uint32_t speed_hz;
} aic3104_ng_t;

// init I2C bus (using legacy driver)
esp_err_t aic3104_ng_init(aic3104_ng_t *ctx, int i2c_port, int sda_gpio, int scl_gpio, uint32_t speed_hz);

// low-level reg rw
esp_err_t aic3104_ng_write(aic3104_ng_t *ctx, uint8_t reg, uint8_t val);
esp_err_t aic3104_ng_read(aic3104_ng_t *ctx, uint8_t reg, uint8_t *val);

// quick sanity test: write page 0 and read it back
esp_err_t aic3104_ng_probe(aic3104_ng_t *ctx, uint8_t *page_val_out);

// apply the same minimal setup as your Arduino example
esp_err_t aic3104_ng_setup_default(aic3104_ng_t *ctx);
