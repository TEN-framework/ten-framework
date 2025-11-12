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
 *
 * lws_sequencer is intended to help implement sequences that:
 *
 *  - outlive a single connection lifetime,
 *  - are not associated with a particular protocol,
 *  - are not associated with a particular vhost,
 *  - must receive and issue events inside the event loop
 *
 * lws_sequencer-s are bound to a pt (per-thread) which for the default case of
 * one service thread is the same as binding to an lws_context.
 */
/*
 * retry backoff table... retry n happens after .retry_ms_table[n] ms, with
 * the last entry used if n is greater than the number of entries.
 *
 * The first .conceal_count retries are concealed, but after that the failures
 * are reported.
 */

typedef enum {
	LWSSEQ_CREATED,		/* sequencer created */
	LWSSEQ_DESTROYED,	/* sequencer destroyed */
	LWSSEQ_TIMED_OUT,	/* sequencer timeout */
	LWSSEQ_HEARTBEAT,	/* 1Hz callback */

	LWSSEQ_WSI_CONNECTED,	/* wsi we bound to us has connected */
	LWSSEQ_WSI_CONN_FAIL,	/* wsi we bound to us has failed to connect */
	LWSSEQ_WSI_CONN_CLOSE,	/* wsi we bound to us has closed */


	LWSSEQ_SS_STATE_BASE,	/* secure streams owned by a sequencer provide
				 * automatic messages about state changes on
				 * the sequencer, passing the oridinal in the
				 * event argument field.  The message index is
				 * LWSSEQ_SS_STATE_BASE + the enum from
				 * lws_ss_constate_t */

	LWSSEQ_USER_BASE = 100	/* define your events from here */
} lws_seq_events_t;

typedef enum lws_seq_cb_return {
	LWSSEQ_RET_CONTINUE,
	LWSSEQ_RET_DESTROY
} lws_seq_cb_return_t;

/*
 * handler for this sequencer.  Return 0 if OK else nonzero to destroy the
 * sequencer.  LWSSEQ_DESTROYED will be called back to the handler so it can
 * close / destroy any private assets associated with the sequence.
 *
 * The callback may return either LWSSEQ_RET_CONTINUE for the sequencer to
 * resume or LWSSEQ_RET_DESTROY to indicate the sequence is finished.
 *
 * Event indexes consist of some generic ones but mainly user-defined ones
 * starting from LWSSEQ_USER_BASE.
 */
typedef lws_seq_cb_return_t (*lws_seq_event_cb)(struct lws_sequencer *seq,
			     void *user, int event, void *data, void *aux);

typedef struct lws_seq_info {
	struct lws_context		*context;   /* lws_context for seq */
	int				tsi;	    /* thread service idx */
	size_t				user_size;  /* size of user alloc */
	void				**puser;    /* place ptr to user */
	lws_seq_event_cb		cb;	    /* seq callback */
	const char			*name;	    /* seq name */
	const lws_retry_bo_t		*retry;	    /* retry policy */
	uint8_t				wakesuspend:1; /* important enough to
						     * wake system */
} lws_seq_info_t;

/**
 * lws_seq_create() - create and bind sequencer to a pt
 *
 * \param info:	information about sequencer to create
 *
 * This binds an abstract sequencer to a per-thread (by default, the single
 * event loop of an lws_context).  After the event loop starts, the sequencer
 * will receive an LWSSEQ_CREATED event on its callback from the event loop
 * context, where it can begin its sequence flow.
 *
 * Lws itself will only call the callback subsequently with LWSSEQ_DESTROYED
 * when the sequencer is being destroyed.
 *
 * pt locking is used to protect the related data structures.
 */
LWS_VISIBLE LWS_EXTERN struct lws_sequencer *
lws_seq_create(lws_seq_info_t *info);

/**
 * lws_seq_destroy() - destroy the sequencer
 *
 * \param seq: pointer to the the opaque sequencer pointer returned by
 *	       lws_seq_create()
 *
 * This proceeds to destroy the sequencer, calling LWSSEQ_DESTROYED and then
 * freeing the sequencer object itself.  The pointed-to seq pointer will be
 * set to NULL.
 */
LWS_VISIBLE LWS_EXTERN void
lws_seq_destroy(struct lws_sequencer **seq);

