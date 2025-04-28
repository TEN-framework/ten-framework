/*
 * lws-minimal-esp32
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <stdint.h>
#include <stddef.h>
#include "i2c.h"
#include "gpio-esp32.h"

typedef struct lws_bb_i2c {
	lws_i2c_ops_t		bb_ops; /* init to lws_bb_i2c_ops */

	/* implementation-specific members */

	_lws_plat_gpio_t	scl;
	_lws_plat_gpio_t	sda;

	const lws_gpio_ops_t	*gpio;
	void (*delay)(void);
} lws_bb_i2c_t;

#define lws_bb_i2c_ops \
	{ \
		.start = lws_bb_i2c_start, \
		.stop = lws_bb_i2c_stop, \
		.write = lws_bb_i2c_write, \
		.read = lws_bb_i2c_read, \
		.set_ack = lws_bb_i2c_set_ack, \
	}

int
lws_bb_i2c_start(lws_i2c_ops_t *octx);

void
lws_bb_i2c_stop(lws_i2c_ops_t *octx);

int
lws_bb_i2c_write(lws_i2c_ops_t *octx, uint8_t data);

int
lws_bb_i2c_read(lws_i2c_ops_t *octx);

void
lws_bb_i2c_set_ack(lws_i2c_ops_t *octx, int ack);


