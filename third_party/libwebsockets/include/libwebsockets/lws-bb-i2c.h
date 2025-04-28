/*
 * I2C - bitbanged generic gpio implementation
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
 * This is like an abstract class for gpio, a real implementation provides
 * functions for the ops that use the underlying OS gpio arrangements.
 */

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
		.init = lws_bb_i2c_init, \
		.start = lws_bb_i2c_start, \
		.stop = lws_bb_i2c_stop, \
		.write = lws_bb_i2c_write, \
		.read = lws_bb_i2c_read, \
		.set_ack = lws_bb_i2c_set_ack, \
	}

int
lws_bb_i2c_init(const lws_i2c_ops_t *octx);

int
lws_bb_i2c_start(const lws_i2c_ops_t *octx);

void
lws_bb_i2c_stop(const lws_i2c_ops_t *octx);

int
lws_bb_i2c_write(const lws_i2c_ops_t *octx, uint8_t data);

int
lws_bb_i2c_read(const lws_i2c_ops_t *octx);

void
lws_bb_i2c_set_ack(const lws_i2c_ops_t *octx, int ack);
