/*
 * lws abstract display
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

#if !defined(__LWS_DISPLAY_H__)
#define __LWS_DISPLAY_H__

#include <stdint.h>

typedef uint16_t lws_display_scalar;

/*
 * This is embedded in the actual display implementation object at the top,
 * so a pointer to this can be cast to a pointer to the implementation object
 * by any code that is specific to how it was implemented.
 *
 * Notice for the backlight / display intensity we contain pwm_ops... these can
 * be some other pwm_ops like existing gpio pwm ops, or handled in a customized
 * way like set oled contrast.  Either way, the pwm level is arrived at via a
 * full set of lws_led_sequences capable of generic lws transitions
 */

typedef struct lws_display {
	int (*init)(const struct lws_display *disp);
	const lws_pwm_ops_t		*bl_pwm_ops;
	int (*contrast)(const struct lws_display *disp, uint8_t contrast);
	int (*blit)(const struct lws_display *disp, const uint8_t *src,
		    lws_display_scalar x, lws_display_scalar y,
		    lws_display_scalar w, lws_display_scalar h);
	int (*power)(const struct lws_display *disp, int state);

	const lws_led_sequence_def_t	*bl_active;
	const lws_led_sequence_def_t	*bl_dim;
	const lws_led_sequence_def_t	*bl_transition;

	void				*variant;

	int				bl_index;

	lws_display_scalar		w;
	/**< display surface width in pixels */
	lws_display_scalar		h;
	/**< display surface height in pixels */

	uint8_t				latency_wake_ms;
	/**< ms required after wake from sleep before display usable again...
	 * delay bringing up the backlight for this amount of time on wake.
	 * This is managed via a sul on the event loop, not blocking. */
} lws_display_t;

/*
 * This contains dynamic data related to display state
 */

enum lws_display_controller_state {
	LWSDISPS_OFF,
	LWSDISPS_AUTODIMMED,	  /* is in pre- blanking static dim mode */
	LWSDISPS_BECOMING_ACTIVE, /* waiting for wake latency before active */
	LWSDISPS_ACTIVE,	  /* is active */
	LWSDISPS_GOING_OFF	  /* dimming then off */
};

typedef struct lws_display_state {

	lws_sorted_usec_list_t		sul_autodim;
	const lws_display_t		*disp;
	struct lws_context		*ctx;

	int				autodim_ms;
	int				off_ms;

	struct lws_led_state		*bl_lcs;

	lws_led_state_chs_t		chs;
	/* set of sequencer transition channels */

	enum lws_display_controller_state state;

} lws_display_state_t;

/**
 * lws_display_state_init() - initialize display states
 *
 * \param lds: the display state object
 * \param ctx: the lws context
 * \param autodim_ms: ms since last active report to dim display (<0 = never)
 * \param off_ms: ms since dim to turn display off (<0 = never)
 * \param bl_lcs: the led controller instance that has the backlight
 * \param disp: generic display object we belong to
 *
 * This initializes a display's state, and sets up the optional screen auto-dim
 * and blanking on inactive, and gradual brightness change timer.
 *
 *  - auto-dim then off: set autodim to some ms and off_ms to some ms
 *  - auto-dim only: set autodim to some ms and off_ms to -1
 *  - off-only: set autodim to some ms and off_ms to 0
 *  - neither: set both autodim and off_ms to -1
 */
LWS_VISIBLE LWS_EXTERN void
lws_display_state_init(lws_display_state_t *lds, struct lws_context *ctx,
		       int autodim_ms, int off_ms, struct lws_led_state *bl_lcs,
		       const lws_display_t *disp);

/**
 * lws_display_state_set_brightness() - gradually change the brightness
 *
 * \param lds: the display state we are changing
 * \param target: the target brightness to transition to
 *
 * Adjusts the brightness gradually twoards the target at 20Hz
 */
LWS_VISIBLE LWS_EXTERN void
lws_display_state_set_brightness(lws_display_state_t *lds,
				 const lws_led_sequence_def_t *pwmseq);

/*
 * lws_display_state_active() - inform the system the display is active
 *
 * \param lds: the display state we are marking as active
 *
 * Resets the auto-dim and auto-off timers and makes sure the display is on and
 * at the active brightness level
 */
LWS_VISIBLE LWS_EXTERN void
lws_display_state_active(lws_display_state_t *lds);

/*
 * lws_display_state_off() - turns off the related display
 *
 * \param lds: the display state we are turning off
 *
 * Turns the display to least power mode or completely off if possible.
 * Disables the timers related to dimming and blanking.
 */
LWS_VISIBLE LWS_EXTERN void
lws_display_state_off(lws_display_state_t *lds);

#endif
