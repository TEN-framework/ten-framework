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

/*
 * 64 entry interpolated CIE correction
 * https://en.wikipedia.org/wiki/Lightness
 */

uint16_t cie[] = {
	0, 113, 227, 340, 454, 568, 688, 824, 976, 1146,
	1335, 1543, 1772, 2023, 2296, 2592, 2914, 3260, 3633, 4034,
	4463, 4921, 5409, 5929, 6482, 7067, 7687, 8341, 9032, 9761,
	10527, 11332, 12178, 13064, 13993, 14964, 15980, 17040, 18146, 19299,
	20500, 21750, 23049, 24400, 25802, 27256, 28765, 30328, 31946, 33622,
	35354, 37146, 38996, 40908, 42881, 44916, 47014, 49177, 51406, 53700,
	56062, 58492, 60992, 63561,
	65535 /* for interpolation */
};

/*
 * This is the default intensity correction function, it can be overridden
 * per-led to eg, normalize intensity of different leds
 */

static lws_led_intensity_t
cie_antilog(lws_led_intensity_t lin)
{
        return (cie[lin >> 10]       * (0x3ff - (lin & 0x3ff)) +
        	cie[(lin >> 10) + 1] * (lin & 0x3ff)) / 0x3ff;
}

static void
lws_seq_advance(lws_led_state_t *lcs, lws_led_state_ch_t *ch)
{
	if (!ch->seq)
		return;

	if (ch->phase_budget != LWS_SEQ_LEDPHASE_TOTAL_ENDLESS &&
	    (ch->phase_budget < ch->step || !ch->phase_budget)) {

		/* we are done */

		ch->seq = NULL;
		if (!(--lcs->timer_refcount)) {
#if defined(LWS_PLAT_TIMER_STOP)
			LWS_PLAT_TIMER_STOP(lcs->timer);
#endif
		}

		return;
	}

	ch->ph += ch->step;
	if (ch->phase_budget != LWS_SEQ_LEDPHASE_TOTAL_ENDLESS)
		ch->phase_budget -= ch->step;
}

static lws_led_intensity_t
lws_seq_sample(const lws_led_gpio_map_t *map, lws_led_state_chs_t *chs)
{
	unsigned int i;

	if (chs->seqs[LLSI_CURR].seq)
		chs->seqs[LLSI_CURR].last = chs->seqs[LLSI_CURR].seq->
						func(chs->seqs[LLSI_CURR].ph);

	if (chs->seqs[LLSI_TRANS].seq) {
		/*
		 * If a transition is ongoing, we need to use the transition
		 * intensity as the mixing factor between the still-live current
		 * and newly-live next sequences
		 */
		chs->seqs[LLSI_TRANS].last = chs->seqs[LLSI_TRANS].seq->
						func(chs->seqs[LLSI_TRANS].ph);

		if (chs->seqs[LLSI_NEXT].seq)
			chs->seqs[LLSI_NEXT].last = chs->seqs[LLSI_NEXT].seq->
						  func(chs->seqs[LLSI_NEXT].ph);

		i = (lws_led_intensity_t)(((
				(unsigned int)chs->seqs[LLSI_CURR].last *
				(65535 - chs->seqs[LLSI_TRANS].last) >> 16) +
			(((unsigned int)chs->seqs[LLSI_NEXT].last *
			 (unsigned int)chs->seqs[LLSI_TRANS].last) >> 16)));
	} else
		i = chs->seqs[LLSI_CURR].last;

	return map->intensity_correction ? map->intensity_correction(i) :
					   cie_antilog((lws_led_intensity_t)i);
}

void
lws_seq_timer_handle(lws_led_state_t *lcs)
{
	lws_led_gpio_controller_t *lgc = lcs->controller;
	lws_led_state_chs_t *chs = (lws_led_state_chs_t *)&lcs[1];
	const lws_led_gpio_map_t *map = &lgc->led_map[0];
	unsigned int n;

	for (n = 0; n < lgc->count_leds; n++) {

		lgc->led_ops.intensity(&lgc->led_ops, map->name,
				       lws_seq_sample(map, chs));

		lws_seq_advance(lcs, &chs->seqs[LLSI_CURR]);

		if (chs->seqs[LLSI_TRANS].seq) {
			lws_seq_advance(lcs, &chs->seqs[LLSI_NEXT]);
			lws_seq_advance(lcs, &chs->seqs[LLSI_TRANS]);

			/*
			 * When we finished the transition, we can make the
			 * "next" sequence the current sequence and no need for
			 * a "next" or a transition any more.
			 */

			if (!chs->seqs[LLSI_TRANS].seq) {
				chs->seqs[LLSI_CURR] = chs->seqs[LLSI_NEXT];
				chs->seqs[LLSI_NEXT].seq = NULL;
			}
		}

		map++;
		chs++;
	}
}

static int
lws_led_set_chs_seq(struct lws_led_state *lcs, lws_led_state_ch_t *dest,
		    const lws_led_sequence_def_t *def)
{
	int steps;

	dest->seq = def;
	dest->ph = def->ledphase_offset;
	dest->phase_budget = def->ledphase_total;

	/*
	 * We need to compute the incremental phase angle step to cover the
	 * total number of phases in the indicated ms, incrementing at the
	 * timer rate of LWS_LED_SEQUENCER_UPDATE_RATE_HZ.  Eg,
	 *
	 * 65536 phase steps (one cycle) in 2000ms at 30Hz timer rate means we
	 * will update 2000ms / 33ms = 60 times, so we must step at at
	 * 65536 / 60 = 1092 phase angle resolution
	 */

	steps = def->ms / LWS_LED_SEQUENCER_UPDATE_INTERVAL_MS;
	dest->step = (def->ledphase_total != LWS_SEQ_LEDPHASE_TOTAL_ENDLESS ?
		def->ledphase_total : LWS_LED_FUNC_PHASE) / (steps ? steps : 1);

	if (!lcs->timer_refcount++) {
#if defined(LWS_PLAT_TIMER_START)
		LWS_PLAT_TIMER_START(lcs->timer);
#endif
	}

	return steps;
}

int
lws_led_transition(struct lws_led_state *lcs, const char *name,
		   const lws_led_sequence_def_t *next,
		   const lws_led_sequence_def_t *trans)
{
	lws_led_state_chs_t *chs = (lws_led_state_chs_t *)&lcs[1];
	int index = lws_led_gpio_lookup(&lcs->controller->led_ops, name);

	if (index < 0)
		return 1;

	lws_led_set_chs_seq(lcs, &chs[index].seqs[LLSI_TRANS], trans);
	lws_led_set_chs_seq(lcs, &chs[index].seqs[LLSI_NEXT], next);

	return 0;
}
