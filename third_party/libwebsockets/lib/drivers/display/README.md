# lws_display

lws provides a generic "display" object that is independent of the connection
to the display, i2c and spi implementations are provided.

Its purpose is to provide basic blit, backlight binding to lws_pwm, backlight /
power management and display info like pixels wide and high in a generic way.

The generic display object `lws_display_t` can be included at the top of a
specific display implementation object, eg, binding it to additional members
to define the actual IO operations to be used, eg, i2c or spi.

When the display is instantiated, it allocates an additional structure on heap
that contains dynamic information about display state, `lws_display_state_t`.

## Power state machine

lws_display objects have convenient power state management using a single lws
sul event loop timer that is managed automatically.

State|Meaning
---|---
OFF|The display is in sleep and not showing anything
BECOMING_ACTIVE|The display was asked to come out of sleep and is waiting for .latency_wake_ms befor proceeding to ACTIVE.  The backlight if any is off.  After the delay, the backlight is sequenced up to `.bl_active` using `.bl_transition` sequencer
ACTIVE|The backlight is ON and the dim timer is running
AUTODIMMED|The dim timer was not told the display was active for `.autodim_ms`, we are at `.bl_dim` brightness.  After `.off_ms` we will transition to OFF 

The lws_pwm sequencers are used to provide customizable, smooth transitions for
the backlight, which may be nonlinear.

## Active notification

Calling `lws_display_state_active(&lds)` on eg, user interaction causes the
display state to transition to ACTIVE smoothly, taking care of waking the display
and waiting out a display-specific wake period, and sequencing the backlight
transition to active level as specified in the display structure.
