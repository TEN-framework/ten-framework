/*
 * Generic GPIO led
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
#include "drivers/led/private-lib-drivers-led.h"

#if defined(LWS_PLAT_TIMER_CB)
static LWS_PLAT_TIMER_CB(lws_led_timer_cb, th)
{
	lws_led_state_t *lcs = LWS_PLAT_TIMER_CB_GET_OPAQUE(th);

	lws_seq_timer_handle(lcs);
}
#endif

struct lws_led_state *
lws_led_gpio_create(const lws_led_ops_t *led_ops)
{
	lws_led_gpio_controller_t *lgc = (lws_led_gpio_controller_t *)led_ops;
	/*
	 * We allocate the main state object, and a 3 x seq dynamic footprint
	 * for each led, since it may be sequencing the transition between two
	 * other sequences.
	 */

	lws_led_state_t *lcs = lws_zalloc(sizeof(lws_led_state_t) +
				(lgc->count_leds * sizeof(lws_led_state_chs_t)),
				__func__);
	int n;

	if (!lcs)
		return NULL;

	lcs->controller = lgc;

#if defined(LWS_PLAT_TIMER_CREATE)
	lcs->timer = LWS_PLAT_TIMER_CREATE("leds",
			LWS_LED_SEQUENCER_UPDATE_INTERVAL_MS, 1, lcs,
				  (TimerCallbackFunction_t)lws_led_timer_cb);
	if (!lcs->timer)
		return NULL;
#endif

	for (n = 0; n < lgc->count_leds; n++) {
		const lws_led_gpio_map_t *map = &lgc->led_map[n];

		if (map->pwm_ops) {
			lgc->gpio_ops->mode(map->gpio, LWSGGPIO_FL_READ);
			lgc->gpio_ops->set(map->gpio, 0);
		} else {
			lgc->gpio_ops->mode(map->gpio, LWSGGPIO_FL_WRITE);
			lgc->gpio_ops->set(map->gpio,
					   !lgc->led_map[n].active_level);
		}
	}

	return lcs;
}

void
lws_led_gpio_destroy(struct lws_led_state *lcs)
{
#if defined(LWS_PLAT_TIMER_DELETE)
	LWS_PLAT_TIMER_DELETE(lcs->timer);
#endif
	lws_free(lcs);
}

int
lws_led_gpio_lookup(const struct lws_led_ops *lo, const char *name)
{
	const lws_led_gpio_controller_t *lgc = (lws_led_gpio_controller_t *)lo;
	int n;

	for (n = 0; n < lgc->count_leds; n++)
		if (!strcmp(name, lgc->led_map[n].name))
			return n;

	return -1;
}

void
lws_led_gpio_intensity(const struct lws_led_ops *lo, const char *name,
		       lws_led_intensity_t inten)
{
	const lws_led_gpio_controller_t *lgc = (lws_led_gpio_controller_t *)lo;
	int idx = lws_led_gpio_lookup(lo, name);
	const lws_led_gpio_map_t *map;

	if (idx < 0)
		return;

	map = &lgc->led_map[idx];

	if (map->pwm_ops)
		map->pwm_ops->intensity(map->pwm_ops, map->gpio, inten);
	else
		lgc->gpio_ops->set(map->gpio,
				(!!map->active_level) ^ !(inten & 0x8000));
}
