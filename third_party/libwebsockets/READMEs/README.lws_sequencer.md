# `struct lws_sequencer` introduction

Often a single network action like a client GET is just part of a
larger series of actions, perhaps involving different connections.

Since lws operates inside an event loop, if the outer sequencing
doesn't, it can be awkward to synchronize these steps with what's
happening on the network with a particular connection on the event
loop thread.

![lws_sequencer](/doc-assets/lws_sequencer.svg)

`struct lws_sequencer` provides a generic way to stage multi-step
operations from inside the event loop.  Because it participates
in the event loop similar to a wsi, it always operates from the
service thread context and can access structures that share the
service thread without locking.  It can also provide its own
higher-level timeout handling.

Naturally you can have many of them running in the same event
loop operating independently.

Sequencers themselves bind to a pt (per-thread) service thread,
by default there's only one of these and it's the same as saying
they bind to an `lws_context`.  The sequencer callback may create
wsi which in turn are bound to a vhost, but the sequencer itself
is above all that.

## Sequencer timeouts

The sequencer additionally maintains its own second-resolution timeout
checked by lws for the step being sequenced... this is independent of
any lws wsi timeouts which tend to be set and reset for very short-term
timeout protection inside one transaction.

The sequencer timeout operates separately and above any wsi timeout, and
is typically only reset by the sequencer callback when it receives an
event indicating a step completed or failed, or it sets up the next sequence
step.

If the sequencer timeout expires, then the sequencer receives a queued
`LWSSEQ_TIMED_OUT` message informing it, and it can take corrective action
or schedule a retry of the step.  This message is queued and sent normally
under the service thread context and in order of receipt.

Unlike lws timeouts which force the wsi to close, the sequencer timeout
only sends the message.  This allows the timeout to be used to, eg, wait
out a retry cooloff period and then start the retry when the
`LWSSEQ_TIMED_OUT` is received, according to the state of the sequencer.

## Creating an `struct lws_sequencer`

```
typedef struct lws_seq_info {
	struct lws_context		*context;   /* lws_context for seq */
	int				tsi;	    /* thread service idx */
	size_t				user_size;  /* size of user alloc */
	void				**puser;    /* place ptr to user */
	lws_seq_event_cb		cb;	    /* seq callback */
	const char			*name;	    /* seq name */
	const lws_retry_bo_t		*retry;	    /* retry policy */
} lws_seq_info_t;
```

```
struct lws_sequencer *
lws_sequencer_create(lws_seq_info_t *info);
```

When created, in lws the sequencer objects are bound to a 'per-thread',
which is by default the same as to say bound to the `lws_context`.  You
can tag them with an opaque user data pointer, and they are also bound to
a user-specified callback which handles sequencer events

```
typedef int (*lws_seq_event_cb)(struct lws_sequencer *seq, void *user_data,
				lws_seq_events_t event, void *data);
```

`struct lws_sequencer` objects are private to lws and opaque to the user.  A small
set of apis lets you perform operations on the pointer returned by the
create api.

## Queueing events on a sequencer

Each sequencer object can be passed "events", which are held on a per-sequencer
queue and handled strictly in the order they arrived on subsequent event loops.
`LWSSEQ_CREATED` and `LWSSEQ_DESTROYED` events are produced by lws reflecting
the sequencer's lifecycle, but otherwise the event indexes have a user-defined
meaning and are queued on the sequencer by user code for eventual consumption
by user code in the sequencer callback.

Pending events are removed from the sequencer queues and sent to the sequencer
callback from inside the event loop at a rate of one per event loop wait.

## Destroying sequencers

`struct lws_sequencer` objects are cleaned up during context destruction if they are
still around.

Normally the sequencer callback receives a queued message that
informs it that it's either failed at the current step, or succeeded and that
was the last step, and requests that it should be destroyed by returning
`LWSSEQ_RET_DESTROY` from the sequencer callback.

## Lifecycle considerations

Sequencers may spawn additional assets like client wsi as part of the sequenced
actions... the lifecycle of the sequencer and the assets overlap but do not
necessarily depend on each other... that is a wsi created by the sequencer may
outlive the sequencer.

It's important therefore to detach assets from the sequencer and the sequencer
from the assets when each step is over and the asset is "out of scope" for the
sequencer.  It doesn't necessarily mean closing the assets, just making sure
pointers are invalidated.  For example, if a client wsi held a pointer to the
sequencer as its `.user_data`, when the wsi is out of scope for the sequencer
it can set it to NULL, eg, `lws_set_wsi_user(wsi, NULL);`.

Under some conditions wsi may want to hang around a bit to see if there is a
subsequent client wsi transaction they can be reused on.  They will clean
themselves up when they time out.

## Watching wsi lifecycle from a sequencer

When a sequencer is creating a wsi as part of its sequence, it will be very
interested in lifecycle events.  At client wsi creation time, the sequencer
callback can set info->seq to itself in order to receive lifecycle messages
about its wsi.

|message|meaning|
|---|---|
|`LWSSEQ_WSI_CONNECTED`|The wsi has become connected|
|`LWSSEQ_WSI_CONN_FAIL`|The wsi has failed to connect|
|`LWSSEQ_WSI_CONN_CLOSE`|The wsi had been connected, but has now closed|

By receiving these, the sequencer can understand when it should attempt
reconnections or that it cannot progress the sequence.

When dealing with wsi that were created by the sequencer, they may close at
any time, eg, be closed by the remote peer or an intermediary.  The
`LWSSEQ_WSI_CONN_CLOSE` message may have been queued but since they are
strictly handled in the order they arrived, before it was
handled an earlier message may want to cause some api to be called on
the now-free()-d wsi.  To detect this situation safely, there is a
sequencer api `lws_sequencer_check_wsi()` which peeks the message
buffer and returns nonzero if it later contains an `LWSSEQ_WSI_CONN_CLOSE`
already.