/**
 * lws_seq_queue_event() - queue an event on the given sequencer
 *
 * \param seq: the opaque sequencer pointer returned by lws_seq_create()
 * \param e: the event index to queue
 * \param data: associated opaque (to lws) data to provide the callback
 * \param aux: second opaque data to provide the callback
 *
 * This queues the event on a given sequencer.  Queued events are delivered one
 * per sequencer each subsequent time around the event loop, so the cb is called
 * from the event loop thread context.
 *
 * Notice that because the events are delivered in order from the event loop,
 * the scope of objects pointed to by \p data or \p aux may exceed the lifetime
 * of the thing containing the pointed-to data.  So it's usually better to pass
 * values here.
 */
LWS_VISIBLE LWS_EXTERN int
lws_seq_queue_event(struct lws_sequencer *seq, lws_seq_events_t e, void *data,
			  void *aux);

/**
 * lws_seq_check_wsi() - check if wsi still extant
 *
 * \param seq: the sequencer interested in the wsi
 * \param wsi: the wsi we want to confirm hasn't closed yet
 *
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
LWS_VISIBLE LWS_EXTERN int
lws_seq_check_wsi(struct lws_sequencer *seq, struct lws *wsi);

#define LWSSEQTO_NONE 0

/**
 * lws_seq_timeout_us() - set a timeout by which the sequence must have
 *				completed by a different event or inform the
 *				sequencer
 *
 * \param seq: The sequencer to set the timeout on
 * \param us: How many us in the future to fire the timeout
 *		LWS_SET_TIMER_USEC_CANCEL = cancel any existing timeout
 *
 * This api allows the sequencer to ask to be informed if it has not completed
 * or disabled its timeout after secs seconds.  Lws will send a LWSSEQ_TIMED_OUT
 * event to the sequencer if the timeout expires.
 *
 * Typically the sequencer sets the timeout when starting a step, then waits to
 * hear a queued event informing it the step completed or failed.  The timeout
 * provides a way to deal with the case the step neither completed nor failed
 * within the timeout period.
 *
 * Lws wsi timeouts are not really suitable for this since they are focused on
 * short-term protocol timeout protection and may be set and reset many times
 * in one transaction.  Wsi timeouts also enforce closure of the wsi when they
 * trigger, sequencer timeouts have no side effect except to queue the
 * LWSSEQ_TIMED_OUT message and leave it to the sequencer to decide how to
 * react appropriately.
 */
LWS_VISIBLE LWS_EXTERN int
lws_seq_timeout_us(struct lws_sequencer *seq, lws_usec_t us);

/**
 * lws_seq_from_user(): get the lws_seq_t pointer from the user ptr
 *
 * \param u: the sequencer user allocation returned by lws_seq_create() or
 *	     provided in the sequencer callback
 *
 * This gets the lws_seq_t * from the sequencer user allocation pointer.
 * Actually these are allocated at the same time in one step, with the user
 * allocation immediately after the lws_seq_t, so lws can compute where
 * the lws_seq_t is from having the user allocation pointer.  Since the
 * size of the lws_seq_t is unknown to user code, this helper does it for
 * you.
 */
LWS_VISIBLE LWS_EXTERN struct lws_sequencer *
lws_seq_from_user(void *u);

/**
 * lws_seq_us_since_creation(): elapsed seconds since sequencer created
 *
 * \param seq: pointer to the lws_seq_t
 *
 * Returns the number of us elapsed since the lws_seq_t was
 * created.  This is useful to calculate sequencer timeouts for the current
 * step considering a global sequencer lifetime limit.
 */
LWS_VISIBLE LWS_EXTERN lws_usec_t
lws_seq_us_since_creation(struct lws_sequencer *seq);

/**
 * lws_seq_name(): get the name of this sequencer
 *
 * \param seq: pointer to the lws_seq_t
 *
 * Returns the name given when the sequencer was created.  This is useful to
 * annotate logging when then are multiple sequencers in play.
 */
LWS_VISIBLE LWS_EXTERN const char *
lws_seq_name(struct lws_sequencer *seq);

/**
 * lws_seq_get_context(): get the lws_context sequencer was created on
 *
 * \param seq: pointer to the lws_seq_t
 *
 * Returns the lws_context.  Saves you having to store it if you have a seq
 * pointer handy.
 */
LWS_VISIBLE LWS_EXTERN struct lws_context *
lws_seq_get_context(struct lws_sequencer *seq);
