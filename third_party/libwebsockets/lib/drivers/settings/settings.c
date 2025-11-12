/*
 * lws_settings
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

#include <private-lib-core.h>

lws_settings_instance_t *
lws_settings_init(const lws_settings_ops_t *so, void *opaque_plat)
{
	lws_settings_instance_t *si = lws_zalloc(sizeof(*si), __func__);

	if (!si)
		return NULL;

	si->so = so;
	si->opaque_plat = opaque_plat;

	return si;
}

void
lws_settings_deinit(lws_settings_instance_t **si)
{
	lws_free(*si);
	*si = NULL;
}

int
lws_settings_plat_printf(lws_settings_instance_t *si, const char *name,
			 const char *format, ...)
{
	va_list ap;
	uint8_t *p;
	int n;

	va_start(ap, format);
	n = vsnprintf(NULL, 0, format, ap);
	va_end(ap);

	p = lws_malloc(n + 2, __func__);
	va_start(ap, format);
	vsnprintf((char *)p, n + 2, format, ap);
	va_end(ap);

	n = si->so->set(si, name, p, n);
	lws_free(p);

	return n;
}
