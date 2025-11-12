/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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
 * After Public Domain implementations
 *
 * https://github.com/svaarala/duktape/tree/master/misc
 */

#include <private-lib-core.h>

static inline uint64_t rol64(uint64_t x, int k)
{
	return (x << k) | (x >> (64 - k));
}

uint64_t
lws_xos(struct lws_xos *xos)
{
	uint64_t *s = &xos->s[0];
	uint64_t const result = rol64(s[1] * 5, 7) * 9;
	uint64_t const c = s[1] << 17;

	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];

	s[2] ^= c;
	s[3] = rol64(s[3], 45);

	return result;
}

static uint64_t
splitmix64(uint64_t *s)
{
	uint64_t r = *s;

	*s = r + 0x9E3779B97F4A7C15ull;

	r = (r ^ (r >> 30)) * 0xBF58476D1CE4E5B9ull;
	r = (r ^ (r >> 27)) * 0x94D049BB133111EBull;

	return r ^ (r >> 31);
}

void
lws_xos_init(struct lws_xos *xos, uint64_t seed)
{
	int n;

	for (n = 0; n < 4; n++)
		xos->s[n] = splitmix64(&seed);
}

int
lws_xos_percent(struct lws_xos *xos, int percent)
{
	return (int)(lws_xos(xos) % 100) < percent;
}
