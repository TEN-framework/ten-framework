# LWS System Message Distribution

## Overview

Independent pieces of a system may need to become aware of events and state
changes in the other pieces quickly, along with the new state if it is small.
These messages are local to inside a system, although they may be triggered by
events outside of it.  Examples include keypresses, or networking state changes.
Individual OSes and frameworks typically have their own fragmented apis for
message-passing, but the lws apis operate the same across any platforms
including, eg, Windows and RTOS and allow crossplatform code to be written once.

Message payloads are short, less than 384 bytes, below system limits for atomic
pipe or UDS datagrams and consistent with heap usage on smaller systems, but
large enough to carry JSON usefully.  Messages are typically low duty cycle.

![SMD message](/doc-assets/smd-message.png)

Messages may be sent by any registered participant, they are allocated on heap
in a linked-list, and delivered to all other registered participants for that
message class no sooner than next time around the event loop.  This retains the
ability to handle multiple event queuing in one event loop trip while
guaranteeing message handling is nonrecursive and so with modest stack usage.
Messages are passed to all other registered participants before being destroyed.

Messages are delivered to all particpants on the same lws_context by default.

![SMD message](/doc-assets/smd-single-process.png)

`lws_smd` apis allow publication and subscription of message objects between
participants that are in a single process and are informed by callback from lws
service thread context.

SMD messages can also broadcast between particpants in different lws_contexts in
different processes, using existing Secure Streams proxying.  In this way
different application processes can intercommunicate and all observe any system
smd messages they are interested in.

![SMD message](/doc-assets/smd-proxy.png)

Registering as a participant and sending messages are threadsafe APIs.

## Message Class

Message class is a bitfield messages use to indicate their general type, eg,
network status, or UI event like a keypress.  Participants set a bitmask to
filter what kind of messages they care about, classes that are 0 in the peer's
filter are never delivered to the peer.   A message usually indicates it is a
single class, but it's possible to set multiple class bits and match on any.  If
so, care must be taken the payload can be parsed by readers expecting any of the
indicated classes, eg, by using JSON.

`lws_smd` tracks a global union mask for all participants' class mask.  Requests
to allocate a message of a class that no participant listens for are rejected,
not at distribution-time but at message allocation-time, so no heap or cpu is
wasted on things that are not currently interesting; but such messages start to
appear as soon as a participant appears that wants them.  The message generation
action should be bypassed without error in the case lws_smd_msg_alloc()
returns NULL.

Various well-known high level classes are defined but also a bit index
`LWSSMDCL_USER_BASE_BITNUM`, which can be used by user code to define up to 8
private classes, with class bit values `(1 << LWSSMDCL_USER_BASE_BITNUM)` thru
`(1 << (LWSSMDCL_USER_BASE_BITNUM + 7))`

## Messaging guarantees

Sent messages are delivered to all registered participants whose class mask
indicates they want it, including the sender.  The send apis are threadsafe.

Locally-delivered message delivery callbacks occur from lws event loop thread
context 0 (the only one in the default case `LWS_MAX_SMP` = 1).  Clients in
different processes receive callbacks from the thread context of their UDS
networking thread.

The message payload may be destroyed immediately when you return from the
callback, you can't store references to it or expect it to be there later.

Messages are timestamped with a systemwide monotonic timestamp.  When
participants are on the lws event loop, messages are delivered in-order.  When
participants are on different threads, delivery order depends on platform lock
acquisition.  External process participants are connected by the Unix Domain
Socket capability of Secure Streams, and may be delivered out-of-order;
receivers that care must consult the message creation timestamps.

## Message Refcounting

To avoid keeping a list of the length of the number of participants for each
message, a refcount is used in the message, computed at the time the message
arrived considering the number of active participants that indicated a desire to
receive messages of that class.

Since peers may detach / close their link asynchronously, the logical peer
objects at the distributor defer destroying themselves until there is no more
possibility of messages arriving timestamped with the period they were active.
A grace period (default 2s) is used to ensure departing peers correctly account
for message refcounts before being destroyed.

## Message creation

Messages may contain arbitrary text or binary data depending on the class.  JSON
is recommended since lws_smd messages are small and low duty cycle but have
open-ended content: JSON is maintainable, extensible, debuggable and self-
documenting and avoids, eg, fragile dependencies on header versions shared
between teams.  To simplify issuing JSON, a threadsafe api to create and send
messages in one step using format strings is provided:

```
int
lws_smd_msg_printf(struct lws_context *ctx, lws_smd_class_t _class,
		   const char *format, ...);
```

## Secure Streams `lws_smd` streamtype

When built with LWS_WITH_SECURE_STREAMS, lws_smd exposes a built-in streamtype
`_lws_smd` which user Secure Streams may use to interoperate with lws_smd using
SS payload semantics.

When using `_lws_smd`, the SS info struct member `manual_initial_tx_credit`
provided by the user when creating the Secure Stream is overloaded to be used as
the RX class mask for the SMD connection associated with the Secure Stream.

