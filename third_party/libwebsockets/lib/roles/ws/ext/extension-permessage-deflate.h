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

#if defined(LWS_WITH_MINIZ)
#include <miniz.h>
#else
#include <zlib.h>
#endif

#define DEFLATE_FRAME_COMPRESSION_LEVEL_SERVER 1
#define DEFLATE_FRAME_COMPRESSION_LEVEL_CLIENT Z_DEFAULT_COMPRESSION

enum arg_indexes {
	PMD_SERVER_NO_CONTEXT_TAKEOVER,
	PMD_CLIENT_NO_CONTEXT_TAKEOVER,
	PMD_SERVER_MAX_WINDOW_BITS,
	PMD_CLIENT_MAX_WINDOW_BITS,
	PMD_RX_BUF_PWR2,
	PMD_TX_BUF_PWR2,
	PMD_COMP_LEVEL,
	PMD_MEM_LEVEL,

	PMD_ARG_COUNT
};

struct lws_ext_pm_deflate_priv {
	z_stream rx;
	z_stream tx;

	unsigned char *buf_rx_inflated; /* RX inflated output buffer */
	unsigned char *buf_tx_deflated; /* TX deflated output buffer */

	unsigned char *buf_tx_holding;

	size_t count_rx_between_fin;
	size_t count_tx_between_fin;

	size_t len_tx_holding;

	unsigned char args[PMD_ARG_COUNT];

	unsigned char tx_first_frame_type;

	unsigned char tx_init:1;
	unsigned char rx_init:1;
	unsigned char compressed_out:1;
};

