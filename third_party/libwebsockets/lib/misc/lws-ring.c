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

struct lws_ring *
lws_ring_create(size_t element_len, size_t count,
		void (*destroy_element)(void *))
{
	struct lws_ring *ring = lws_malloc(sizeof(*ring), "ring create");

	if (!ring)
		return NULL;

	ring->buflen = (uint32_t)(count * element_len);
	ring->element_len = (uint32_t)element_len;
	ring->head = 0;
	ring->oldest_tail = 0;
	ring->destroy_element = destroy_element;

	ring->buf = lws_malloc(ring->buflen, "ring buf");
	if (!ring->buf) {
		lws_free(ring);

		return NULL;
	}

	return ring;
}

void
lws_ring_destroy(struct lws_ring *ring)
{
	if (ring->destroy_element)
		while (ring->oldest_tail != ring->head) {
			ring->destroy_element((uint8_t *)ring->buf +
					      ring->oldest_tail);
			ring->oldest_tail =
				(ring->oldest_tail + ring->element_len) %
				ring->buflen;
		}
	if (ring->buf)
		lws_free_set_NULL(ring->buf);

	lws_free(ring);
}

size_t
lws_ring_get_count_free_elements(struct lws_ring *ring)
{
	int f;

	/*
	 * possible ringbuf patterns
	 *
	 * h == t
	 * |--------t***h---|
	 * |**h-----------t*|
	 * |t**************h|
	 * |*****ht*********|
	 */
	if (ring->head == ring->oldest_tail)
		f = (int)(ring->buflen - ring->element_len);
	else
		if (ring->head < ring->oldest_tail)
			f = (int)((ring->oldest_tail - ring->head) -
			    ring->element_len);
		else
			f = (int)((ring->buflen - ring->head) + ring->oldest_tail -
			    ring->element_len);

	if (f < 2)
		return 0;

	return (unsigned int)f / ring->element_len;
}

size_t
lws_ring_get_count_waiting_elements(struct lws_ring *ring, uint32_t *tail)
{	int f;

	if (!tail)
		tail = &ring->oldest_tail;
	/*
	 * possible ringbuf patterns
	 *
	 * h == t
	 * |--------t***h---|
	 * |**h-----------t*|
	 * |t**************h|
	 * |*****ht*********|
	 */
	if (ring->head == *tail)
		f = 0;
	else
		if (ring->head > *tail)
			f = (int)(ring->head - *tail);
		else
			f = (int)((ring->buflen - *tail) + ring->head);

	return (unsigned int)f / ring->element_len;
}

int
lws_ring_next_linear_insert_range(struct lws_ring *ring, void **start,
				  size_t *bytes)
{
	int n;

	/* n is how many bytes the whole fifo can take */
	n = (int)(lws_ring_get_count_free_elements(ring) * ring->element_len);

	if (!n)
		return 1;

	if (ring->head + (unsigned int)n > ring->buflen) {
		*start = (void *)(((uint8_t *)ring->buf) + ring->head);
		*bytes = ring->buflen - ring->head;

		return 0;
	}

	*start = (void *)(((uint8_t *)ring->buf) + ring->head);
	*bytes = (unsigned int)n;

	return 0;
}

void
lws_ring_bump_head(struct lws_ring *ring, size_t bytes)
{
	ring->head = (ring->head + (uint32_t)bytes) % ring->buflen;
}

