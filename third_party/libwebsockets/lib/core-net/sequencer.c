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

/*
 * per pending event
 */
typedef struct lws_seq_event {
	struct lws_dll2			seq_event_list;

	void				*data;
	void				*aux;
	lws_seq_events_t		e;
} lws_seq_event_t;

/*
 * per sequencer
 */
typedef struct lws_sequencer {
	struct lws_dll2			seq_list;

	lws_sorted_usec_list_t		sul_timeout;
	lws_sorted_usec_list_t		sul_pending;

	struct lws_dll2_owner		seq_event_owner;
	struct lws_context_per_thread	*pt;
	lws_seq_event_cb		cb;
	const char			*name;
	const lws_retry_bo_t		*retry;

	lws_usec_t			time_created;
	lws_usec_t			timeout; /* 0 or time we timeout */

	uint8_t				going_down:1;
	uint8_t				wakesuspend:1;
} lws_seq_t;

#define QUEUE_SANITY_LIMIT 10

static void
lws_sul_seq_heartbeat_cb(lws_sorted_usec_list_t *sul)
{
	struct lws_context_per_thread *pt = lws_container_of(sul,
			struct lws_context_per_thread, sul_seq_heartbeat);

	/* send every sequencer a heartbeat message... it can ignore it */

	lws_start_foreach_dll_safe(struct lws_dll2 *, p, tp,
				   lws_dll2_get_head(&pt->seq_owner)) {
		lws_seq_t *s = lws_container_of(p, lws_seq_t, seq_list);

		/* queue the message to inform the sequencer */
		lws_seq_queue_event(s, LWSSEQ_HEARTBEAT, NULL, NULL);

	} lws_end_foreach_dll_safe(p, tp);

	/* schedule the next one */

	__lws_sul_insert_us(&pt->pt_sul_owner[LWSSULLI_MISS_IF_SUSPENDED],
			    &pt->sul_seq_heartbeat, LWS_US_PER_SEC);
}

int
lws_seq_pt_init(struct lws_context_per_thread *pt)
{
	pt->sul_seq_heartbeat.cb = lws_sul_seq_heartbeat_cb;

	/* schedule the first heartbeat */
	__lws_sul_insert_us(&pt->pt_sul_owner[LWSSULLI_MISS_IF_SUSPENDED],
			    &pt->sul_seq_heartbeat, LWS_US_PER_SEC);

	return 0;
}

lws_seq_t *
lws_seq_create(lws_seq_info_t *i)
{
	struct lws_context_per_thread *pt = &i->context->pt[i->tsi];
	lws_seq_t *seq = lws_zalloc(sizeof(*seq) + i->user_size, __func__);

	if (!seq)
		return NULL;

	seq->cb = i->cb;
	seq->pt = pt;
	seq->name = i->name;
	seq->retry = i->retry;
	seq->wakesuspend = i->wakesuspend;

	*i->puser = (void *)&seq[1];

	/* add the sequencer to the pt */

	lws_pt_lock(pt, __func__); /* ---------------------------------- pt { */

	lws_dll2_add_tail(&seq->seq_list, &pt->seq_owner);

	lws_pt_unlock(pt); /* } pt ------------------------------------------ */

	seq->time_created = lws_now_usecs();

	/* try to queue the creation cb */

	if (lws_seq_queue_event(seq, LWSSEQ_CREATED, NULL, NULL)) {
		lws_dll2_remove(&seq->seq_list);
		lws_free(seq);

		return NULL;
	}

	return seq;
}

static int
seq_ev_destroy(struct lws_dll2 *d, void *user)
{
	lws_seq_event_t *seqe = lws_container_of(d, lws_seq_event_t,
						 seq_event_list);

	lws_dll2_remove(&seqe->seq_event_list);
	lws_free(seqe);

	return 0;
}

void
lws_seq_destroy(lws_seq_t **pseq)
{
	lws_seq_t *seq = *pseq;

	/* defeat another thread racing to add events while we are destroying */
	seq->going_down = 1;

	seq->cb(seq, (void *)&seq[1], LWSSEQ_DESTROYED, NULL, NULL);

	lws_pt_lock(seq->pt, __func__); /* -------------------------- pt { */

	lws_dll2_remove(&seq->seq_list);
	lws_dll2_remove(&seq->sul_timeout.list);
	lws_dll2_remove(&seq->sul_pending.list);
	/* remove and destroy any pending events */
	lws_dll2_foreach_safe(&seq->seq_event_owner, NULL, seq_ev_destroy);

	lws_pt_unlock(seq->pt); /* } pt ---------------------------------- */


	lws_free_set_NULL(seq);
}

void
lws_seq_destroy_all_on_pt(struct lws_context_per_thread *pt)
{
	lws_start_foreach_dll_safe(struct lws_dll2 *, p, tp,
				   pt->seq_owner.head) {
		lws_seq_t *s = lws_container_of(p, lws_seq_t,
						      seq_list);

		lws_seq_destroy(&s);

	} lws_end_foreach_dll_safe(p, tp);
}

