/*
 * esp32 / esp-idf pwm
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

#include "private-lib-core.h"
#include "soc/ledc_reg.h"
#include "driver/ledc.h"

#define _LEDC_HIGH_SPEED_MODE 0

static const ledc_timer_config_t tc = {
	.speed_mode             	= _LEDC_HIGH_SPEED_MODE,
	.duty_resolution        	= LEDC_TIMER_13_BIT,
	.timer_num              	= LEDC_TIMER_0,
	.freq_hz                	= 5000,
	.clk_cfg                	= LEDC_AUTO_CLK
};

int
lws_pwm_plat_init(const struct lws_pwm_ops *lo)
{
	ledc_channel_config_t lc = {
		.duty			= 8191,
		.intr_type		= LEDC_INTR_FADE_END,
		.speed_mode		= _LEDC_HIGH_SPEED_MODE,
		.timer_sel		= LEDC_TIMER_0,
	};
	size_t n;

        ledc_timer_config(&tc);

        for (n = 0; n < lo->count_pwm_map; n++) {
        	lc.channel = LEDC_CHANNEL_0 + lo->pwm_map[n].index;
        	lc.gpio_num = lo->pwm_map[n].gpio;
        	ledc_channel_config(&lc);
                ledc_set_duty(_LEDC_HIGH_SPEED_MODE, lc.channel, 0);
                ledc_update_duty(_LEDC_HIGH_SPEED_MODE, lc.channel);
        }

	return 0;
}

void
lws_pwm_plat_intensity(const struct lws_pwm_ops *lo, _lws_plat_gpio_t gpio,
		       lws_led_intensity_t inten)
{
	size_t n;

	for (n = 0; n < lo->count_pwm_map; n++) {
		if (lo->pwm_map[n].gpio == gpio) {
			if (!lo->pwm_map[n].active_level)
				inten = 65535 - inten;
			ledc_set_duty(_LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0 +
					lo->pwm_map[n].index, inten >> 3);
			ledc_update_duty(_LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0 +
					lo->pwm_map[n].index);
			return;
		}
	}

	lwsl_err("%s: unknown gpio for pwm\n", __func__);
}
