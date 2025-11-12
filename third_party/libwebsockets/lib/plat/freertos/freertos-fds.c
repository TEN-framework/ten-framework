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

void
lws_plat_insert_socket_into_fds(struct lws_context *context, struct lws *wsi)
{
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];

	pt->fds[pt->fds_count++].revents = 0;
}

void
lws_plat_delete_socket_from_fds(struct lws_context *context,
						struct lws *wsi, int m)
{
	struct lws_context_per_thread *pt = &context->pt[(int)wsi->tsi];

	pt->fds_count--;
}

int
lws_plat_change_pollfd(struct lws_context *context,
		      struct lws *wsi, struct lws_pollfd *pfd)
{
	return 0;
}

int
insert_wsi(const struct lws_context *context, struct lws *wsi)
{
    assert(context->lws_lookup[wsi->desc.sockfd -
                               lws_plat_socket_offset()] == 0);

    context->lws_lookup[wsi->desc.sockfd - \
                      lws_plat_socket_offset()] = wsi;

    return 0;
}
