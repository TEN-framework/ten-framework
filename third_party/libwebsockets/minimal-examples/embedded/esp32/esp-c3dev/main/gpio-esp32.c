#include <driver/gpio.h>
#include "gpio-esp32.h"
	
static void
lws_gpio_esp32_mode_write(_lws_plat_gpio_t gpio)
{
	gpio_reset_pin(gpio);
	gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
	gpio_set_direction(gpio, GPIO_MODE_INPUT_OUTPUT);
	gpio_set_level(gpio, 1);
}
static void
lws_gpio_esp32_mode_read(_lws_plat_gpio_t gpio)
{
	gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
	gpio_set_direction(gpio, GPIO_MODE_INPUT);
	gpio_set_level(gpio, 1);
}
static int
lws_gpio_esp32_read(_lws_plat_gpio_t gpio)
{
	return gpio_get_level(gpio);
}
static void
lws_gpio_esp32_set(_lws_plat_gpio_t gpio, int val)
{
	gpio_set_level(gpio, val);
}

const lws_gpio_ops_t lws_gpio_esp32 = {
	.mode_write		= lws_gpio_esp32_mode_write,
	.mode_read		= lws_gpio_esp32_mode_read,
	.read			= lws_gpio_esp32_read,
	.set			= lws_gpio_esp32_set,
};

