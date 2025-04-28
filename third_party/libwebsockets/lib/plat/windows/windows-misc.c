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

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
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
        Sleep(ms);
}

lws_usec_t
lws_now_usecs(void)
{
#ifndef DELTA_EPOCH_IN_MICROSECS
#define DELTA_EPOCH_IN_MICROSECS 11644473600000000ULL
#endif
	FILETIME filetime;
	ULARGE_INTEGER datetime;

#ifdef _WIN32_WCE
	GetCurrentFT(&filetime);
#else
	GetSystemTimeAsFileTime(&filetime);
#endif

	/*
	 * As per Windows documentation for FILETIME, copy the resulting
	 * FILETIME structure to a ULARGE_INTEGER structure using memcpy
	 * (using memcpy instead of direct assignment can prevent alignment
	 * faults on 64-bit Windows).
	 */
	memcpy(&datetime, &filetime, sizeof(datetime));

	/* Windows file times are in 100s of nanoseconds. */
	return (datetime.QuadPart / 10) - DELTA_EPOCH_IN_MICROSECS;
}


#ifdef _WIN32_WCE
time_t time(time_t *t)
{
	time_t ret = lws_now_usecs() / 1000000;

	if(t != NULL)
		*t = ret;

	return ret;
}
#endif

size_t
lws_get_random(struct lws_context *context, void *buf, size_t len)
{
	size_t n;
	char *p = (char *)buf;

	for (n = 0; n < len; n++)
		p[n] = (unsigned char)rand();

	return n;
}


void
lwsl_emit_syslog(int level, const char *line)
{
	lwsl_emit_stderr(level, line);
}


int kill(int pid, int sig)
{
	lwsl_err("Sorry Windows doesn't support kill().");
	exit(0);
}

int fork(void)
{
	lwsl_err("Sorry Windows doesn't support fork().");
	exit(0);
}


int
lws_plat_recommended_rsa_bits(void)
{
	return 4096;
}



