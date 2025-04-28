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

#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

int
lws_plat_pipe_create(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];
	struct sockaddr_in *si = &pt->frt_pipe_si;
	lws_sockfd_type *fd = pt->dummy_pipe_fds;
	socklen_t sl;

	/*
	 * Non-WSA HANDLEs can't join the WSAPoll() wait... use a UDP socket
	 * listening on 127.0.0.1:xxxx and send a byte to it from a second UDP
	 * socket to cancel the wait.
	 *
	 * Set the port to 0 at the bind, so lwip will choose a free one in the
	 * ephemeral range for us.
	 */

	fd[0] = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd[0] == INVALID_SOCKET)
		goto bail;

	fd[1] = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd[1] == INVALID_SOCKET)
		goto bail;

	/*
	 * No need for memset since it's in zalloc'd context... it's in the
	 * context so we can reuse the prepared sockaddr to send tp fd[0] whem
	 * we want to cancel the wait
	 */

	si->sin_family = AF_INET;
	si->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	si->sin_port = 0;

	if (bind(fd[0], (const struct sockaddr *)si, sizeof(*si)) < 0)
		goto bail;

	/*
	 * Query the socket to set pt->frt_pipe_si to the full sockaddr it
	 * wants to be addressed by, including the port that the os chose.
	 *
	 * Afterwards, we can use this prepared sockaddr stashed in the context
	 * to trigger the "pipe" without any other preliminaries.
	 */

	sl = sizeof(*si);
	if (getsockname(fd[0], (struct sockaddr *)si, &sl))
		goto bail;

	lwsl_info("%s: cancel UDP skt port %d\n", __func__,
		  ntohs(si->sin_port));

	return 0;

bail:
	lwsl_err("%s: failed\n", __func__);

	return 1;
}

int
lws_plat_pipe_signal(struct lws_context *ctx, int tsi)
{
	struct lws_context_per_thread *pt = &ctx->pt[tsi];
	struct sockaddr_in *si = &pt->frt_pipe_si;
	lws_sockfd_type *fd = pt->dummy_pipe_fds;
	char u = 0;
	int n;

	/*
	 * Send a single UDP byte payload to the listening socket fd[0], forcing
	 * the event loop wait to wake.  fd[1] and context->frt_pipe_si are
	 * set at pt creation and are static.
	 */

	n = sendto(fd[1], &u, 1, 0, (struct sockaddr *)si, sizeof(*si));

	return n != 1;
}

void
lws_plat_pipe_close(struct lws *wsi)
{
	struct lws_context_per_thread *pt = &wsi->a.context->pt[(int)wsi->tsi];

	if (pt->dummy_pipe_fds[0] && pt->dummy_pipe_fds[0] != LWS_SOCK_INVALID)
		closesocket(pt->dummy_pipe_fds[0]);
	if (pt->dummy_pipe_fds[1] && pt->dummy_pipe_fds[1] != LWS_SOCK_INVALID)
		closesocket(pt->dummy_pipe_fds[1]);

	pt->dummy_pipe_fds[0] = pt->dummy_pipe_fds[1] = LWS_SOCK_INVALID;
}
