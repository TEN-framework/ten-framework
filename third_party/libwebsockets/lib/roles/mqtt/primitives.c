/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
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
 * MQTT v5
 *
 * http://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html
 */

#include "private-lib-core.h"

#include <string.h>
#include <sys/types.h>
#include <assert.h>


/*
 * Encode is done into a buffer of at least 4 bytes space.
 *
 * Returns -1 for error, or number of bytes used
 */

int
lws_mqtt_vbi_encode(uint32_t value, void *buf)
{
	uint8_t *p = (uint8_t *)buf, b;

	if (value > 0xfffffff) {
		assert(0);
		return -1;
	}

	do {
		b = value & 0x7f;
		value >>= 7;
		if (value)
			*p++ = (0x80 | b);
		else
			*p++ = b;
	} while (value);

	return lws_ptr_diff(p, (uint8_t *)buf);
}

void
lws_mqtt_vbi_init(lws_mqtt_vbi *vbi)
{
	vbi->value = 0;
	vbi->consumed = 0;
	vbi->budget = 4;
}

void
lws_mqtt_2byte_init(lws_mqtt_vbi *vbi)
{
	vbi->value = 0;
	vbi->consumed = 0;
	vbi->budget = 2;
}

void
lws_mqtt_4byte_init(lws_mqtt_vbi *vbi)
{
	vbi->value = 0;
	vbi->consumed = 0;
	vbi->budget = 4;
}

lws_mqtt_stateful_primitive_return_t
lws_mqtt_vbi_r(lws_mqtt_vbi *vbi, const uint8_t **in, size_t *len)
{
	uint8_t multiplier = 0;
	if (!vbi->budget) {
		lwsl_info("%s: bad vbi\n", __func__);

		return LMSPR_FAILED_ALREADY_COMPLETED;
	}

	while (*len && vbi->budget--) {
		uint8_t u = *((*in)++);

		(*len)--;
		vbi->consumed++;
		vbi->value = vbi->value + (uint32_t)((u & 0x7f) << multiplier);
		multiplier = (uint8_t)(multiplier + 7);
		if (!(u & 0x80))
			return LMSPR_COMPLETED; /* finished */
	}

	if (!vbi->budget) { /* should have ended on b7 = 0 and exited then... */
		lwsl_info("%s: bad vbi\n", __func__);

		return LMSPR_FAILED_FORMAT;
	}

	return LMSPR_NEED_MORE;
}

lws_mqtt_stateful_primitive_return_t
lws_mqtt_mb_parse(lws_mqtt_vbi *vbi, const uint8_t **in, size_t *len)
{
	if (!vbi->budget)
		return LMSPR_FAILED_ALREADY_COMPLETED;

	while (*len && vbi->budget--) {
		vbi->value = (vbi->value << 8) | *((*in)++);
		(*len)--;
		vbi->consumed++;
	}

	return vbi->budget ? LMSPR_NEED_MORE : LMSPR_COMPLETED;
}

/*
 * You can leave buf NULL, if so it will be allocated on the heap once the
 * actual length is known.  nf should be 0, it will be set at allocation time.
 *
 * Or you can ensure no allocation and use an external buffer by setting buf
 * and lim.  But buf must be in the ep context somehow, since it may have to
 * survive returns to the event loop unchanged.  Set nf to 0 in this case.
 *
 * Or you can set buf to an externally allocated buffer, in which case you may
 * set nf so it will be freed when the string is "freed".
 */

void
lws_mqtt_str_init(lws_mqtt_str_t *s, uint8_t *buf, uint16_t lim, char nf)
{
	s->len = 0;	/* at COMPLETED, consumed count is s->len + 2 */
	s->pos = 0;
	s->buf = buf;
	s->limit = lim;
	s->len_valid = 0;
	s->needs_freeing = nf;
}

lws_mqtt_str_t *
lws_mqtt_str_create(uint16_t lim)
{
	lws_mqtt_str_t *s = lws_malloc(sizeof(*s) + lim + 1, __func__);

	if (!s)
		return NULL;

	s->len = 0;
	s->pos = 0;
	s->buf = (uint8_t *)&s[1];
	s->limit = lim;
	s->len_valid = 0;
	s->needs_freeing = 1;

	return s;
}

