/*
 * Generic GPIO / irq buttons
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

typedef enum lws_button_classify_states {
	LBCS_IDLE,		/* nothing happening */
	LBCS_MIN_DOWN_QUALIFY,

	LBCS_ASSESS_DOWN_HOLD,
	LBCS_UP_SETTLE1,
	LBCS_WAIT_DOUBLECLICK,
	LBCS_MIN_DOWN_QUALIFY2,

	LBCS_WAIT_UP,
	LBCS_UP_SETTLE2,
} lws_button_classify_states_t;

/*
 * This is the opaque, allocated, non-const, dynamic footprint of the
 * button controller
 */

typedef struct lws_button_state {
#if defined(LWS_PLAT_TIMER_TYPE)
	LWS_PLAT_TIMER_TYPE			timer;	   /* bh timer */
	LWS_PLAT_TIMER_TYPE			timer_mon; /* monitor timer */
#endif
	const lws_button_controller_t		*controller;
	struct lws_context			*ctx;
	short					mon_refcount;
	lws_button_idx_t			enable_bitmap;
	lws_button_idx_t			state_bitmap;

	uint16_t				mon_timer_count;
	/* incremented each time the mon timer cb happens */

	/* lws_button_each_t per button overallocated after this */
} lws_button_state_t;

typedef struct lws_button_each {
	lws_button_state_t			*bcs;
	uint16_t				mon_timer_comp;
	uint8_t					state;
	/**^ lws_button_classify_states_t */
	uint8_t					isr_pending;
} lws_button_each_t;

#if defined(LWS_PLAT_TIMER_START)
static const lws_button_regime_t default_regime = {
	.ms_min_down			= 20,
	.ms_min_down_longpress		= 300,
	.ms_up_settle			= 20,
	.ms_doubleclick_grace		= 120,
	.flags				= LWSBTNRGMFLAG_CLASSIFY_DOUBLECLICK
};
#endif


/*
 * This is happening in interrupt context, we have to schedule a bottom half to
 * do the foreground lws_smd queueing, using, eg, a platform timer.
 *
 * All the buttons point here and use one timer per button controller.  An
 * interrupt here means, "something happened to one or more buttons"
 */
#if defined(LWS_PLAT_TIMER_START)
void
lws_button_irq_cb_t(void *arg)
{
	lws_button_each_t *each = (lws_button_each_t *)arg;

	each->isr_pending = 1;
	LWS_PLAT_TIMER_START(each->bcs->timer);
}
#endif

/*
 * This is the bottom-half scheduled via a timer set in the ISR.  From here
 * we are allowed to hold mutexes etc.  We are coming here because any button
 * interrupt arrived, we have to try to figure out which events have happened.
 */

