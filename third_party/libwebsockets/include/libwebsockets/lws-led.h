/*
 * Generic LED controller ops
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
 *
 * This is like an abstract class for leds, a real implementation provides
 * functions for the ops that use the underlying, eg, OS gpio arrangements.
 */

/* only b15 significant for GPIO */
typedef uint16_t lws_led_intensity_t;
typedef uint16_t lws_led_seq_phase_t;

/* the normalized max intensity */
#define LWS_LED_MAX_INTENSITY			(0xffff)

/* the normalized 360 degree phase count for intensity functions */
#define LWS_LED_FUNC_PHASE			65536
/* used when the sequence doesn't stop by itself and goes around forever */
#define LWS_SEQ_LEDPHASE_TOTAL_ENDLESS		(-1)

#define LWS_LED_SEQUENCER_UPDATE_INTERVAL_MS	33

struct lws_led_state; /* opaque */
struct lws_pwm_ops; /* forward ref */

typedef lws_led_intensity_t (*lws_led_lookup_t)(lws_led_seq_phase_t ph);

typedef struct lws_led_sequence_def_t {
	lws_led_lookup_t		func;
	lws_led_seq_phase_t		ledphase_offset;
	int				ledphase_total; /* 65536= one cycle */
	uint16_t			ms;
	uint8_t				flags;
} lws_led_sequence_def_t;

enum {
	LLSI_CURR,
	LLSI_NEXT,
	LLSI_TRANS
};

typedef struct lws_led_state_ch
{
	const lws_led_sequence_def_t		*seq; /* NULL = inactive */
	lws_led_seq_phase_t			ph;
	lws_led_seq_phase_t			step;
	int					phase_budget;
	lws_led_intensity_t			last;
	/**< at the end of the sequence we decouple the sequencer, but leave
	 * the last computed sample behind for further transitions to base off
	 */
} lws_led_state_ch_t;

typedef struct lws_led_state_chs
{
	lws_led_state_ch_t			seqs[3];
} lws_led_state_chs_t;

/* this should always be first in the subclassed implementation types */

typedef struct lws_led_ops {
	void (*intensity)(const struct lws_led_ops *lo, const char *name,
			  lws_led_intensity_t inten);
	/**< for BOOL led control like GPIO, only inten b15 is significant */
	struct lws_led_state * (*create)(const struct lws_led_ops *led_ops);
	void (*destroy)(struct lws_led_state *);
} lws_led_ops_t;

typedef struct lws_led_gpio_map {
	const char			*name;
	_lws_plat_gpio_t		gpio;
	lws_led_lookup_t		intensity_correction;
	/**< May be NULL.  If GPIO-based LED, ignored.  If pwm_ops provided,
	 * NULL means use default CIE 100% correction function.  If non-NULL,
	 * use the pointed-to correction function.  This is useful to provide
	 * LED-specific intensity correction / scaling so different types of
	 * LED can "look the same". */
	const struct lws_pwm_ops	*pwm_ops;
	/**< if NULL, gpio controls the led directly.  If set to a pwm_ops,
	 * the led control is outsourced to the pwm controller. */
	uint8_t				active_level;
} lws_led_gpio_map_t;

typedef struct lws_led_gpio_controller {
	const lws_led_ops_t		led_ops;

	const lws_gpio_ops_t		*gpio_ops;
	const lws_led_gpio_map_t	*led_map;
	uint8_t				count_leds;
} lws_led_gpio_controller_t;

/* ops */

LWS_VISIBLE LWS_EXTERN struct lws_led_state *
lws_led_gpio_create(const lws_led_ops_t *led_ops);

LWS_VISIBLE LWS_EXTERN void
lws_led_gpio_destroy(struct lws_led_state *lcs);

/**
 * lws_led_gpio_intensity() - set the static intensity of an led
 *
 * \param lo: the base class of the led controller
 * \param index: which led in the controller set
 * \param inten: 16-bit unsigned intensity
 *
 * For LEDs controlled by a BOOL like GPIO, only inten b15 is significant.
 * For PWM type LED control, as many bits as the hardware can support from b15
 * down are significant.
 */
LWS_VISIBLE LWS_EXTERN void
lws_led_gpio_intensity(const struct lws_led_ops *lo, const char *name,
		       lws_led_intensity_t inten);

LWS_VISIBLE LWS_EXTERN int
lws_led_transition(struct lws_led_state *lcs, const char *name,
		   const lws_led_sequence_def_t *next,
		   const lws_led_sequence_def_t *trans);


#define lws_led_gpio_ops \
	{ \
		.create		= lws_led_gpio_create, \
		.destroy	= lws_led_gpio_destroy, \
		.intensity	= lws_led_gpio_intensity, \
	}