Both RX and TX payloads have a 16-byte binary header before the actual payload.
For TX, although the header is 16-bytes, only the first 64-bit class bitfield
needs setting, the timestamp is fetched and added by lws.

 - MSB-first 64-bit class bitfield (currently only 32 least-sig in use) 
 - MSB-First Order 64-bit us-resolution timestamp
 
A helper `lws_smd_ss_msg_printf()` is provided to format and create and smd
message from the SS tx() callback in one step, using the same api layout as
for direct messages via `lws_smd_msg_printf()`

```
int
lws_smd_ss_msg_printf(const char *tag, uint8_t *buf, size_t *len,
		      lws_smd_class_t _class, const char *format, ...);
```

## Well-known message schema

Class|Schema
---|---
LWSSMDCL_INTERACTION|lws_button events
LWSSMDCL_NETWORK|captive portal detection requests and results
LWSSMDCL_SYSTEM_STATE|lws_system state progression

### User interaction Button events

Class: `LWSSMDCL_INTERACTION`

Produced by lws_button when a user interacts with a defined button.

Click-related events are produced alongside up and down related events, the
participant can choose which to attend to according to the meaning of the
interaction.

Both kinds of event go through sophisticated filtering before being issued, see
`./lib/drivers/button/README.md` for details.

#### SMD Button interaction event

Schema:
```
{
	"type":  "button",
	"src":   "<controller-name>/<button-name>",
	"event": "<event-name>"
}
```

For example, `{"type":"button","src":"bc/user","event":"doubleclick"}`

Event name|Meaning
---|---
down|The button passes a filter for being down, useful for duration-based response
up|The button has come up, useful for duration-based response
click|The button activity resulted in a classification as a single-click
longclick|The button activity resulted in a classification as a long-click
doubleclick|The button activity resulted in a classification as a double-click

### Routing Table Change

Class: `LWSSMDCL_NETWORK`

If able to subscribe to OS routing table changes (eg, by rtnetlink on Linux
which is supported), lws announces there have been changes using SMD.

If Captive Portal Detect is enabled, and routing tables changes can be seen,
then a new CPD is requested automatically and the results will be seen over SMD
when that completes.

Schema:

```
	{
	  "rt":      "add|del",   "add" if being added
	}
```

When the context / pts are created, if linux then lws attempts to get the
routing table sent, which requires root.  This is done before the permissions
are dropped after protocols init.

Lws maintains a cache of the routing table in each pt.  Upon changes, existing
connections are reassessed to see if their peer can still be routed to, if not
the connection is closed.

If a gateway route changes, `{"trigger":"cpdcheck","src":"gw-change"}` is
issued on SMD as well.

### Captive Portal Detection

Class: `LWSSMDCL_NETWORK`

Actively detects if the network can reach the internet or if it is
intercepted by a captive portal.  The detection steps are programmable
via the Secure Streams Policy for a streamtype `captive_portal_detect`, eg

```
	"captive_portal_detect": {
		"endpoint":		"connectivitycheck.android.com",
		"http_url":		"generate_204",
		"port":			80,
		"protocol":		"h1",
		"http_method":		"GET",
		"opportunistic":	true,
		"http_expect":		204,
		"http_fail_redirect":	true
	}
```

#### SMD Report Result

Schema: `{"type": "cpd", "result":"<result>"}`

result|meaning
---|---
OK|Internet is reachable
Captive|Internet is behind a captive portal
No internet|There is no connectivity

#### SMD Request re-detection

Schema: `{"trigger": "cpdcheck"}`

### lws_system state progression

Class: `LWSSMDCL_SYSTEM_STATE`

Lws system state changes are forwarded to lws_smd messages so participants not
on the lws event loop directly can be aware of progress.  Code registering a
lws_system notifier callback, on the main lws loop, can synchronously veto state
changes and hook proposed state changes, lws_smd events are asynchronous
notifications of state changes after they were decided only... however they are
available over the whole system.

It's not possible to make validated TLS connections until the system has
acquired the date as well as acquired an IP on a non-captive portal connection,
for that reason user code will usually be dependent on the system reaching
"OPERATIONAL" state if lws is responsible for managing the boot process.

#### System state event

Schema: `{"state":"<state>"}"`

State|Meaning
---|---
CONTEXT_CREATED|We're creating the lws_context
INITIALIZED|Initial vhosts and protocols initialized
IFACE_COLDPLUG|Network interfaces discovered
DHCP|DHCP acquired
CPD_PRE_TIME|Captive portal detect hook before we have system time
TIME_VALID|Ntpclient has run
CPD_POST_TIME|Captive portal detect hook after system time (tls-based check)
POLICY_VALID|The system policy has been acquired and parsed
REGISTERED|This device is registered with an authority
AUTH1|We acquired auth1 from the authority using our registration info
AUTH2|We acquired auth2 from the authority using our registration info
OPERATIONAL|We are active and able to make authenticated tls connections
POLICY_INVALID|The policy is being changed
