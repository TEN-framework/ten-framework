/*
 * devices for ESP WROVER KIT
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#define LWIP_PROVIDE_ERRNO 1
#define _ESP_PLATFORM_ERRNO_H_

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <driver/gpio.h>

#include <libwebsockets.h>

struct lws_led_state *lls;
lws_display_state_t lds;
struct lws_button_state *bcs;
lws_netdev_instance_wifi_t *wnd;

/*
 * Button controller
 *
 * On the WROVER KIT, it's a bit overloaded... the two buttons are reset and
 * gpio0, gpio is also used for one of the RGB LEDs channels control so it's not
 * really usable as a general user button.
 *
 * Instead we use GPIO 14 (available on J1) for a button with the other side
 * of the switch connected to 0V.
 */

static const lws_button_map_t bcm[] = {
	{
		.gpio			= GPIO_NUM_14,
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

/*
 * pwm controller
 */

static const lws_pwm_map_t pwm_map[] = {
	{ .gpio = GPIO_NUM_2, .index = 0, .active_level = 1 },
	{ .gpio = GPIO_NUM_0, .index = 1, .active_level = 1 },
	{ .gpio = GPIO_NUM_4, .index = 2, .active_level = 1 },
	{ .gpio = GPIO_NUM_5, .index = 3, .active_level = 0 }
};

static const lws_pwm_ops_t pwm_ops = {
	lws_pwm_plat_ops,
	.pwm_map			= &pwm_map[0],
	.count_pwm_map			= LWS_ARRAY_SIZE(pwm_map)
};

/*
 * led controller
 */

static const lws_led_gpio_map_t lgm[] = {
	{
		.name			= "red",
		.gpio			= GPIO_NUM_2,
		.pwm_ops		= &pwm_ops, /* managed by pwm */
		.active_level		= 1,
	},
	{
		.name			= "green",
		.gpio			= GPIO_NUM_0,
		.pwm_ops		= &pwm_ops, /* managed by pwm */
		.active_level		= 1,
	},
	{
		.name			= "blue",
		.gpio			= GPIO_NUM_4,
		.pwm_ops		= &pwm_ops, /* managed by pwm */
		.active_level		= 1,
	},
	{
		.name			= "backlight",
		.gpio			= GPIO_NUM_5,
		.pwm_ops		= &pwm_ops, /* managed by pwm */
		.active_level		= 0,
		/*
		 * The wrover kit uses a 2 NPN in series to drive the backlight
		 * which means if the GPIO provides no current, the backlight is
		 * full-on.  This causes a white flash during boot... they mark
		 * the first stage with "Modify In ESP-WROVER-KIT!" on the
		 * schematics but on Kit v4.1, it's still like that.
		 */
	},
};

static const lws_led_gpio_controller_t lgc = {
	.led_ops			= lws_led_gpio_ops,
	.gpio_ops			= &lws_gpio_plat,
	.led_map			= &lgm[0],
	.count_leds			= LWS_ARRAY_SIZE(lgm)
};

/*
 * Bitbang SPI configuration for display
 */

static const lws_bb_spi_t lbspi = {
		.bb_ops = {
			lws_bb_spi_ops,
			.bus_mode = LWS_SPI_BUSMODE_CLK_IDLE_LOW_SAMP_RISING
		},
		.gpio		= &lws_gpio_plat,
		.clk		= GPIO_NUM_19,
		.ncs		= { GPIO_NUM_22 },
		.ncmd		= { GPIO_NUM_21 },
		.mosi		= GPIO_NUM_23,
		.miso		= GPIO_NUM_25,
		.flags		= LWSBBSPI_FLAG_USE_NCS0 |
				  LWSBBSPI_FLAG_USE_NCMD0
};

/*
 * SPI display
 */

static const lws_display_ili9341_t disp = {
	.disp = {
		lws_display_ili9341_ops,
		.bl_pwm_ops		= &pwm_ops,
		.bl_active		= &lws_pwmseq_static_on,
		.bl_dim			= &lws_pwmseq_static_half,
		.bl_transition		= &lws_pwmseq_linear_wipe,
		.bl_index		= 3,
		.w			= 320,
		.h			= 240,
		.latency_wake_ms	= 150,
	},
	.spi				= (lws_spi_ops_t *)&lbspi,
	.gpio				= &lws_gpio_plat,
	.reset_gpio			= GPIO_NUM_18,
	.spi_index			= 0
};

/*
 * Settings stored in platform nv
 */

static const lws_settings_ops_t sett = {
	lws_settings_ops_plat
};

/*
 * Wifi
 */

static const lws_netdev_ops_t wifi_ops = {
	lws_netdev_wifi_plat_ops
};

int
init_plat_devices(struct lws_context *ctx)
{
	lws_settings_instance_t *si;
	lws_netdevs_t *netdevs = lws_netdevs_from_ctx(ctx);

	si = lws_settings_init(&sett, (void *)"nvs");
	if (!si) {
		lwsl_err("%s: failed to create settings instance\n", __func__);
		return 1;
	}
	netdevs->si = si;

#if 0
	/*
	 * This is a temp hack to bootstrap the settings to contain the test
	 * AP ssid and passphrase for one time, so the settings can be stored
	 * while there's no UI atm
	 */
	{
		lws_wifi_creds_t creds;

		memset(&creds, 0, sizeof(creds));

		lws_strncpy(creds.ssid, "xxx", sizeof(creds.ssid));
		lws_strncpy(creds.passphrase, "yyy", sizeof(creds.passphrase));
		lws_dll2_add_tail(&creds.list, &netdevs->owner_creds);

		if (lws_netdev_credentials_settings_set(netdevs)) {
			lwsl_err("%s: failed to write bootstrap creds\n",
					__func__);
			return 1;
		}
	}
#endif

//	if (lws_netdev_instance_wifi_settings_get(si, "netdev.wl0", &niw, &ac)) {
//		lwsl_err("%s: unable to fetch wl0 settings\n", __func__);
//		return 1;
//	}

	/* create the wifi network device and configure it */

	wnd = (lws_netdev_instance_wifi_t *)
				wifi_ops.create(ctx, &wifi_ops, "wl0", NULL);
	if (!wnd) {
		lwsl_err("%s: failed to create wifi object\n", __func__);
		return 1;
	}

	wnd->flags |= LNDIW_MODE_STA;

	if (wifi_ops.configure(&wnd->inst, NULL)) {
		lwsl_err("%s: failed to configure wifi object\n", __func__);
		return 1;
	}

	wifi_ops.up(&wnd->inst);

	/* bring up the led controller */

	lls = lgc.led_ops.create(&lgc.led_ops);
	if (!lls) {
		lwsl_err("%s: could not create led\n", __func__);
		return 1;
	}

	/* pwm init must go after the led controller init */

	pwm_ops.init(&pwm_ops);

	/* ... and the button controller */

	bcs = lws_button_controller_create(ctx, &bc);
	if (!bcs) {
		lwsl_err("%s: could not create buttons\n", __func__);
		return 1;
	}

	lws_button_enable(bcs, 0, lws_button_get_bit(bcs, "user"));

	/* ... bring up spi bb and the display */

	lbspi.bb_ops.init(&lbspi.bb_ops);
	lws_display_state_init(&lds, ctx, 30000, 10000, lls, &disp.disp);

	/*
	 * Make the RGB LED do something using sequenced PWM... pressing the
	 * GPIO14 button with single-presses advances the blue channel between
	 * different sequences
	 */

	lws_led_transition(lls, "blue", &lws_pwmseq_sine_endless_fast,
					&lws_pwmseq_linear_wipe);
	lws_led_transition(lls, "green", &lws_pwmseq_sine_endless_slow,
					 &lws_pwmseq_linear_wipe);
	lws_led_transition(lls, "red", &lws_pwmseq_sine_endless_slow,
				       &lws_pwmseq_linear_wipe);

	return 0;
}
