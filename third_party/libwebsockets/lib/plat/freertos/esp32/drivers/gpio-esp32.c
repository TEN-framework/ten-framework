/*
 * esp32 / esp-idf gpio
 *
 * Copyright (C) 2019 - 2020 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <libwebsockets.h>
	
static void
lws_gpio_esp32_mode(_lws_plat_gpio_t gpio, int flags)
{
	int mode, pup = GPIO_FLOATING;

	switch (flags & (LWSGGPIO_FL_READ | LWSGGPIO_FL_WRITE)) {
	default:
		lwsl_err("%s: neither read nor write\n", __func__);
		return;
	case LWSGGPIO_FL_READ:
		mode = GPIO_MODE_INPUT;
		break;
	case LWSGGPIO_FL_WRITE:
		mode = GPIO_MODE_OUTPUT;
		break;
	case LWSGGPIO_FL_READ | LWSGGPIO_FL_WRITE:
		mode = GPIO_MODE_INPUT_OUTPUT;
		break;
	}

	switch (flags & (LWSGGPIO_FL_PULLUP | LWSGGPIO_FL_PULLDOWN)) {
	default:
		break;
	case LWSGGPIO_FL_PULLUP:
		pup = GPIO_PULLUP_ONLY;
		break;
	case LWSGGPIO_FL_PULLDOWN:
		pup = GPIO_PULLDOWN_ONLY;
		break;
	case LWSGGPIO_FL_PULLUP | LWSGGPIO_FL_PULLDOWN:
		pup = GPIO_PULLUP_PULLDOWN;
		break;
	}

	gpio_reset_pin(gpio);
	gpio_set_direction(gpio, mode);
	gpio_set_pull_mode(gpio, pup);
	gpio_set_level(gpio, flags & LWSGGPIO_FL_START_LOW ? 0 : 1);
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

static int
lws_gpio_esp32_irq_mode(_lws_plat_gpio_t gpio, lws_gpio_irq_t irq_type,
			lws_gpio_irq_cb_t cb, void *arg)
{
	if (gpio_set_intr_type(gpio, irq_type))
		return 1;

	if (cb)
		return gpio_isr_handler_add(gpio, cb, arg);

	return gpio_isr_handler_remove(gpio);
}

const lws_gpio_ops_t lws_gpio_plat = {
	.mode			= lws_gpio_esp32_mode,
	.read			= lws_gpio_esp32_read,
	.set			= lws_gpio_esp32_set,
	.irq_mode		= lws_gpio_esp32_irq_mode,
};
