/*
 * Generic I2C ops
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
 *
 * This is like an abstract class for spi, a real implementation provides
 * functions for the ops that use the underlying OS arrangements.
 *
 * It uses descriptor / queuing semantics but eg the GPIO BB implementantion is
 * synchronous.
 */

#if !defined(__LWS_SPI_H__)
#define __LWS_SPI_H__

#include <stdint.h>
#include <stddef.h>

typedef int (*lws_spi_cb_t)(void *opaque);

enum {
	LWSSPIMODE_CPOL					= (1 << 0),
	LWSSPIMODE_CPHA					= (1 << 1),

	LWS_SPI_BUSMODE_CLK_IDLE_LOW_SAMP_RISING	= 0,
	LWS_SPI_BUSMODE_CLK_IDLE_HIGH_SAMP_RISING	= LWSSPIMODE_CPOL,
	LWS_SPI_BUSMODE_CLK_IDLE_LOW_SAMP_FALLING	= LWSSPIMODE_CPHA,
	LWS_SPI_BUSMODE_CLK_IDLE_HIGH_SAMP_FALLING	= LWSSPIMODE_CPHA |
							  LWSSPIMODE_CPOL,

	LWS_SPI_TXN_HALF_DUPLEX_DISCRETE	= 0,
	/**< separate MISO and MOSI, but only either MISO or MOSI has data at
	 * one time... i2c style in SPI */
};

typedef struct lws_spi_desc {
	const uint8_t		*src;
	const uint8_t		*data;
	uint8_t			*dest;
	void			*opaque;
	lws_spi_cb_t		completion_cb;
	uint16_t		count_cmd;
	uint16_t		count_write;
	uint16_t		count_read;
	uint8_t			txn_type;
	uint8_t			channel;
} lws_spi_desc_t;

typedef struct lws_spi_ops {
	int  (*init)(const struct lws_spi_ops *ctx);
	int  (*queue)(const struct lws_spi_ops *ctx, const lws_spi_desc_t *desc);
	uint8_t	bus_mode;
} lws_spi_ops_t;

#endif
