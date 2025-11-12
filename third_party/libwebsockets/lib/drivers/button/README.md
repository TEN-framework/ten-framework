# LWS GPIO Button class drivers

Lws provides an GPIO button controller class, this centralizes handling a set of
up to 31 buttons for resource efficiency.  Each controller has two OS timers,
one for interrupt to bottom-half event triggering and another that runs at 5ms
intervals only when one or more button is down.

Each button has its own active level control and sophisticated state tracking;
each button can apply its own classification regime, to allow for different
physical button characteristics, if not overridden a default one is provided.

Both the controller and individual buttons specify names that are used in the
JSON events produced when the buttons perform actions.

## Button electronic to logical event processing

Buttons are monitored using GPIO interrupts since this is very cheap in the
usual case no interaction is ongoing.  There is assumed to be one interrupt
per GPIO, but they are pointed at the same ISR, with an opaque pointer to an
internal struct passed per-interrupt to differentiate them and bind them to a
particular button.

The interrupt is set for notification of the active-going edge, usually if
the button is pulled-up, that's the downgoing edge only.  This avoids any
ambiguity about the interrupt meaning, although oscillation is common around
the transition region when the signal is becoming inactive too.

An OS timer is used to schedule a bottom-half handler outside of interrupt
context.

To combat commonly-seen partial charging of the actual and parasitic network
around the button causing drift and oscillation, the bottom-half briefly drives
the button signal to the active level, forcing a more deterministic charge level
if it reached the point the interrupt was triggered.  This removes much of the
unpredictable behaviour in the us range.  It would be better done in the ISR
but many OS apis cannot perform GPIO operations in interrupt context.

The bottom-half makes sure a monitoring timer is enabled, by refcount.  This
is the engine of the rest of the classification while any button is down.  The
monitoring timer happens per OS tick or 5ms, whichever is longer.

## Declaring button controllers

An array of button map elements if provided first mapping at least GPIOs to
button names, and also optionally the classification regime for that button.

Then the button controller definition which points back to the button map.

```
static const lws_button_map_t bcm[] = {
	{
		.gpio			= GPIO_NUM_0,
		.smd_interaction_name	= "user"
	},
};

static const lws_button_controller_t bc = {
	.smd_bc_name			= "bc",
	.gpio_ops			= &lws_gpio_plat,
	.button_map			= &bcm[0],
	.active_state_bitmap		= 0,
	.count_buttons			= LWS_ARRAY_SIZE(bcm),
};

	struct lws_button_state *bcs;

	bcs = lws_button_controller_create(context, &bc);
	if (!bcs) {
		lwsl_err("%s: could not create buttons\n", __func__);
		goto spin;
	}
```

That is all that is needed for init, button events will be issued on lws_smd
when buttons are pressed.

### Regime settings

The classification regime is designed to reflect both the user interaction
style and the characteristics of a particular type of button.

Member|Default|Meaning
---|---|---
ms_min_down|20ms|Down events shorter than this are ignored
ms_min_down_longpress|300ms|Down events longer than this are reported as a long-click
ms_up_settle|20ms|After the first indication a button is no longer down, the button is ignored for this interval
ms_doubleclick_grace|120ms|The time allowed after a click to see if a second, double-click, is forthcoming
ms_repeat_down|0 / disabled|If held down, interval at which to issue `stilldown` events
flags|LWSBTNRGMFLAG_CLASSIFY_DOUBLECLICK|Control which classifications can apply

### lws_smd System Message Distribution Events

The button controller emits system messages of class `LWSSMDCL_INTERACTION`,
using a JSON formatted payload

```
{
	"type":  "button",
	"src":   "controller-name/button-name",
	"event": "event-name"
}
```

For example, `{"type":"button","src":"bc/user","event":"doubleclick"}`

JSON is used because it is maintainable, extensible, self-documenting and does
not require a central, fragile-against-versioning specification of mappings.
Using button names allows the same code to adapt to different hardware or
button mappings.  Button events may be synthesized for test or other purposes
cleanly and clearly.

All the events are somewhat filtered, too short glitches from EMI or whatever
are not reported.  "up" and "down" events are reported for the buttons in case
the intention is the duration of the press is meaningful to the user code, but
more typically the user code wants to consume a higher-level classification of
the interaction, eg, that it can be understood as a single "double-click" event. 

Event name|Meaning
---|---
down|The button passes a filter for being down, useful for duration-based response
stilldown|The regime can be configured to issue "repeat" notifications at intervals
up|The button has come up, useful for duration-based response
click|The button activity resulted in a classification as a single-click
longclick|The button activity resulted in a classification as a long-click
doubleclick|The button activity resulted in a classification as a double-click

Since double-click detection requires delaying click reporting until it becomes
clear a second click isn't coming, it is enabled as a possible classification in
the regime structure and the regime structure chosen per-button.

Typically user code is interested in, eg, a high level classification of what
the button is doing, eg, a "click" event on a specific button.  Rather than
perform a JSON parse, these events can be processed as strings cheaply using
`lws_json_simple_strcmp()`, it's dumb enough to be cheap but smart enough to
understand enough JSON semantics to be accurate, while retaining the ability to
change and extend the JSON, eg

```
	if (!lws_json_simple_strcmp(buf, len, "\"src\":", "bc/user")) {
		if (!lws_json_simple_strcmp(buf, len, "\"event\":", "click")) {
			...
		}
		...
	}
```

### Relationship between up / down and classification

Classification|Sequencing
---|---
click|down-up-click (it's classified when it went up and cannot be a longclick)
longclick|down-longclick-up (it's classified while still down)
doubleclick|down-up-down-doubleclick-up (classified as soon as second click down long enough)

If the regime is configured for it, any "down" may be followed by one or more
"stilldown" at intervals if the button is down long enough