static void
lws_seq_sul_pending_cb(lws_sorted_usec_list_t *sul)
{
	lws_seq_t *seq = lws_container_of(sul, lws_seq_t, sul_pending);
	lws_seq_event_t *seqe;
	struct lws_dll2 *dh;
	int n;

	if (!seq->seq_event_owner.count)
		return;

	/* events are only added at tail, so no race possible yet... */

	dh = lws_dll2_get_head(&seq->seq_event_owner);
	seqe = lws_container_of(dh, lws_seq_event_t, seq_event_list);

	n = (int)seq->cb(seq, (void *)&seq[1], (int)seqe->e, seqe->data, seqe->aux);

	/* ... have to lock here though, because we will change the list */

	lws_pt_lock(seq->pt, __func__); /* ----------------------------- pt { */

	/* detach event from sequencer event list and free it */
	lws_dll2_remove(&seqe->seq_event_list);
	lws_free(seqe);
	lws_pt_unlock(seq->pt); /* } pt ------------------------------------- */

	if (n) {
		lwsl_info("%s: destroying seq '%s' by request\n", __func__,
				seq->name);
		lws_seq_destroy(&seq);
	}
}

int
lws_seq_queue_event(lws_seq_t *seq, lws_seq_events_t e, void *data, void *aux)
{
	lws_seq_event_t *seqe;

	if (!seq || seq->going_down)
		return 1;

	seqe = lws_zalloc(sizeof(*seqe), __func__);
	if (!seqe)
		return 1;

	seqe->e = e;
	seqe->data = data;
	seqe->aux = aux;

	// lwsl_notice("%s: seq %s: event %d\n", __func__, seq->name, e);

	lws_pt_lock(seq->pt, __func__); /* ----------------------------- pt { */

	if (seq->seq_event_owner.count > QUEUE_SANITY_LIMIT) {
		lwsl_err("%s: more than %d events queued\n", __func__,
			 QUEUE_SANITY_LIMIT);
	}

	lws_dll2_add_tail(&seqe->seq_event_list, &seq->seq_event_owner);

	seq->sul_pending.cb = lws_seq_sul_pending_cb;
	__lws_sul_insert_us(&seq->pt->pt_sul_owner[seq->wakesuspend],
			    &seq->sul_pending, 1);

	lws_pt_unlock(seq->pt); /* } pt ------------------------------------- */

	return 0;
}

/*
 * Check if wsi still extant, by peeking in the message queue for a
 * LWSSEQ_WSI_CONN_CLOSE message about wsi.  (Doesn't need to do the same for
 * CONN_FAIL since that will never have produced any messages prior to that).
 *
 * Use this to avoid trying to perform operations on wsi that have already
 * closed but we didn't get to that message yet.
 *
 * Returns 0 if not closed yet or 1 if it has closed but we didn't process the
 * close message yet.
 */

int
lws_seq_check_wsi(lws_seq_t *seq, struct lws *wsi)
{
	lws_seq_event_t *seqe;
	struct lws_dll2 *dh;

	lws_pt_lock(seq->pt, __func__); /* ----------------------------- pt { */

	dh = lws_dll2_get_head(&seq->seq_event_owner);
	while (dh) {
		seqe = lws_container_of(dh, lws_seq_event_t, seq_event_list);

		if (seqe->e == LWSSEQ_WSI_CONN_CLOSE && seqe->data == wsi)
			break;

		dh = dh->next;
	}

	lws_pt_unlock(seq->pt); /* } pt ------------------------------------- */

	return !!dh;
}


static void
lws_seq_sul_timeout_cb(lws_sorted_usec_list_t *sul)
{
	lws_seq_t *s = lws_container_of(sul, lws_seq_t, sul_timeout);

	lws_seq_queue_event(s, LWSSEQ_TIMED_OUT, NULL, NULL);
}

/* set us to LWS_SET_TIMER_USEC_CANCEL to remove timeout */

int
lws_seq_timeout_us(lws_seq_t *seq, lws_usec_t us)
{
	seq->sul_timeout.cb = lws_seq_sul_timeout_cb;
	/* list is always at the very top of the sul */
	__lws_sul_insert_us(&seq->pt->pt_sul_owner[seq->wakesuspend],
			(lws_sorted_usec_list_t *)&seq->sul_timeout.list, us);

	return 0;
}

lws_seq_t *
lws_seq_from_user(void *u)
{
	return &((lws_seq_t *)u)[-1];
}

const char *
lws_seq_name(lws_seq_t *seq)
{
	return seq->name;
}

lws_usec_t
lws_seq_us_since_creation(lws_seq_t *seq)
{
	return lws_now_usecs() - seq->time_created;
}

struct lws_context *
lws_seq_get_context(lws_seq_t *seq)
{
	return seq->pt->context;
}