#if defined(LWS_PLAT_TIMER_CB)
static LWS_PLAT_TIMER_CB(lws_button_bh, th)
{
	lws_button_state_t *bcs = LWS_PLAT_TIMER_CB_GET_OPAQUE(th);
	const lws_button_controller_t *bc = bcs->controller;
	lws_button_each_t *each = (lws_button_each_t *)&bcs[1];
	size_t n;

	/*
	 * The ISR and bottom-half is shared by all the buttons.  Each gpio
	 * IRQ has an individual opaque ptr pointing to the corresponding
	 * button's dynamic lws_button_each_t, the ISR marks the button's
	 * each->isr_pending and schedules this bottom half.
	 *
	 * So now the bh timer has fired and something to do, we need to go
	 * through all the buttons that have isr_pending set and service their
	 * state.  Intermediate states should start / bump the refcount on the
	 * mon timer.  That's refcounted so it only runs when a button down.
	 */

	for (n = 0; n < bc->count_buttons; n++) {

		if (!each[n].isr_pending)
			continue;

		/*
		 * Hide what we're about to do from the delicate eyes of the
		 * IRQ controller...
		 */

		bc->gpio_ops->irq_mode(bc->button_map[n].gpio,
				       LWSGGPIO_IRQ_NONE, NULL, NULL);

		each[n].isr_pending = 0;

		/*
		 * Force the network around the switch to the
		 * active level briefly
		 */

		bc->gpio_ops->set(bc->button_map[n].gpio,
				  !!(bc->active_state_bitmap & (1 << n)));
		bc->gpio_ops->mode(bc->button_map[n].gpio, LWSGGPIO_FL_WRITE);

		if (each[n].state == LBCS_IDLE) {
			/*
			 * If this is the first sign something happening on this
			 * button, make sure the monitor timer is running to
			 * classify it over time
			 */

			each[n].state = LBCS_MIN_DOWN_QUALIFY;
			each[n].mon_timer_comp = bcs->mon_timer_count;

			if (!bcs->mon_refcount++) {
#if defined(LWS_PLAT_TIMER_START)
				// lwsl_notice("%s: starting mon timer\n", __func__);
				LWS_PLAT_TIMER_START(bcs->timer_mon);
#endif
			}
		}

		/*
		 * Just for a us or two inbetween here, we're driving it to the
		 * level we were informed by the interrupt it had enetered, to
		 * force to charge on the actual and parasitic network around
		 * the switch to a deterministic-ish state.
		 *
		 * If the switch remains in that state, well, it makes no
		 * difference; if it was a pre-contact and the charge on the
		 * network was left indeterminate, this will dispose it to act
		 * consistently in the short term until the pullup / pulldown
		 * has time to act on it or the switch comes and forces the
		 * network charge state itself.
		 */
		bc->gpio_ops->mode(bc->button_map[n].gpio, LWSGGPIO_FL_READ);

		/*
		 * We could do a better job manipulating the irq mode according
		 * to the switch state.  But if an interrupt comes and we have
		 * done that, we can't tell if it's from before or after the
		 * mode change... ie, we don't know what the interrupt was
		 * telling us.  We can't trust the gpio state if we read it now
		 * to be related to what the irq from some time before was
		 * trying to tell us.  So always set it back to the same mode
		 * and accept the limitation.
		 */

		bc->gpio_ops->irq_mode(bc->button_map[n].gpio,
				       bc->active_state_bitmap & (1 << n) ?
					   LWSGGPIO_IRQ_RISING :
					   LWSGGPIO_IRQ_FALLING,
					      lws_button_irq_cb_t, &each[n]);
	}
}
#endif

