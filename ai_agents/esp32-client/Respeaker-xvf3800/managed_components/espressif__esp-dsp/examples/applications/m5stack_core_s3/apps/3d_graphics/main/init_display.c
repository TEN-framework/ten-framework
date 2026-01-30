

#include "bsp/m5stack_core_s3.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include "esp_lcd_ili9341.h"
#include "esp_err.h"


static i2c_master_bus_handle_t i2c_handle = NULL;
static bool i2c_initialized = false;
static i2c_master_dev_handle_t axp2101_h = NULL;
static i2c_master_dev_handle_t aw9523_h = NULL;
static bool spi_initialized = false;

static const char *TAG = "3d-graphics";

#define BSP_AXP2101_ADDR    0x34
#define BSP_AW9523_ADDR     0x58

#define BSP_ERROR_CHECK_RETURN_ERR(x) do { \
    esp_err_t err_rc_ = (x);               \
    if (unlikely(err_rc_ != ESP_OK)) {     \
        return err_rc_;                    \
    }                                      \
} while(0)


#define ESP_RETURN_ON_ERROR(x, log_tag, format, ...) do {                                   \
    esp_err_t err_rc_ = (x);                                                                \
    if (unlikely(err_rc_ != ESP_OK)) {                                                      \
        ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);        \
        return err_rc_;                                                                     \
    }                                                                                       \
} while(0)

#define BSP_ERROR_CHECK_RETURN_NULL(x)  do { \
    if (unlikely((x) != ESP_OK)) {           \
        return NULL;                         \
    }                                        \
} while(0)

#define BSP_NULL_CHECK(x, ret) do { \
    if ((x) == NULL) {              \
        return ret;                 \
    }                               \
} while(0)

#define ESP_GOTO_ON_ERROR(x, goto_tag, log_tag, format, ...) do {                           \
    esp_err_t err_rc_ = (x);                                                                \
    if (unlikely(err_rc_ != ESP_OK)) {                                                      \
        ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);        \
        ret = err_rc_;                                                                      \
        goto goto_tag;                                                                      \
    }                                                                                       \
} while(0)



esp_err_t bsp_i2c_init(void)
{
    /* I2C was initialized before */
    if (i2c_initialized) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t i2c_config = {
        .i2c_port = BSP_I2C_NUM,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .clk_source = 1,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_config, &i2c_handle));

    // AXP2101 and AW9523 are managed by this BSP
    const i2c_device_config_t axp2101_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BSP_AXP2101_ADDR,
        .scl_speed_hz = 400000,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_master_bus_add_device(i2c_handle, &axp2101_config, &axp2101_h));
    const i2c_device_config_t aw9523_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BSP_AW9523_ADDR,
        .scl_speed_hz = 400000,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_master_bus_add_device(i2c_handle, &aw9523_config, &aw9523_h));

    i2c_initialized = true;
    return ESP_OK;
}

static esp_err_t dsp_display_brightness_init(void)
{
    /* Initilize I2C */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    const uint8_t lcd_bl_en[] = { 0x90, 0xBF }; // AXP DLDO1 Enable
    ESP_RETURN_ON_ERROR(i2c_master_transmit(axp2101_h, lcd_bl_en, sizeof(lcd_bl_en), 1000), TAG, "I2C write failed");
    const uint8_t lcd_bl_val[] = { 0x99, 0b00011000 };  // AXP DLDO1 voltage
    ESP_RETURN_ON_ERROR(i2c_master_transmit(axp2101_h, lcd_bl_val, sizeof(lcd_bl_val), 1000), TAG, "I2C write failed");

    return ESP_OK;
}


static esp_err_t bsp_spi_init(uint32_t max_transfer_sz)
{
    /* SPI was initialized before */
    if (spi_initialized) {
        return ESP_OK;
    }

    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_PCLK,
        .mosi_io_num = BSP_LCD_MOSI,
        .miso_io_num = BSP_LCD_MISO,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = max_transfer_sz,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    spi_initialized = true;

    return ESP_OK;
}

/* Features */
typedef enum {
    BSP_FEATURE_LCD,
    BSP_FEATURE_TOUCH,
    BSP_FEATURE_SD,
    BSP_FEATURE_SPEAKER,
    BSP_FEATURE_CAMERA,
} bsp_feature_t;

