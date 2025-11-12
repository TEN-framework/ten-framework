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

/*
 * Abstract transport ops
 */

typedef struct lws_abs_transport {
	const char *name;
	int alloc;

	int (*create)(lws_abs_t *abs);
	void (*destroy)(lws_abs_transport_inst_t **d);

	/* check if the transport settings for these connections are the same */
	int (*compare)(lws_abs_t *abs1, lws_abs_t *abs2);

	/* events the abstract protocol invokes (handled by transport) */

	int (*tx)(lws_abs_transport_inst_t *d, uint8_t *buf, size_t len);
	int (*client_conn)(const lws_abs_t *abs);
	int (*close)(lws_abs_transport_inst_t *d);
	int (*ask_for_writeable)(lws_abs_transport_inst_t *d);
	int (*set_timeout)(lws_abs_transport_inst_t *d, int reason, int secs);
	int (*state)(lws_abs_transport_inst_t *d);
} lws_abs_transport_t;

/**
 * lws_abs_protocol_get_by_name() - returns a pointer to the named protocol ops
 *
 * \param name: the name of the abstract protocol
 *
 * Returns a pointer to the named protocol ops struct if available, otherwise
 * NULL.
 */
LWS_VISIBLE LWS_EXTERN const lws_abs_transport_t *
lws_abs_transport_get_by_name(const char *name);

/*
 * bring in public api pieces from transports
 */

#include <libwebsockets/abstract/transports/raw-skt.h>
#include <libwebsockets/abstract/transports/unit-test.h>
