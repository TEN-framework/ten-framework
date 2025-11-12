list(APPEND SOURCES
        drivers/display/lws-display.c
        drivers/display/ssd1306-i2c.c
        drivers/display/ili9341-spi.c
	drivers/i2c/lws-i2c.c
        drivers/i2c/bitbang/lws-bb-i2c.c
        drivers/spi/lws-spi.c
        drivers/spi/bitbang/lws-bb-spi.c
	drivers/button/lws-button.c
	drivers/led/led-gpio.c
	drivers/led/led-seq.c
	drivers/pwm/pwm.c
	drivers/settings/settings.c
)

if (LWS_WITH_NETWORK)
	list(APPEND SOURCES
		drivers/netdev/netdev.c
		drivers/netdev/wifi.c)
endif()

if (LWS_ESP_PLATFORM)
	list(APPEND SOURCES
		plat/freertos/esp32/drivers/gpio-esp32.c
		plat/freertos/esp32/drivers/pwm-esp32.c
		)
endif()

exports_to_parent_scope()

