# `lws_sul` scheduler api

Since v3.2 lws no longer requires periodic checking for timeouts and
other events.  A new system was refactored in where future events are
scheduled on to a single, unified, sorted linked-list in time order,
with everything at us resolution.

This makes it very cheap to know when the next scheduled event is
coming and restrict the poll wait to match, or for event libraries
set a timer to wake at the earliest event when returning to the
event loop.

Everything that was checked periodically was converted to use `lws_sul`
and schedule its own later event.  The end result is when lws is idle,
it will stay asleep in the poll wait until a network event or the next
scheduled `lws_sul` event happens, which is optimal for power.

# Side effect for older code

If your older code uses `lws_service_fd()`, it used to be necessary
to call this with a NULL pollfd periodically to indicate you wanted
to let the background checks happen.  `lws_sul` eliminates the whole
concept of periodic checking and NULL is no longer a valid pollfd
value for this and related apis.

# Using `lws_sul` in user code

See `minimal-http-client-multi` for an example of using the `lws_sul`
scheduler from your own code; it uses it to spread out connection
attempts so they are staggered in time.  You must create an
`lws_sorted_usec_list_t` object somewhere, eg, in you own existing object.

```
static lws_sorted_usec_list_t sul_stagger;
```

Create your own callback for the event... the argument points to the sul object
used when the callback was scheduled.  You can use pointer arithmetic to translate
that to your own struct when the `lws_sorted_usec_list_t` was a member of the
same struct.

```
static void
stagger_cb(lws_sorted_usec_list_t *sul)
{
...
}
```

When you want to schedule the callback, use `lws_sul_schedule()`... this will call
it 10ms in the future

```
	lws_sul_schedule(context, 0, &sul_stagger, stagger_cb, 10 * LWS_US_PER_MS);
```

In the case you destroy your object and need to cancel the scheduled callback, use

```
	lws_sul_schedule(context, 0, &sul_stagger, NULL, LWS_SET_TIMER_USEC_CANCEL);
```

# lws_sul2 and system suspend

In v4.1, alongside the existing `lws_sul` apis there is a refactor and additional
functionality aimed at negotiating system suspend, while remaining completely
backwards-compatible with v3.2+ lws_sul apis.

Devicewide suspend is basically the withdrawal of CPU availability for an unbounded
amount of time, so what may have been scheduled by the user code may miss its time
slot because the cpu was down and nothing is getting serviced.  Whether that is
actively desirable, OK, a big disaster, or a failure that will be corrected at other
layers at the cost of, eg, some additional latency, depends on the required device
behaviours and the function of the user code that was scheduled, and its meaning to
the system.

Before v4.1, lws just offers the same scheduling service for everything both internal
and arranged by user code, and has no way to know what is critical for the device to
operate as intended, and so must force wake from suspend, or if for that scheduled
event 'failure [to get the event] is an option'.

For example locally-initiated periodic keepalive pings not happening may allow
persistently dead (ie, no longer passing data) connections to remain unrenewed, but
eventually when suspend ends for another reason, the locally-initiated PING probes
will resume and it will be discovered and if the connectivity allows, corrected.

If the device's function can handle the latency of there being no connectivity in
suspend under those conditions until it wakes for another reason, it's OK for these
kind of timeouts to be suppressed during suspend and basically take the power saving
instead.  If for a particular device it's intolerable to ever have a silently dead
connection for more than a very short time compared to suspend durations, then these
kind of timeouts must have the priority to wake the whole device from suspend so
they continue to operate unimpeded.

That is just one example, lws offers generic scheduler services the user code can
exploit for any purpose, including mission-critical ones.  The changes give the user
code a way to tell lws if a particular scheduled event is important enough to the
system operation to wake the system from devicewide suspend.

