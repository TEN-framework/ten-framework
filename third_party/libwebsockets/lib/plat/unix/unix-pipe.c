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
 */

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "private-lib-core.h"

int
lws_plat_pipe_create(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	int n;

#if defined(LWS_HAVE_EVENTFD)
	pt->dummy_pipe_fds[0] = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	pt->dummy_pipe_fds[1] = -1;

	n = pt->dummy_pipe_fds[0] < 0 ? -1 : 0;
	goto set;

#elif defined(LWS_HAVE_PIPE2)
	n = pipe2(pt->dummy_pipe_fds, O_NONBLOCK);
#else
	n = pipe(pt->dummy_pipe_fds);
#endif

#if defined(LWS_HAVE_EVENTFD)
set:
#endif
	if (n >= 0) {
		if (fcntl(pt->dummy_pipe_fds[0], F_SETFL, O_NONBLOCK) < 0)
			n = -1;
		else if (pt->dummy_pipe_fds[1] >= 0) {
			if (fcntl(pt->dummy_pipe_fds[1], F_SETFL, O_NONBLOCK) < 0)
				n = -1;
		}
	}

	return n;
}

int
lws_plat_pipe_signal(struct lws_context *ctx, int tsi)
{
	struct lws_context_per_thread *pt = &ctx->pt[tsi];
#if defined(LWS_HAVE_EVENTFD)
	eventfd_t value = 1;

	return eventfd_write(pt->dummy_pipe_fds[0], value);
#else
	char buf = 0;
	int n;

	n = (int)write(pt->dummy_pipe_fds[1], &buf, 1);

	return n != 1;
#endif
}

void
lws_plat_pipe_close(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];

	if (pt->dummy_pipe_fds[0] && pt->dummy_pipe_fds[0] != -1)
		close(pt->dummy_pipe_fds[0]);
	if (pt->dummy_pipe_fds[1] && pt->dummy_pipe_fds[1] != -1)
		close(pt->dummy_pipe_fds[1]);

	pt->dummy_pipe_fds[0] = pt->dummy_pipe_fds[1] = -1;
}
