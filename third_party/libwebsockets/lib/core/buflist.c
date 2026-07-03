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

#ifdef LWS_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

/* lws_buflist */

int
lws_buflist_append_segment(struct lws_buflist **head, const uint8_t *buf,
			   size_t len)
{
	struct lws_buflist *nbuf;
	int first = !*head;
	void *p = *head;
	int sanity = 1024;

	assert(buf);
	assert(len);

	/* append at the tail */
	while (*head) {
		if (!--sanity) {
			lwsl_err("%s: buflist reached sanity limit\n", __func__);
			return -1;
		}
		if (*head == (*head)->next) {
			lwsl_err("%s: corrupt list points to self\n", __func__);
			return -1;
		}
		head = &((*head)->next);
	}

	(void)p;
	lwsl_info("%s: len %u first %d %p\n", __func__, (unsigned int)len,
					      first, p);

	nbuf = (struct lws_buflist *)lws_malloc(sizeof(struct lws_buflist) +
						len + LWS_PRE + 1, __func__);
	if (!nbuf) {
		lwsl_err("%s: OOM\n", __func__);
		return -1;
	}

	nbuf->len = len;
	nbuf->pos = 0;
	nbuf->next = NULL;

	/* whoever consumes this might need LWS_PRE from the start... */
	p = (uint8_t *)nbuf + sizeof(*nbuf) + LWS_PRE;
	memcpy(p, buf, len);

	*head = nbuf;

	return first; /* returns 1 if first segment just created */
}

static int
lws_buflist_destroy_segment(struct lws_buflist **head)
{
	struct lws_buflist *old = *head;

	assert(*head);
	*head = old->next;
	old->next = NULL;
	old->pos = old->len = 0;
	lws_free(old);

	return !*head; /* returns 1 if last segment just destroyed */
}

void
lws_buflist_destroy_all_segments(struct lws_buflist **head)
{
	struct lws_buflist *p = *head, *p1;

	while (p) {
		p1 = p->next;
		p->next = NULL;
		lws_free(p);
		p = p1;
	}

	*head = NULL;
}

size_t
lws_buflist_next_segment_len(struct lws_buflist **head, uint8_t **buf)
{
	struct lws_buflist *b = (*head);

	if (buf)
		*buf = NULL;

	if (!b)
		return 0;	/* there is no next segment len */

	if (!b->len && b->next)
		if (lws_buflist_destroy_segment(head))
			return 0;

	b = (*head);
	if (!b)
		return 0;	/* there is no next segment len */

	assert(b->pos < b->len);

	if (buf)
		*buf = ((uint8_t *)b) + sizeof(*b) + b->pos + LWS_PRE;

	return b->len - b->pos;
}

size_t
lws_buflist_use_segment(struct lws_buflist **head, size_t len)
{
	struct lws_buflist *b = (*head);

	assert(b);
	assert(len);
	assert(b->pos + len <= b->len);

	b->pos = b->pos + (size_t)len;

	assert(b->pos <= b->len);

	if (b->pos < b->len)
		return (unsigned int)(b->len - b->pos);

	if (lws_buflist_destroy_segment(head))
		/* last segment was just destroyed */
		return 0;

	return lws_buflist_next_segment_len(head, NULL);
}

size_t
lws_buflist_total_len(struct lws_buflist **head)
{
	struct lws_buflist *p = *head;
	size_t size = 0;

	while (p) {
		size += p->len;
		p = p->next;
	}

	return size;
}

int
lws_buflist_linear_copy(struct lws_buflist **head, size_t ofs, uint8_t *buf,
			size_t len)
{
	struct lws_buflist *p = *head;
	uint8_t *obuf = buf;
	size_t s;

	while (p && len) {
		if (ofs < p->len) {
			s = p->len - ofs;
			if (s > len)
				s = len;
			memcpy(buf, ((uint8_t *)&p[1]) + LWS_PRE + ofs, s);
			len -= s;
			buf += s;
			ofs = 0;
		} else
			ofs -= p->len;
		p = p->next;
	}

	return lws_ptr_diff(buf, obuf);
}

int
lws_buflist_linear_use(struct lws_buflist **head, uint8_t *buf, size_t len)
{
	uint8_t *obuf = buf;
	size_t s;

	while (*head && len) {
		s = (*head)->len - (*head)->pos;
		if (s > len)
			s = len;
		memcpy(buf, ((uint8_t *)((*head) + 1)) +
			    LWS_PRE + (*head)->pos, s);
		len -= s;
		buf += s;
		lws_buflist_use_segment(head, s);
	}

	return lws_ptr_diff(buf, obuf);
}

int
lws_buflist_fragment_use(struct lws_buflist **head, uint8_t *buf,
			 size_t len, char *frag_first, char *frag_fin)
{
	uint8_t *obuf = buf;
	size_t s;

	if (!*head)
		return 0;

	s = (*head)->len - (*head)->pos;
	if (s > len)
		s = len;

	if (frag_first)
		*frag_first = !(*head)->pos;

	if (frag_fin)
		*frag_fin = (*head)->pos + s == (*head)->len;

	memcpy(buf, ((uint8_t *)((*head) + 1)) + LWS_PRE + (*head)->pos, s);
	len -= s;
	buf += s;
	lws_buflist_use_segment(head, s);

	return lws_ptr_diff(buf, obuf);
}

#if defined(_DEBUG)
void
lws_buflist_describe(struct lws_buflist **head, void *id, const char *reason)
{
	struct lws_buflist *old;
	int n = 0;

	if (*head == NULL)
		lwsl_notice("%p: %s: buflist empty\n", id, reason);

	while (*head) {
		lwsl_notice("%p: %s: %d: %llu / %llu (%llu left)\n", id,
			    reason, n,
			    (unsigned long long)(*head)->pos,
			    (unsigned long long)(*head)->len,
			    (unsigned long long)(*head)->len - (*head)->pos);
		old = *head;
		head = &((*head)->next);
		if (*head == old) {
			lwsl_err("%s: next points to self\n", __func__);
			break;
		}
		n++;
	}
}
#endif