size_t
lws_ring_insert(struct lws_ring *ring, const void *src, size_t max_count)
{
	const uint8_t *osrc = src;
	size_t m;
	int n;

	/* n is how many bytes the whole fifo can take */
	n = (int)(lws_ring_get_count_free_elements(ring) * ring->element_len);

	/* restrict n to how much we want to insert */
	if ((uint32_t)n > max_count * ring->element_len)
		n = (int)(max_count * ring->element_len);

	/*
	 * n is legal to insert, but as an optimization we can cut the
	 * insert into one or two memcpys, depending on if it wraps
	 */
	if (ring->head + (unsigned int)n > ring->buflen) {

		/*
		 * He does wrap.  The first memcpy should take us up to
		 * the end of the buffer
		 */

		m = ring->buflen - ring->head;
		memcpy(((uint8_t *)ring->buf) + ring->head, src, m);
		/* we know it will wrap exactly back to zero */
		ring->head = 0;

		/* adapt the second memcpy for what we already did */

		src = ((uint8_t *)src) + m;
		n = n - (int)m;
	}

	memcpy(((uint8_t *)ring->buf) + ring->head, src, (size_t)n);
	ring->head = (ring->head + (unsigned int)n) % ring->buflen;

	return (unsigned long)(((uint8_t *)src + (unsigned int)n) - osrc) / ring->element_len;
}

size_t
lws_ring_consume(struct lws_ring *ring, uint32_t *tail, void *dest,
		 size_t max_count)
{
	uint8_t *odest = dest;
	void *orig_tail = tail;
	uint32_t fake_tail;
	int m, n;

	if (!tail) {
		fake_tail = ring->oldest_tail;
		tail = &fake_tail;
	}

	/* n is how many bytes the whole fifo has for us */
	n = (int)(lws_ring_get_count_waiting_elements(ring, tail) *
							ring->element_len);

	/* restrict n to how much we want to insert */
	if ((size_t)n > max_count * ring->element_len)
		n = (int)(max_count * ring->element_len);

	if (!dest) {
		*tail = ((*tail) + (unsigned int)n) % ring->buflen;
		if (!orig_tail) /* single tail */
			lws_ring_update_oldest_tail(ring, *tail);

		return (unsigned int)n / ring->element_len;
	}
	if (*tail + (unsigned int)n > ring->buflen) {

		/*
		 * He does wrap.  The first memcpy should take us up to
		 * the end of the buffer
		 */

		m = (int32_t)(ring->buflen - *tail);
		memcpy(dest, ((uint8_t *)ring->buf) + *tail, (size_t)m);
		/* we know it will wrap exactly back to zero */
		*tail = 0;

		/* adapt the second memcpy for what we already did */

		dest = ((uint8_t *)dest) + m;
		n -= m;
	}

	memcpy(dest, ((uint8_t *)ring->buf) + *tail, (size_t)n);

	*tail = ((*tail) + (unsigned int)n) % ring->buflen;
	if (!orig_tail) /* single tail */
		lws_ring_update_oldest_tail(ring, *tail);

	return (unsigned int)(((uint8_t *)dest + n) - odest) / (unsigned int)ring->element_len;
}

const void *
lws_ring_get_element(struct lws_ring *ring, uint32_t *tail)
{
	if (!tail)
		tail = &ring->oldest_tail;

	if (*tail == ring->head)
		return NULL;

	return ((uint8_t *)ring->buf) + *tail;
}

void
lws_ring_update_oldest_tail(struct lws_ring *ring, uint32_t tail)
{
	if (!ring->destroy_element) {
		ring->oldest_tail = tail;
		return;
	}

	while (ring->oldest_tail != tail) {
		ring->destroy_element((uint8_t *)ring->buf + ring->oldest_tail);
		ring->oldest_tail = (ring->oldest_tail + ring->element_len) %
				    ring->buflen;
	}
}

uint32_t
lws_ring_get_oldest_tail(struct lws_ring *ring)
{
	return ring->oldest_tail;
}

void
lws_ring_dump(struct lws_ring *ring, uint32_t *tail)
{
	if (tail == NULL)
		tail = &ring->oldest_tail;
	lwsl_notice("ring %p: buflen %u, elem_len %u, head %u, oldest_tail %u\n"
		    "     free_elems: %u; for tail %u, waiting elements: %u\n",
		    ring, (int)ring->buflen, (int)ring->element_len,
		    (int)ring->head, (int)ring->oldest_tail,
		    (int)lws_ring_get_count_free_elements(ring), (int)*tail,
		    (int)lws_ring_get_count_waiting_elements(ring, tail));
}
