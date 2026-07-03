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

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "private-lib-core.h"

/*
 * Normally you don't want this, use lws_sul instead inside the event loop.
 * But sometimes for drivers it makes sense, so there's an internal-only
 * crossplatform api for it.
 */

void
lws_msleep(unsigned int ms)
{
        usleep((unsigned int)(ms * LWS_US_PER_MS));
}

lws_usec_t
lws_now_usecs(void)
{
#if defined(LWS_HAVE_CLOCK_GETTIME)
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts))
		return 0;

	return (((lws_usec_t)ts.tv_sec) * LWS_US_PER_SEC) +
			((lws_usec_t)ts.tv_nsec / LWS_NS_PER_US);
#else
	struct timeval now;

	gettimeofday(&now, NULL);
	return (((lws_usec_t)now.tv_sec) * LWS_US_PER_SEC) +
			(lws_usec_t)now.tv_usec;
#endif
}

size_t
lws_get_random(struct lws_context *context, void *buf, size_t len)
{
#if defined(__COVERITY__)
	memset(buf, 0, len);
	return len;
#else
	/* coverity[tainted_scalar] */
	return (size_t)read(context->fd_random, (char *)buf, len);
#endif
}

void lwsl_emit_syslog(int level, const char *line)
{
	int syslog_level = LOG_DEBUG;

	switch (level) {
	case LLL_ERR:
		syslog_level = LOG_ERR;
		break;
	case LLL_WARN:
		syslog_level = LOG_WARNING;
		break;
	case LLL_NOTICE:
		syslog_level = LOG_NOTICE;
		break;
	case LLL_INFO:
		syslog_level = LOG_INFO;
		break;
	}
	syslog(syslog_level, "%s", line);
}


int
lws_plat_write_cert(struct lws_vhost *vhost, int is_key, int fd, void *buf,
			size_t len)
{
	ssize_t n;

	n = write(fd, buf, len);

	if (n < 0 || fsync(fd))
		return 1;
	if (lseek(fd, 0, SEEK_SET) < 0)
		return 1;

	return (size_t)n != len;
}


int
lws_plat_recommended_rsa_bits(void)
{
	return 4096;
}

/*
 * Platform-specific ntpclient server configuration
 */

int
lws_plat_ntpclient_config(struct lws_context *context)
{
#if defined(LWS_HAVE_GETENV)
	char *ntpsrv = getenv("LWS_NTP_SERVER");

	if (ntpsrv && strlen(ntpsrv) < 64) {
		lws_system_blob_t *blob = lws_system_get_blob(context,
                                            LWS_SYSBLOB_TYPE_NTP_SERVER, 0);
		if (!blob)
			return 0;

		lws_system_blob_direct_set(blob, (const uint8_t *)ntpsrv,
					    strlen(ntpsrv));
		return 1;
	}
#endif
	return 0;
}

