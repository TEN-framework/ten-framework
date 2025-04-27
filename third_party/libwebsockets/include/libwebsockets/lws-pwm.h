/*
 * Generic PWM controller ops
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

typedef struct lws_pwm_map {
	_lws_plat_gpio_t		gpio;
	uint8_t				index;
	uint8_t				active_level;
} lws_pwm_map_t;

typedef struct lws_pwm_ops {
	int (*init)(const struct lws_pwm_ops *lo);
	void (*intensity)(const struct lws_pwm_ops *lo, _lws_plat_gpio_t gpio,
			  lws_led_intensity_t inten);
	const lws_pwm_map_t		*pwm_map;
	uint8_t				count_pwm_map;
} lws_pwm_ops_t;

LWS_VISIBLE LWS_EXTERN int
lws_pwm_plat_init(const struct lws_pwm_ops *lo);

LWS_VISIBLE LWS_EXTERN void
lws_pwm_plat_intensity(const struct lws_pwm_ops *lo, _lws_plat_gpio_t gpio,
		       lws_led_intensity_t inten);

#define lws_pwm_plat_ops \
		.init			= lws_pwm_plat_init, \
		.intensity		= lws_pwm_plat_intensity

/*
 * May be useful for making your own transitions or sequences
 */

LWS_VISIBLE LWS_EXTERN lws_led_intensity_t
lws_led_func_linear(lws_led_seq_phase_t n);
LWS_VISIBLE LWS_EXTERN lws_led_intensity_t
lws_led_func_sine(lws_led_seq_phase_t n);

/* canned sequences that can work out of the box */

extern const lws_led_sequence_def_t lws_pwmseq_sine_endless_slow,
				    lws_pwmseq_sine_endless_fast,
				    lws_pwmseq_linear_wipe,
				    lws_pwmseq_sine_up, lws_pwmseq_sine_down,
				    lws_pwmseq_static_on,
				    lws_pwmseq_static_half,
				    lws_pwmseq_static_off;
