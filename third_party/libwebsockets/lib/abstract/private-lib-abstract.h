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

#if !defined(__PRIVATE_LIB_ABSTRACT_H__)
#define __PRIVATE_LIB_ABSTRACT_H__

typedef struct lws_token_map lws_token_map_t;
typedef void lws_abs_transport_inst_t;
typedef void lws_abs_protocol_inst_t;

typedef struct lws_abs {
	void				*user;
	struct lws_vhost		*vh;

	const struct lws_abs_protocol	*ap;
	const lws_token_map_t		*ap_tokens;
	const struct lws_abs_transport	*at;
	const lws_token_map_t		*at_tokens;

	struct lws_sequencer		*seq;
	void				*opaque_user_data;

	/* vh lock */
	struct lws_dll2_owner		children_owner; /* our children / queue */
	/* vh lock */
	struct lws_dll2			bound; /* parent or encapsulator */
	/* vh lock */
	struct lws_dll2			abstract_instances;
	lws_abs_transport_inst_t	*ati;
	lws_abs_protocol_inst_t		*api;
} lws_abs_t;

#endif