lws_mqtt_str_t *
lws_mqtt_str_create_init(uint8_t *buf, uint16_t len, uint16_t lim)
{
	lws_mqtt_str_t *s;

	if (!lim)
		lim = len;

	s = lws_mqtt_str_create(lim);

	if (!s)
		return NULL;

	memcpy(s->buf, buf, len);
	s->len = len;
	s->len_valid = 1;
	s->pos = len;

	return s;
}


lws_mqtt_str_t *
lws_mqtt_str_create_cstr_dup(const char *buf, uint16_t lim)
{
	size_t len = strlen(buf);

	if (!lim)
		lim = (uint16_t)len;

	return lws_mqtt_str_create_init((uint8_t *)buf, (uint16_t)len, lim);
}

uint8_t *
lws_mqtt_str_next(lws_mqtt_str_t *s, uint16_t *budget)
{
	if (budget)
		*budget = (uint16_t)(s->limit - s->pos);

	return &s->buf[s->pos];
}

int
lws_mqtt_str_advance(lws_mqtt_str_t *s, int n)
{
	if (n > s->limit - s->pos) {
		lwsl_err("%s: attempted overflow %d vs %d\n", __func__,
			 n, s->limit - s->pos);
		return 1;
	}

	s->pos = (uint16_t)(s->pos + (uint16_t)n);
	s->len = (uint16_t)(s->len + (uint16_t)n);

	return 0;
}

void
lws_mqtt_str_free(lws_mqtt_str_t **ps)
{
	lws_mqtt_str_t *s = *ps;

	if (!s || !s->needs_freeing)
		return;

	/* buf may be independently allocated or allocated along with the
	 * lws_mqtt_str_t at the end... if so the whole lws_mqtt_str_t is freed.
	 */

	if (s->buf != (uint8_t *)&s[1])
		lws_free_set_NULL(s->buf);
	else
		lws_free_set_NULL(*ps);
}

/*
 * Parses and allocates for lws_mqtt_str_t in a fragmentation-immune, but
 * efficient for bulk data way.
 *
 * Returns: LMSPR_NEED_MORE if needs more data,
 * 	    LMSPR_COMPLETED if complete, <0 for error
 *
 * *len is reduced by, and *in is advanced by, the amount of data actually used,
 * except in error case
 *
 * lws_mqtt_str_free() must be called after calling this successfully
 * or not.
 */
lws_mqtt_stateful_primitive_return_t
lws_mqtt_str_parse(lws_mqtt_str_t *s, const uint8_t **in, size_t *len)
{
	const uint8_t *oin = *in;

	/* handle the length + allocation if needed */
	while (*len && !s->len_valid && s->pos < 2) {
		s->len = (uint16_t)((s->len << 8) | *((*in)++));
		(*len)--;
		oin = *in;
		if (++s->pos == 2) {
			if (s->len > s->limit)
				return LMSPR_FAILED_OVERSIZE;

			s->pos = 0;
			s->len_valid = 1;

			if (!s->len) /* do not need to allocate */
				return LMSPR_COMPLETED;

			if (!s->buf) {
				s->buf = lws_malloc(s->len, __func__);
				if (!s->buf)
					return LMSPR_FAILED_OOM;

				s->needs_freeing = 1;
			}
		}
	}

	/* handle copying bulk data into allocation */
	if (s->len_valid && *len) {
		uint16_t span = (uint16_t)(s->len - s->pos);

		if (span > *len)
			span = (uint16_t)*len;

		memcpy(s->buf + s->pos, *in, span);
		*in += span;
		s->pos = (uint16_t)(s->pos + (uint16_t)span);
	}

	*len -= (unsigned long)(*in - oin);

	return s->buf && s->pos == s->len ? LMSPR_COMPLETED : LMSPR_NEED_MORE;
}

int
lws_mqtt_bindata_cmp(const lws_mqtt_str_t *bd1,
		     const lws_mqtt_str_t *bd2)
{
	if (bd1->len != bd2->len)
		return 1;

	if (!!bd1->buf != !!bd2->buf)
		return 1;

	if (!bd1->buf && !bd2->buf)
		return 0;

	return memcmp(bd1->buf, bd2->buf, bd1->len);
}

