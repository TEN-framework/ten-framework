/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2020 Andy Green <andy@warmcat.com>
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
 * C++ classes for Secure Streams - atomic heap messages
 */

#include <libwebsockets.hxx>
#include "private-lib-misc-lwsac.h"

void
lssAc::start(bool atomic)
{
	if (atomic && ac->next) {
		struct lwsac *ac2 = NULL, *i;
		size_t total = (size_t)lwsac_total_alloc(ac);
		uint8_t *p = (uint8_t *)lwsac_use(&ac2, total, total);

		/*
		 * He wants a single linear buffer, and we have more than one
		 * piece... let's make a new, single one, copy the fragments
		 * in and replace the fragmented one with the unified copy.
		 */

		i = ac;
		while (i) {
			size_t bl = lwsac_get_tail_pos(i) -
						lwsac_sizeof(i == ac);
			memcpy(p, (uint8_t *)i + lwsac_sizeof(i == ac), bl);
			p += bl;
		}

		lwsac_free(&ac);
		ac = ac2;
	}

	iter = ac;
}

int
lssAc::get(lssbuf_t *lb)
{
	if (!ac)
		return 1;

	lb->buf = (uint8_t *)iter + lwsac_sizeof(iter == ac);
	lb->len = lwsac_get_tail_pos(iter) - lwsac_sizeof(iter == ac);
	iter = iter->next;

	return 0;
}

void
lssAc::append(lssbuf_t *lb)
{
	uint8_t *p = (uint8_t *)lwsac_use(&ac, lb->len, lb->len);

	if (!p)
		throw lssException("oom");
	memcpy(p, lb->buf, lb->len);
}
