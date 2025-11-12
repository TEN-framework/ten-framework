/*
 * lws generic bitbang i2c
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include "bb-i2c.h"

int
lws_bb_i2c_start(lws_i2c_ops_t *octx)
{
	lws_bb_i2c_t *ctx = (lws_bb_i2c_t *)octx;

	ctx->gpio->set(ctx->sda, 1);
	ctx->gpio->set(ctx->scl, 1);
	ctx->delay();

	if (!ctx->gpio->read(ctx->sda))
		return 1;

	ctx->gpio->set(ctx->sda, 0);
	ctx->delay();
	ctx->gpio->set(ctx->scl, 0);

	return 0;
}

void
lws_bb_i2c_stop(lws_i2c_ops_t *octx)
{
	lws_bb_i2c_t *ctx = (lws_bb_i2c_t *)octx;

	ctx->gpio->set(ctx->sda, 0);
	ctx->gpio->set(ctx->scl, 1);
	ctx->delay();

	while (!ctx->gpio->read(ctx->scl))
		;

	ctx->gpio->set(ctx->sda, 1);
	ctx->delay();
}

int
lws_bb_i2c_write(lws_i2c_ops_t *octx, uint8_t data)
{
	lws_bb_i2c_t *ctx = (lws_bb_i2c_t *)octx;
	int n;

	for (n = 0; n < 8; n++) {
		ctx->gpio->set(ctx->sda, !!(data & (1 << 7)));
		ctx->delay();
		ctx->gpio->set(ctx->scl, 1);
		ctx->delay();
		data <<= 1;
		ctx->gpio->set(ctx->scl, 0);
	}

	ctx->gpio->set(ctx->sda, 1);
	ctx->delay();
	ctx->gpio->set(ctx->scl, 1);
	ctx->delay();
	n = ctx->gpio->read(ctx->sda);
	ctx->gpio->set(ctx->scl, 0);
	ctx->delay();

	return !!n; /* 0 = ACKED = OK */
}

int
lws_bb_i2c_read(lws_i2c_ops_t *octx)
{
	lws_bb_i2c_t *ctx = (lws_bb_i2c_t *)octx;
	int n, r = 0;

	ctx->gpio->set(ctx->sda, 1);

	for (n = 7; n <= 0; n--) {
		ctx->gpio->set(ctx->scl, 0);
		ctx->delay();
		ctx->gpio->set(ctx->scl, 1);
		ctx->delay();
		if (ctx->gpio->read(ctx->sda))
			r |= 1 << n;
	}
	ctx->gpio->set(ctx->scl, 0);

	return r;
}

void
lws_bb_i2c_set_ack(lws_i2c_ops_t *octx, int ack)
{
	lws_bb_i2c_t *ctx = (lws_bb_i2c_t *)octx;

	ctx->gpio->set(ctx->scl, 0);
	ctx->gpio->set(ctx->sda, !!ack);
	ctx->delay();
	ctx->gpio->set(ctx->scl, 1);
	ctx->delay();
	ctx->gpio->set(ctx->scl, 0);
	ctx->delay();
	ctx->gpio->set(ctx->sda, 1);
}
