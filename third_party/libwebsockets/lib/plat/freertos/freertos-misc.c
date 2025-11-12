/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2019 Andy Green <andy@warmcat.com>
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

#include "private-lib-core.h"

/*
 * Normally you don't want this, use lws_sul instead inside the event loop.
 * But sometimes for drivers it makes sense, so there's an internal-only
 * crossplatform api for it.
 */

void
lws_msleep(unsigned int ms)
{
	vTaskDelay(portTICK_PERIOD_MS > ms ? 1 : ms / portTICK_PERIOD_MS);
}

lws_usec_t
lws_now_usecs(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return ((unsigned long long)tv.tv_sec * 1000000LL) + tv.tv_usec;
}

size_t
lws_get_random(struct lws_context *context, void *buf, size_t len)
{
#if defined(LWS_WITH_ESP32)
	uint8_t *pb = buf;

	while (len) {
		uint32_t r = esp_random();
		uint8_t *p = (uint8_t *)&r;
		int b = 4;

		if (len < (size_t)b)
			b = len;

		len -= b;

		while (b--)
			*pb++ = p[b];
	}

	return pb - (uint8_t *)buf;
#else
#if defined(LWS_WITH_MBEDTLS)
	int n;

	n = mbedtls_ctr_drbg_random(&context->mcdc, buf, len);
	if (!n)
		return len;

	/* failed */

	lwsl_err("%s: mbedtls_ctr_drbg_random returned 0x%x\n", __func__, n);
#endif
	return 0;
#endif
}


void lwsl_emit_syslog(int level, const char *line)
{
	lwsl_emit_stderr(level, line);
}

int
lws_plat_drop_app_privileges(struct lws_context *context, int actually_init)
{
	return 0;
}

int
lws_plat_recommended_rsa_bits(void)
{
	/*
	 * 2048-bit key generation takes up to a minute on ESP32, 4096
	 * is like 15 minutes +
	 */
	return 2048;
}