#if defined(LWS_PLAT_TIMER_CB)
static LWS_PLAT_TIMER_CB(lws_button_mon, th)
{
	lws_button_state_t *bcs = LWS_PLAT_TIMER_CB_GET_OPAQUE(th);
	lws_button_each_t *each = (lws_button_each_t *)&bcs[1];
	const lws_button_controller_t *bc = bcs->controller;
	const lws_button_regime_t *regime;
	const char *event_name;
	int comp_age_ms;
	char active;
	size_t n;

	bcs->mon_timer_count++;

	for (n = 0; n < bc->count_buttons; n++) {

		if (each[n].state == LBCS_IDLE)
			continue;

		if (bc->button_map[n].regime)
			regime = bc->button_map[n].regime;
		else
			regime = &default_regime;

		comp_age_ms = (bcs->mon_timer_count - each[n].mon_timer_comp) *
				LWS_BUTTON_MON_TIMER_MS;

		active = bc->gpio_ops->read(bc->button_map[n].gpio) ^
			       (!(bc->active_state_bitmap & (1 << n)));

		// lwsl_notice("%d\n", each[n].state);

		switch (each[n].state) {
		case LBCS_MIN_DOWN_QUALIFY:
			/*
			 * We're trying to figure out if the initial down event
			 * is a glitch, or if it meets the criteria for being
			 * treated as the definitive start of some kind of click
			 * action.  To get past this, he has to be solidly down
			 * for the time mentioned in the applied regime (at
			 * least when we sample it).
			 *
			 * Significant bounce at the start will abort this try,
			 * but if it's really down there will be a subsequent
			 * solid down period... it will simply restart this flow
			 * from a new interrupt and pass the filter then.
			 *
			 * The "brief drive on edge" strategy considerably
			 * reduces inconsistencies here.  But physical bounce
			 * will continue to be observed.
			 */

			if (!active) {
				/* We ignore stuff for a bit after discard */
				each[n].mon_timer_comp = bcs->mon_timer_count;
				each[n].state = LBCS_UP_SETTLE2;
				continue;
			}

			if (comp_age_ms >= regime->ms_min_down) {

				/* We made it through the initial regime filter,
				 * the next step is wait and see if this down
				 * event evolves into a single/double click or
				 * we can call it as a long-click
				 */

				each[n].state = LBCS_ASSESS_DOWN_HOLD;
				break;
			}
			break;

		case LBCS_ASSESS_DOWN_HOLD:
			/*
			 * How long is he going to hold it?  If he holds it
			 * past the long-click threshold, we can call it as a
			 * long-click and do the up processing afterwards.
			 */
			if (comp_age_ms >= regime->ms_min_down_longpress) {
				/* call it as a longclick */
				event_name = "longclick";
				each[n].state = LBCS_WAIT_UP;
				goto classify;
			}

			if (!active) {
				/*
				 * He didn't hold it past the long-click
				 * threshold... we could end up classifying it
				 * as either a click or a double-click then.
				 *
				 * If double-clicks are not allowed to be
				 * classified, then we can already classify it
				 * as a single-click.
				 */
				if (!(regime->flags & LWSBTNRGMFLAG_CLASSIFY_DOUBLECLICK))
					goto classify_single;

				/*
				 * Just wait for the up settle time then start
				 * looking for a second down.
				 */
				each[n].mon_timer_comp = bcs->mon_timer_count;
				each[n].state = LBCS_UP_SETTLE1;
			}
			break;

		case LBCS_UP_SETTLE1:
			if (comp_age_ms > regime->ms_up_settle)
				/*
				 * Just block anything for the up settle time
				 */
				each[n].state = LBCS_WAIT_DOUBLECLICK;
			break;

		case LBCS_WAIT_DOUBLECLICK:
			if (active) {
				/*
				 * He has gone down again inside the regime's
				 * doubleclick grace period... he's going down
				 * the double-click path
				 */
				each[n].mon_timer_comp = bcs->mon_timer_count;
				each[n].state = LBCS_MIN_DOWN_QUALIFY2;
				break;
			}

			if (comp_age_ms >= regime->ms_doubleclick_grace) {
				/*
				 * The grace period expired, the second click
				 * was either not forthcoming at all, or coming
				 * quick enough to count: we classify it as a
				 * single-click
				 */

				goto classify_single;
			}
			break;

		case LBCS_MIN_DOWN_QUALIFY2:
			if (!active) {
classify_single:
				/*
				 * He went up again too quickly, classify it
				 * as a single-click.  It could be bounce in
				 * which case you might want to increase
				 * the ms_up_settle in the regime
				 */
				event_name = "click";
				each[n].mon_timer_comp = bcs->mon_timer_count;
				each[n].state = LBCS_UP_SETTLE2;
				goto classify;
			}

			if (comp_age_ms >= regime->ms_min_down) {
				/*
				 * It's a double-click
				 */
				event_name = "doubleclick";
				each[n].state = LBCS_WAIT_UP;
				goto classify;
			}
			break;

		case LBCS_WAIT_UP:
			if (!active) {
				each[n].mon_timer_comp = bcs->mon_timer_count;
				each[n].state = LBCS_UP_SETTLE2;
			}
			break;

		case LBCS_UP_SETTLE2:
			if (comp_age_ms < regime->ms_up_settle)
				break;

			each[n].state = LBCS_IDLE;
			if (!(--bcs->mon_refcount)) {
#if defined(LWS_PLAT_TIMER_STOP)
				LWS_PLAT_TIMER_STOP(bcs->timer_mon);
#endif
			}
			break;
		}

		continue;

classify:
		lws_smd_msg_printf(bcs->ctx, LWSSMDCL_INTERACTION,
		   "{\"btn\":\"%s/%s\", \"s\":\"%s\"}",
		   bc->smd_bc_name,
		   bc->button_map[n].smd_interaction_name,
		   event_name);
	}
}
#endif

