/*
 * devices for ESP32 Heltec WB32
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
 * Hook up bitbang i2c, display driver and display
 */

static void
esp32_i2c_delay(void)
{
	volatile int n = 0;

	while (n < 20)
		n++;
}

static const lws_bb_i2c_t li2c = {
	.bb_ops				= lws_bb_i2c_ops,
	.scl				= GPIO_NUM_15,
	.sda				= GPIO_NUM_4,
	.gpio				= &lws_gpio_plat,
	.delay				= esp32_i2c_delay
};

/*
 * Button controller
 */

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

/*
 * pwm controller
 */

static const lws_pwm_map_t pwm_map[] = {
	{ .gpio = GPIO_NUM_25, .index = 0, .active_level = 1 }
};

static const lws_pwm_ops_t pwm_ops = {
	lws_pwm_plat_ops,
	.pwm_map			= &pwm_map[0],
	.count_pwm_map			= LWS_ARRAY_SIZE(pwm_map)
};

static const lws_display_ssd1306_t disp = {
	.disp = {
		lws_display_ssd1306_ops,
		.w			= 128,
		.h			= 64
	},
	.i2c				= (lws_i2c_ops_t *)&li2c,
	.gpio				= &lws_gpio_plat,
	.reset_gpio			= GPIO_NUM_16,
	.i2c7_address			= SSD1306_I2C7_ADS1
};

/*
 * led controller
 */

static const lws_led_gpio_map_t lgm[] = {
	{
		.name			= "alert",
		.gpio			= GPIO_NUM_25,
		.pwm_ops		= &pwm_ops, /* managed by pwm */
		.active_level		= 1,
	},
};

static const lws_led_gpio_controller_t lgc = {
	.led_ops			= lws_led_gpio_ops,
	.gpio_ops			= &lws_gpio_plat,
	.led_map			= &lgm[0],
	.count_leds			= LWS_ARRAY_SIZE(lgm)
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

	lls = lgc.led_ops.create(&lgc.led_ops);
	if (!lls) {
		lwsl_err("%s: could not create led\n", __func__);
		return 1;
	}

	/* pwm init must go after the led controller init */

	pwm_ops.init(&pwm_ops);

	bcs = lws_button_controller_create(ctx, &bc);
	if (!bcs) {
		lwsl_err("%s: could not create buttons\n", __func__);
		return 1;
	}

	/*
	 * Show the lws logo on the display
	 */

	lws_display_state_init(&lds, ctx, 10000, 20000, lls, &disp.disp);

	lws_button_enable(bcs, 0, lws_button_get_bit(bcs, "user"));
	lws_led_transition(lls, "alert", &lws_pwmseq_static_off,
					 &lws_pwmseq_static_on);

	return 0;
}
