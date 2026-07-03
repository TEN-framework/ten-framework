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

static lws_ss_state_return_t
lssmsg_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	return LWSSSSRET_OK;
}

static lws_ss_state_return_t
lssmsg_tx(void *userobj, lws_ss_tx_ordinal_t ord,uint8_t *buf, size_t *len,
   int *flags)
{
	/*
	 * TODO: we don't know how to send things yet
	 */
	return LWSSSSRET_TX_DONT_SEND;
}

static lws_ss_state_return_t
lssmsg_state(void *userobj, void *h_src, lws_ss_constate_t state,
		lws_ss_tx_ordinal_t ack)
{
	return LWSSSSRET_OK;
}


lssMsg::lssMsg(lws_ctx_t ctx, lsscomp_t _comp, std::string uri) :
	lss(ctx, uri, comp, 0, lssmsg_rx, lssmsg_tx, lssmsg_state)
{
}

lssMsg::~lssMsg()
{
}
