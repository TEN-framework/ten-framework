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
 * These are used to optionally pass an array of index = C string, binary array,
 * or ulong tokens to the abstract transport or protocol.  For example if it's
 * raw socket transport, then the DNS address to connect to and the port are
 * passed using these when the client created and bound to the transport.
 */

typedef struct lws_token_map {
	union {
		const char	*value;
		uint8_t		*bvalue;
		unsigned long	lvalue;
	} u;
	short			name_index;  /* 0 here indicates end of array */
	short			length_or_zero;
} lws_token_map_t;

/*
 * The indvidual protocols and transports define their own name_index-es which
 * are meaningful to them.  Define index 0 globally as the end of an array of
 * them, and provide bases so user protocol and transport ones don't overlap.
 */

enum {
	LTMI_END_OF_ARRAY,

	LTMI_PROTOCOL_BASE	= 2048,

	LTMI_TRANSPORT_BASE	= 4096
};

struct lws_abs_transport;
struct lws_abs_protocol;
typedef struct lws_abs lws_abs_t;

LWS_VISIBLE LWS_EXTERN const lws_token_map_t *
lws_abs_get_token(const lws_token_map_t *token_map, short name_index);

/*
 * the combination of a protocol, transport, and token maps for each
 */

typedef void lws_abs_transport_inst_t;
typedef void lws_abs_protocol_inst_t;

/**
 * lws_abstract_alloc() - allocate and configure an lws_abs_t
 *
 * \param vhost: the struct lws_vhost to bind to
 * \param user: opaque user pointer
 * \param abstract_path: "protocol.transport" names
 * \param ap_tokens: tokens for protocol options
 * \param at_tokens: tokens for transport
 * \param seq: optional sequencer we should bind to, or NULL
 * \param opaque_user_data: data given in sequencer callback, if any
 *
 * Returns an allocated lws_abs_t pointer set up with the other arguments.
 *
 * Doesn't create a connection instance, just allocates the lws_abs_t and
 * sets it up with the arguments.
 *
 * Returns NULL is there's any problem.
 */
LWS_VISIBLE LWS_EXTERN lws_abs_t *
lws_abstract_alloc(struct lws_vhost *vhost, void *user,
		   const char *abstract_path, const lws_token_map_t *ap_tokens,
		   const lws_token_map_t *at_tokens, struct lws_sequencer *seq,
		   void *opaque_user_data);

/**
 * lws_abstract_free() - free an allocated lws_abs_t
 *
 * \param pabs: pointer to the lws_abs_t * to free
 *
 * Frees and sets the pointer to NULL.
 */

LWS_VISIBLE LWS_EXTERN void
lws_abstract_free(lws_abs_t **pabs);

/**
 * lws_abs_bind_and_create_instance - use an abstract protocol and transport
 *
 * \param abs: the lws_abs_t describing the combination desired
 *
 * This instantiates an abstract protocol and abstract transport bound together.
 * A single heap allocation is made for the combination and the protocol and
 * transport creation ops are called on it.  The ap_tokens and at_tokens
 * are consulted by the creation ops to decide the details of the protocol and
 * transport for the instance.
 */
LWS_VISIBLE LWS_EXTERN lws_abs_t *
lws_abs_bind_and_create_instance(const lws_abs_t *ai);

/**
 * lws_abs_destroy_instance() - destroys an instance
 *
 * \param ai: pointer to the ai pointer to destroy
 *
 * This is for destroying an instance created by
 * lws_abs_bind_and_create_instance() above.
 *
 * Calls the protocol and transport destroy operations on the instance, then
 * frees the combined allocation in one step.  The pointer ai is set to NULL.
 */
LWS_VISIBLE LWS_EXTERN void
lws_abs_destroy_instance(lws_abs_t **ai);

/*
 * bring in all the protocols and transports definitions
 */

#include <libwebsockets/abstract/protocols.h>
#include <libwebsockets/abstract/transports.h>
