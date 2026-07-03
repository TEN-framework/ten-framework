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
 * This is like an abstract class for i2c, a real implementation provides
 * functions for the ops that use the underlying OS arrangements.
 */

#if !defined(__LWS_I2C_H__)
#define __LWS_I2C_H__

#include <stdint.h>
#include <stddef.h>

typedef struct lws_i2c_ops {
	int  (*init)(const struct lws_i2c_ops *ctx);
	int  (*start)(const struct lws_i2c_ops *ctx);
	void (*stop)(const struct lws_i2c_ops *ctx);
	int  (*write)(const struct lws_i2c_ops *ctx, uint8_t data);
	int  (*read)(const struct lws_i2c_ops *ctx);
	void (*set_ack)(const struct lws_i2c_ops *octx, int ack);
} lws_i2c_ops_t;

/*
 * These are implemented by calling the ops above, and so are generic
 */

LWS_VISIBLE LWS_EXTERN int
lws_i2c_command(const lws_i2c_ops_t *ctx, uint8_t ads7, uint8_t c);

LWS_VISIBLE LWS_EXTERN int
lws_i2c_command_list(const lws_i2c_ops_t *ctx, uint8_t ads7, const uint8_t *buf,
		     size_t len);

#endif