struct lws_button_state *
lws_button_controller_create(struct lws_context *ctx,
			     const lws_button_controller_t *controller)
{
	lws_button_state_t *bcs = lws_zalloc(sizeof(lws_button_state_t) +
			(controller->count_buttons * sizeof(lws_button_each_t)),
			__func__);
	lws_button_each_t *each = (lws_button_each_t *)&bcs[1];
	size_t n;

	if (!bcs)
		return NULL;

	bcs->controller = controller;
	bcs->ctx = ctx;

	for (n = 0; n < controller->count_buttons; n++)
		each[n].bcs = bcs;

#if defined(LWS_PLAT_TIMER_CREATE)
	/* this only runs inbetween a gpio ISR and the bottom half */
	bcs->timer = LWS_PLAT_TIMER_CREATE("bcst",
			1, 0, bcs, (TimerCallbackFunction_t)lws_button_bh);
	if (!bcs->timer)
		return NULL;
	/* this only runs when a button activity is being classified */
	bcs->timer_mon = LWS_PLAT_TIMER_CREATE("bcmon", LWS_BUTTON_MON_TIMER_MS, 1, bcs,
			(TimerCallbackFunction_t)lws_button_mon);
	if (!bcs->timer_mon)
		return NULL;
#endif

	return bcs;
}

void
lws_button_controller_destroy(struct lws_button_state *bcs)
{
	/* disable them all */
	lws_button_enable(bcs, 0, 0);

#if defined(LWS_PLAT_TIMER_DELETE)
	LWS_PLAT_TIMER_DELETE(&bcs->timer);
	LWS_PLAT_TIMER_DELETE(&bcs->timer_mon);
#endif

	lws_free(bcs);
}

lws_button_idx_t
lws_button_get_bit(struct lws_button_state *bcs, const char *name)
{
	const lws_button_controller_t *bc = bcs->controller;
	int n;

	for (n = 0; n < bc->count_buttons; n++)
		if (!strcmp(name, bc->button_map[n].smd_interaction_name))
			return 1 << n;

	return 0; /* not found */
}

void
lws_button_enable(lws_button_state_t *bcs,
		  lws_button_idx_t _reset, lws_button_idx_t _set)
{
	lws_button_idx_t u = (bcs->enable_bitmap & (~_reset)) | _set;
	const lws_button_controller_t *bc = bcs->controller;
#if defined(LWS_PLAT_TIMER_START)
	lws_button_each_t *each = (lws_button_each_t *)&bcs[1];
#endif
	int n;

	for (n = 0; n < bcs->controller->count_buttons; n++) {
		if (!(bcs->enable_bitmap & (1 << n)) && (u & (1 << n))) {
			/* set as input with pullup or pulldown appropriately */
			bc->gpio_ops->mode(bc->button_map[n].gpio,
				LWSGGPIO_FL_READ |
				((bc->active_state_bitmap & (1 << n)) ?
				LWSGGPIO_FL_PULLDOWN : LWSGGPIO_FL_PULLUP));
#if defined(LWS_PLAT_TIMER_START)
			/*
			 * This one is becoming enabled... the opaque for the
			 * ISR is the indvidual lws_button_each_t, they all
			 * point to the same ISR
			 */
			bc->gpio_ops->irq_mode(bc->button_map[n].gpio,
					bc->active_state_bitmap & (1 << n) ?
						LWSGGPIO_IRQ_RISING :
							LWSGGPIO_IRQ_FALLING,
						lws_button_irq_cb_t, &each[n]);
#endif
		}
		if ((bcs->enable_bitmap & (1 << n)) && !(u & (1 << n)))
			/* this one is becoming disabled */
			bc->gpio_ops->irq_mode(bc->button_map[n].gpio,
						LWSGGPIO_IRQ_NONE, NULL, NULL);
	}

	bcs->enable_bitmap = u;
}
