/*
 * lws generic gpio - esp32 platform wrapper
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

typedef int _lws_plat_gpio_t;
#include "gpio.h"

extern const lws_gpio_ops_t lws_gpio_esp32;
