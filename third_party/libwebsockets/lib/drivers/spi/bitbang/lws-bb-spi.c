/*
 * SPI bitbang implementation using generic gpio
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
#include <libwebsockets.h>

int
lws_bb_spi_init(const lws_spi_ops_t *octx)
{
	lws_bb_spi_t *ctx = (lws_bb_spi_t *)octx;
	int n;

	for (n = 0; n < LWS_SPI_BB_MAX_CH; n++) {
		if (ctx->flags & (1 << n))
			ctx->gpio->mode(ctx->ncs[n], LWSGGPIO_FL_WRITE);
		if (ctx->flags & (1 << (n + 4)))
			ctx->gpio->mode(ctx->ncmd[n], LWSGGPIO_FL_WRITE);
	}

	ctx->gpio->mode(ctx->clk, LWSGGPIO_FL_WRITE |
				  ((octx->bus_mode & LWSSPIMODE_CPOL) ?
					   0 : LWSGGPIO_FL_START_LOW));
	ctx->gpio->mode(ctx->mosi, LWSGGPIO_FL_WRITE | LWSGGPIO_FL_START_LOW);
	ctx->gpio->mode(ctx->miso, LWSGGPIO_FL_READ | LWSGGPIO_FL_PULLUP);

	return 0;
}

/* if active, prepare DnC before this and call separately for Cmd / Data */

static void
lws_bb_spi_write(lws_bb_spi_t *ctx, const uint8_t *buf, size_t len)
{
	uint8_t u, inv = !!(ctx->bb_ops.bus_mode & LWSSPIMODE_CPOL);

	while (len--) {
		int n;

		u = *buf++;

		for (n = 0; n < 4; n++) {
			ctx->gpio->set(ctx->clk, inv);
			ctx->gpio->set(ctx->mosi, !!(u & 0x80));
			ctx->gpio->set(ctx->clk, !inv);
			ctx->gpio->set(ctx->clk, inv);
			ctx->gpio->set(ctx->mosi, !!(u & 0x40));
			ctx->gpio->set(ctx->clk, !inv);
			u <<= 2;
		}
	}

	ctx->gpio->set(ctx->clk, 0 ^ inv);
}

static void
lws_bb_spi_read(lws_bb_spi_t *ctx, uint8_t *buf, size_t len)
{
	uint8_t u = 0;
	uint8_t inv = !!(ctx->bb_ops.bus_mode & LWSSPIMODE_CPOL);

	while (len--) {
		int n;

		for (n = 0; n < 8; n++) {
			ctx->gpio->set(ctx->clk, inv);
			u = (u << 1) | !!ctx->gpio->read(ctx->miso);
			ctx->gpio->set(ctx->mosi, !!(u & 0x80));
			ctx->gpio->set(ctx->clk, !inv);
		}
		*buf++ = u;
	}

	ctx->gpio->set(ctx->clk, 0 ^ inv);
}

int
lws_bb_spi_queue(const lws_spi_ops_t *octx, const lws_spi_desc_t *desc)
{
	lws_bb_spi_t *ctx = (lws_bb_spi_t *)octx;
	const uint8_t *src = desc->src;

	/* clock to idle */
	ctx->gpio->set(ctx->clk, 0 ^ !!(octx->bus_mode & LWSSPIMODE_CPOL));
	/* enable nCS */
	ctx->gpio->set(ctx->ncs[desc->channel], 0);

	if (desc->count_cmd) {
		ctx->gpio->set(ctx->ncmd[desc->channel], 0);
		lws_bb_spi_write(ctx, src, desc->count_cmd);
		ctx->gpio->set(ctx->ncmd[desc->channel], 1);

		src += desc->count_cmd;
	}

	if (desc->count_write)
		lws_bb_spi_write(ctx, desc->data, desc->count_write);

	if (desc->count_read)
		lws_bb_spi_read(ctx, desc->dest, desc->count_read);

	/* disable nCS */
	ctx->gpio->set(ctx->ncs[desc->channel], 1);

	/* clock to idle */
	ctx->gpio->set(ctx->clk, 0 ^ !!(octx->bus_mode & LWSSPIMODE_CPOL));

	return 0;
}
