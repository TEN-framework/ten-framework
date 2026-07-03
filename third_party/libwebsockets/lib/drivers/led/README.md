# lws_led gpio and pwm class drivers

Lws provides an abstract led controller class that can bind an array of LEDs
to gpio and pwm controllers, and automatically handled pwm sequencers.

Lumience intensity is corrected for IEC curves to match perceptual intensity,
and the correction can be overridden per led for curve adaptation matching.

Intensity is normalized to a 16-bit scale, when controlled by a GPIO b15 is
significant and the rest ignored.  When controlled by PWM, as many bits from
b15 down are significant as the PWM arrangements can represent.

The PWM sequencers use arbitrary function generation callbacks on a normalized
16-bit phase space, they can choose how much to interpolate and how much to put
in a table, a 64-sample, 16-bit sine function is provided along with 16-bit
linear sawtooth.

Changing the sequencer is subject to a third transition function sequencer, this
can for example mix the transition linearly over, eg, 500ms so the leds look
very smooth.

## Defining an led controller

An array of inidividual LED information is provided first, and referenced by
the LED controller definintion.  Leds are named so code does not introduce
dependencies on specific implementations.

```
static const lws_led_gpio_map_t lgm[] = {
	{
		.name			= "alert",
		.gpio			= GPIO_NUM_25,
		.pwm_ops		= &pwm_ops,
		.active_level		= 1,
	},
};

static const lws_led_gpio_controller_t lgc = {
	.led_ops			= lws_led_gpio_ops,
	.gpio_ops			= &lws_gpio_plat,
	.led_map			= &lgm[0],
	.count_leds			= LWS_ARRAY_SIZE(lgm)
};

	struct lws_led_state *lls;

	lls = lgc.led_ops.create(&lgc.led_ops);
	if (!lls) {
		lwsl_err("%s: could not create led\n", __func__);
		goto spin;
	}

```

For GPIO control, the active level of the GPIO to light the LED may be set.

Each LED may bind to a pwm controller, in which case setting the intensity
programs the pwm controller corresponding to the GPIO.

## Setting the intensity directly

```
	lgc.led_ops.intensity(&lgc.led_ops, "alert", 0);
```

## Defining Sequencer

Some common sequencers are provided out of the box, you can also define your
own arbitrary ones.

The main point is sequencers have a function that returns an intensity for each
of 65536 phase steps in its cycle.  For example, this is the linear function
that is included

```
lws_led_intensity_t
lws_led_func_linear(lws_led_seq_phase_t n)
{
	return (lws_led_intensity_t)n;
}
```

It simply returns an intensity between 0 - 65535 matching the phase angle of
0 - 65535 that it was given, so it's a sawtooth ramp.

An interpolated sine function is also provided that returns an intensity
between 0 - 65535 reflecting one cycle of sine wave for the phase angle of 0 -
65535.

These functions are packaged into sequencer structures like this

```
const lws_led_sequence_def_t lws_pwmseq_sine_endless_fast = {
	.func			= lws_led_func_sine,
	.ledphase_offset	= 0, /* already at 0 amp at 0 phase */
	.ledphase_total		= LWS_SEQ_LEDPHASE_TOTAL_ENDLESS,
	.ms			= 750
};
```

This "endless" sequencer cycles through the sine function at 750ms per cycle.
Non-endless sequencers have a specific start and end in the phase space, eg

```
const lws_led_sequence_def_t lws_pwmseq_sine_up = {
	.func			= lws_led_func_sine,
	.ledphase_offset	= 0, /* already at 0 amp at 0 phase */
	.ledphase_total		= LWS_LED_FUNC_PHASE / 2, /* 180 degree ./^ */
	.ms			= 300
};
```

... this one traverses 180 degrees of the sine wave starting from 0 and ending
at full intensity, over 300ms.

A commonly-used, provided one is like this, as used in the next section

```
const lws_led_sequence_def_t lws_pwmseq_linear_wipe = {
	.func			= lws_led_func_linear,
	.ledphase_offset	= 0,
	.ledphase_total		= LWS_LED_FUNC_PHASE - 1,
	.ms			= 300
};
```

## Setting the intensity using sequencer transitions

The main api for high level sequenced control is

```
int
lws_led_transition(struct lws_led_state *lcs, const char *name,
		   const lws_led_sequence_def_t *next,
		   const lws_led_sequence_def_t *trans);
```

This fades from the current sequence to a new sequence, using `trans` sequencer
intensity as the mix factor.  `trans` is typically `lws_pwmseq_linear_wipe`,
fading between the current and new linearly over 300ms.  At the end of the
`trans` sequence, the new sequence simply replaces the current one and the
transition is completed.

Sequencers use a single 30Hz OS timer while any sequence is active.

exported sequencer symbol|description
---|---
lws_pwmseq_sine_endless_slow|continuous 100% sine, 1.5s cycle 
lws_pwmseq_sine_endless_fast|continuous 100% sine, 0.75s cycle 
lws_pwmseq_linear_wipe|single 0 - 100% ramp over 0.3s
lws_pwmseq_sine_up|single 0 - 100% using sine curve over 0.3s
lws_pwmseq_sine_down|single 100% - 0 using sine curve over 0.3s
lws_pwmseq_static_on|100% static
lws_pwmseq_static_half|50% static
lws_pwmseq_static_off|0% static
