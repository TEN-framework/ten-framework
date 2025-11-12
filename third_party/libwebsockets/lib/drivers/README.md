# lws meta-drivers

Although drivers in lws (enabled in cmake by `LWS_WITH_DRIVERS`) provide
actual drivers for some devices like I2C OLED controllers, their main job is
to conceal from user code the underlying OS APIs being used to interface
to the SoC hardware assets.

CMake already allows lws to be platform-agnostic for build, the plat adaptations
allow lws to be platform-agnostic within itself for runtime.  The lws
drivers intend to extend that agnosticism to user code.

Using this technique on supported OSes frees the user code from dependencies
on the underlying OS choice... for example, although ESP32 is very good, it
comes with a highly specific set of apis in esp-idf that mean your code is
locked in to esp-idf if you follow them.  Esp-idf uses freertos apis for things
like OS timers, again if you follow those you are locked into freertos, the
end result is your work is non-portable to other platforms and completely
dependent on esp.

LWS drivers provide a thin wrapper to eliminate the OS dependencies while
still taking advantage of the work, drivers and maintenance of the underlying
OS layer without duplicating them, but bringing the flexibility to retarget
your work to other scenarios... for example, there is a generic gpio object
subclassed for specific implementations, an i2c object which may be subclassed
to use OS drivers or bitbang using the generic gpio object, buttons on top of
generic gpio, led class that can use generic gpio or pwm interchangeably,
platform-specific gpio, i2c, pwm implementations that can be used at the generic
level are defined to use underlying OS native apis and drivers.

## Building on the next layer up

At these generic objects like buttons or led controllers, there is a stable
codebase used by multiple implementations and the intention is to provide
best-of-breed features there generically, like

 - sophisticated button press debounce and classification

 - high quality transitions and log-response compensation and mixing for led pwm

 - display dimming timers, blanking timers, generic interaction detection to unblank

which are automatically available on top of any implementation that is ported to
lws drivers.

