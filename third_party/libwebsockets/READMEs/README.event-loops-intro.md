# Considerations around Event Loops

Much of the software we use is written around an **event loop**.  Some examples

 - Chrome / Chromium, transmission, tmux, ntp SNTP... [libevent](https://libevent.org/)
 - node.js / cjdns / Julia / cmake ... [libuv](https://archive.is/64pOt)
 - Gstreamer, Gnome / GTK apps ... [glib](https://people.gnome.org/~desrt/glib-docs/glib-The-Main-Event-Loop.html)
 - SystemD ... sdevent
 - OpenWRT ... uloop

Many applications roll their own event loop using poll() or epoll() or similar,
using the same techniques.  Another set of apps use message dispatchers that
take the same approach, but are for cases that don't need to support sockets.
Event libraries provide crossplatform abstractions for this functoinality, and
provide the best backend for their event waits on the platform automagically.

libwebsockets networking operations require an event loop, it provides a default
one for the platform (based on poll() for Unix) if needed, but also can natively
use any of the event loop libraries listed above, including "foreign" loops
already created and managed by the application.

## What is an 'event loop'?

Event loops have the following characteristics:

 - they have a **single thread**, therefore they do not require locking
 - they are **not threadsafe**
 - they require **nonblocking IO**
 - they **sleep** while there are no events (aka the "event wait")
 - if one or more event seen, they call back into user code to handle each in
   turn and then return to the wait (ie, "loop")

### They have a single thread

By doing everything in turn on a single thread, there can be no possibility of
conflicting access to resources from different threads... if the single thread
is in callback A, it cannot be in two places at the same time and also in
callback B accessing the same thing: it can never run any other code
concurrently, only sequentially, by design.

It means that all mutexes and other synchronization and locking can be
eliminated, along with the many kinds of bugs related to them.

### They are not threadsafe

Event loops mandate doing everything in a single thread.  You cannot call their
apis from other threads, since there is no protection against reentrancy.

Lws apis cannot be called safely from any thread other than the event loop one,
with the sole exception of `lws_cancel_service()`.

### They have nonblocking IO

With blocking IO, you have to create threads in order to block them to learn
when your IO could proceed.  In an event loop, all descriptors are set to use
nonblocking mode, we only attempt to read or write when we have been informed by
an event that there is something to read, or it is possible to write.

So sacrificial, blocking discrete IO threads are also eliminated, we just do
what we should do sequentially, when we get the event indicating that we should
do it.

### They sleep while there are no events

An OS "wait" of some kind is used to sleep the event loop thread until something
to do.  There's an explicit wait on file descriptors that have pending read or
write, and also an implicit wait for the next scheduled event.  Even if idle for
descriptor events, the event loop will wake and handle scheduled events at the
right time.

In an idle system, the event loop stays in the wait and takes 0% CPU.

### If one or more event, they handle them and then return to sleep

As you can expect from "event loop", it is an infinite loop alternating between
sleeping in the event wait and sequentially servicing pending events, by calling
callbacks for each event on each object.

The callbacks handle the event and then "return to the event loop".  The state
of things in the loop itself is guaranteed to stay consistent while in a user
callback, until you return from the callback to the event loop, when socket
closes may be processed and lead to object destruction.

Event libraries like libevent are operating the same way, once you start the
event loop, it sits in an inifinite loop in the library, calling back on events
until you "stop" or "break" the loop by calling apis.

## Why are event libraries popular?

Developers prefer an external library solution for the event loop because:

 - the quality is generally higher than self-rolled ones.  Someone else is
   maintaining it, a fulltime team in some cases.
 - the event libraries are crossplatform, they will pick the most effective
   event wait for the platform without the developer having to know the details.
   For example most libs can conceal whether the platform is windows or unix,
   and use native waits like epoll() or WSA accordingly.
 - If your application uses a event library, it is possible to integrate very
   cleanly with other libraries like lws that can use the same event library.
   That is extremely messy or downright impossible to do with hand-rolled loops.

Compared to just throwing threads on it

 - thread lifecycle has to be closely managed, threads must start and must be
   brought to an end in a controlled way.  Event loops may end and destroy
   objects they control at any time a callback returns to the event loop.

 - threads may do things sequentially or genuinely concurrently, this requires
   locking and careful management so only deterministic and expected things
   happen at the user data.

 - threads do not scale well to, eg, serving tens of thousands of connections;
   web servers use event loops.

## Multiple codebases cooperating on one event loop

The ideal situation is all your code operates via a single event loop thread.
For lws-only code, including lws_protocols callbacks, this is the normal state
of affairs.

When there is other code that also needs to handle events, say already existing
application code, or code handling a protocol not supported by lws, there are a
few options to allow them to work together, which is "best" depends on the
details of what you're trying to do and what the existing code looks like.
In descending order of desirability:

### 1) Use a common event library for both lws and application code

This is the best choice for Linux-class devices.  If you write your application
to use, eg, a libevent loop, then you only need to configure lws to also use
your libevent loop for them to be able to interoperate perfectly.  Lws will
operate as a guest on this "foreign loop", and can cleanly create and destroy
its context on the loop without disturbing the loop.

In addition, your application can merge and interoperate with any other
libevent-capable libraries the same way, and compared to hand-rolled loops, the
quality will be higher.

### 2) Use lws native wsi semantics in the other code too

Lws supports raw sockets and file fd abstractions inside the event loop.  So if
your other code fits into that model, one way is to express your connections as
"RAW" wsis and handle them using lws_protocols callback semantics.

This ties the application code to lws, but it has the advantage that the
resulting code is aware of the underlying event loop implementation and will
work no matter what it is.

### 3) Make a custom lws event lib shim for your custom loop

Lws provides an ops struct abstraction in order to integrate with event
libraries, you can find it in ./includes/libwebsockets/lws-eventlib-exports.h.

Lws uses this interface to implement its own event library plugins, but you can
also use it to make your own customized event loop shim, in the case there is
too much written for your custom event loop to be practical to change it.

In other words this is a way to write a customized event lib "plugin" and tell
the lws_context to use it at creation time.  See [minimal-http-server.c](https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/http-server/minimal-http-server-eventlib-custom/minimal-http-server.c)

### 4) Cooperate at thread level

This is less desirable because it gives up on unifying the code to run from a
single thread, it means the codebases cannot call each other's apis directly.

In this scheme the existing threads do their own thing, lock a shared
area of memory and list what they want done from the lws thread context, before
calling `lws_cancel_service()` to break the lws event wait.  Lws will then
broadcast a `LWS_CALLBACK_EVENT_WAIT_CANCELLED` protocol callback, the handler
for which can lock the shared area and perform the requested operations from the
lws thread context.

### 5) Glue the loops together to wait sequentially (don't do this)

If you have two or more chunks of code with their own waits, it may be tempting
to have them wait sequentially in an outer event loop.  (This is only possible
with the lws default loop and not the event library support, event libraries
have this loop inside their own `...run(loop)` apis.)

```
	while (1) {
		do_lws_wait(); /* interrupted at short intervals */
		do_app_wait(); /* interrupted at short intervals */
	}
```

This never works well, either:

 - the whole thing spins at 100% CPU when idle, or

 - the waits have timeouts where they sleep for short periods, but then the
   latency to service on set of events is increased by the idle timeout period
   of the wait for other set of events

## Common Misunderstandings

### "Real Men Use Threads"

Sometimes you need threads or child processes.  But typically, whatever you're
trying to do does not literally require threads.  Threads are an architectural
choice that can go either way depending on the goal and the constraints.

Any thread you add should have a clear reason to specifically be a thread and
not done on the event loop, without a new thread or the consequent locking (and
bugs).

### But blocking IO is faster and simpler

No, blocking IO has a lot of costs to conceal the event wait by blocking.

For any IO that may wait, you must spawn an IO thread for it, purely to handle
the situation you get blocked in read() or write() for an arbitrary amount of
time.  It buys you a simple story in one place, that you will proceed on the
thread if read() or write() has completed, but costs threads and locking to get
to that.

Event loops dispense with the threads and locking, and still provide a simple
story, you will get called back when data arrives or you may send.

Event loops can scale much better, a busy server with 50,000 connections active
does not have to pay the overhead of 50,000 threads and their competing for
locking.

With blocked threads, the thread can do no useful work at all while it is stuck
waiting.  With event loops the thread can service other events until something
happens on the fd.

### Threads are inexpensive

In the cases you really need threads, you must have them, or fork off another
process.  But if you don't really need them, they bring with them a lot of
expense, some you may only notice when your code runs on constrained targets

 - threads have an OS-side footprint both as objects and in the scheduler

 - thread context switches are not slow on modern CPUs, but have side effects
   like cache flushing

 - threads are designed to be blocked for arbitrary amounts of time if you use
   blocking IO apis like write() or read().  Then how much concurrency is really
   happening?  Since blocked threads just go away silently, it is hard to know
   when in fact your thread is almost always blocked and not doing useful work.

 - threads require their own stack, which is on embedded is typically suffering
   from a dedicated worst-case allocation where the headroom is usually idle

 - locking must be handled, and missed locking or lock order bugs found

### But... what about latency if only one thing happens at a time?

 - Typically, at CPU speeds, nothing is happening at any given time on most
   systems, the event loop is spending most of its time in the event wait
   asleep at 0% cpu.

 - The POSIX sockets layer is disjoint from the actual network device driver.
   It means that once you hand off the packet to the networking stack, the POSIX
   api just returns and leaves the rest of the scheduling, retries etc to the
   networking stack and device, descriptor queuing is driven by interrupts in
   the driver part completely unaffected by the event loop part.

 - Passing data around via POSIX apis between the user code and the networking
   stack tends to return almost immediately since its onward path is managed
   later in another, usually interrupt, context.

 - So long as enough packets-worth of data are in the network stack ready to be
   handed to descriptors, actual throughput is completely insensitive to jitter
   or latency at the application event loop

 - The network device itself is inherently serializing packets, it can only send
   one thing at a time.  The networking stack locking also introduces hidden
   serialization by blocking multiple threads.

 - Many user systems are decoupled like the network stack and POSIX... the user
   event loop and its latencies do not affect backend processes occurring in
   interrupt or internal thread or other process contexts

## Conclusion

Event loops have been around for a very long time and are in wide use today due
to their advantages.  Working with them successfully requires understand how to
use them and why they have the advantages and restrictions they do.

The best results come from all the participants joining the same loop directly.
Using a common event library in the participating codebases allows completely
different code can call each other's apis safely without locking.