static esp_err_t bsp_enable_feature(bsp_feature_t feature)
{
    esp_err_t err = ESP_OK;
    static uint8_t aw9523_P0 = 0b10;
    static uint8_t aw9523_P1 = 0b10100000;
    uint8_t data[2];

    /* Initilize I2C */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    switch (feature) {
    case BSP_FEATURE_LCD:
        /* Enable LCD */
        aw9523_P1 |= (1 << 1);
        break;
    case BSP_FEATURE_TOUCH:
        /* Enable Touch */
        aw9523_P0 |= (1);
        break;
    case BSP_FEATURE_SD:
        /* AXP ALDO4 voltage / SD Card / 3V3 */
        data[0] = 0x95;
        data[1] = 0b00011100; //3V3
        err |= i2c_master_transmit(axp2101_h, data, sizeof(data), 1000);
        /* Enable SD */
        aw9523_P0 |= (1 << 4);
        break;
    case BSP_FEATURE_SPEAKER:
        /* AXP ALDO1 voltage / PA PVDD / 1V8 */
        data[0] = 0x92;
        data[1] = 0b00001101; //1V8
        err |= i2c_master_transmit(axp2101_h, data, sizeof(data), 1000);
        /* AXP ALDO2 voltage / Codec / 3V3 */
        data[0] = 0x93;
        data[1] = 0b00011100; //3V3
        err |= i2c_master_transmit(axp2101_h, data, sizeof(data), 1000);
        /* AXP ALDO3 voltage / Codec+Mic / 3V3 */
        data[0] = 0x94;
        data[1] = 0b00011100; //3V3
        err |= i2c_master_transmit(axp2101_h, data, sizeof(data), 1000);
        /* AW9523 P0 is in push-pull mode */
        data[0] = 0x11;
        data[1] = 0x10;
        err |= i2c_master_transmit(aw9523_h, data, sizeof(data), 1000);
        /* Enable Codec AW88298 */
        aw9523_P0 |= (1 << 2);
        break;
    case BSP_FEATURE_CAMERA:
        /* Enable Camera */
        aw9523_P1 |= (1);
        break;
    }

    data[0] = 0x02;
    data[1] = aw9523_P0;
    err |= i2c_master_transmit(aw9523_h, data, sizeof(data), 1000);

    data[0] = 0x03;
    data[1] = aw9523_P1;
    err |= i2c_master_transmit(aw9523_h, data, sizeof(data), 1000);

    return err;
}

#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8
#define LCD_LEDC_CH            CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH

esp_err_t dsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;
    assert(config != NULL && config->max_transfer_sz > 0);

    BSP_ERROR_CHECK_RETURN_ERR(bsp_enable_feature(BSP_FEATURE_LCD));

    /* Initialize SPI */
    ESP_RETURN_ON_ERROR(bsp_spi_init(config->max_transfer_sz), TAG, "");

    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,
        .cs_gpio_num = BSP_LCD_CS,
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, ret_io), err, TAG, "New panel IO failed");

    ESP_LOGD(TAG, "Install LCD driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST, // Shared with Touch reset
        .color_space = BSP_LCD_COLOR_SPACE,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_ili9341(*ret_io, &panel_config, ret_panel), err, TAG, "New panel failed");

    esp_lcd_panel_reset(*ret_panel);
    esp_lcd_panel_init(*ret_panel);
    esp_lcd_panel_invert_color(*ret_panel, true);
    return ret;

err:
    if (*ret_panel) {
        esp_lcd_panel_del(*ret_panel);
    }
    if (*ret_io) {
        esp_lcd_panel_io_del(*ret_io);
    }
    spi_bus_free(BSP_LCD_SPI_NUM);
    return ret;
}

static lv_display_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = NULL;
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = BSP_LCD_DRAW_BUFF_SIZE * sizeof(uint16_t),
    };
    BSP_ERROR_CHECK_RETURN_NULL(dsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle));

    esp_lcd_panel_disp_on_off(panel_handle, true);

    lv_display_t *display = lv_display_create(BSP_LCD_H_RES, BSP_LCD_V_RES);

    lv_display_set_user_data(display, panel_handle);
    // set color depth
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    return display;
}

static lv_display_t *disp;

static lv_display_t *dsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    BSP_ERROR_CHECK_RETURN_NULL(lvgl_port_init(&cfg->lvgl_port_cfg));

    BSP_ERROR_CHECK_RETURN_NULL(dsp_display_brightness_init());

    BSP_NULL_CHECK(disp = bsp_display_lcd_init(cfg), NULL);

    return disp;
}

lv_display_t *display_init(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
        }
    };
    cfg.lvgl_port_cfg.task_affinity = 1; /* For camera */
    return dsp_display_start_with_config(&cfg);
}
