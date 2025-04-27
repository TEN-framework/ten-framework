# Abstract protocols and transports

## Overview

Until now protocol implementations in lws have been done directly
to the network-related apis inside lws.

In an effort to separate out completely network implementation
details from protocol specification, lws now supports
"abstract protocols" and "abstract transports".

![lws_abstract overview](/doc-assets/abstract-overview.svg)

The concept is that the implementation is split into two separate
chunks of code hidden behind "ops" structs... the "abstract protocol"
implementation is responsible for the logical protocol operation
and reads and writes only memory buffers.

The "abstract transport" implementation is responsible for sending
and receiving buffers on some kind of transport, and again is hidden
behind a standardized ops struct.

In the system, both the abstract protocols and transports are
found by their name.

An actual "connection" is created by calling a generic api
`lws_abs_bind_and_create_instance()` to instantiate the
combination of a protocol and a transport.

This makes it possible to confidently offer the same protocol on
completely different transports, eg, like serial, or to wire
up the protocol implementation to a test jig sending canned
test vectors and confirming the response at buffer level, without
any network.  The abstract protocol itself has no relationship
to the transport at all and is completely unchanged by changes
to the transport.

In addition, generic tokens to control settings in both the
protocol and the transport are passed in at instantiation-time,
eg, controlling the IP address targeted by the transport.

lws SMTP client support has been rewritten to use the new scheme,
and lws provides a raw socket transport built-in.

## Public API

The public api for defining abstract protocols and transports is
found at

 - [abstract.h](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/abstract/abstract.h)
 - [protocols.h](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/abstract/protocols.h)
 - [transports.h](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/abstract/transports.h)

### `lws_abs_t`

The main structure that defines the abstraction is `lws_abs_t`,
this is a name and then pointers to the protocol and transport,
optional tokens to control both the protocol and transport,
and pointers to private allocations for both the
protocol and transport when instantiated.

The transport is selected using

```
LWS_VISIBLE LWS_EXTERN const lws_abs_transport_t *
lws_abs_transport_get_by_name(const char *name);
```

and similarly the protocol by

```
LWS_VISIBLE LWS_EXTERN const lws_abs_protocol_t *
lws_abs_protocol_get_by_name(const char *name);
```

At the moment only "`raw-skt`" is defined as an lws built-in, athough
you can also create your own mock transport the same way for creating
test jigs.

|transport op|meaning|
|---|---|
|`tx()`|transmit a buffer|
|`client_conn()`|start a connection to a peer|
|`close()`|request to close the connection to a peer|
|`ask_for_writeable()`|request a `writeable()` callback when tx can be used|
|`set_timeout()`|set a timeout that will close the connection if reached|
|`state()`|check if the connection is established and can carry traffic|

These are called by the protocol to get things done and make queries
through the abstract transport.

|protocol op|meaning|
|---|---|
|`accept()`|The peer has accepted the transport connection|
|`rx()`|The peer has sent us some payload|
|`writeable()`|The connection to the peer can take more tx|
|`closed()`|The connection to the peer has closed|
|`heartbeat()`|Called periodically even when no network events|

These are called by the transport to inform the protocol of events
and traffic.

### Instantiation

The user fills an lws_abs_t and passes a pointer to it to
`lws_abs_bind_and_create_instance()` to create an instantiation
of the protocol + transport.

### `lws_token_map_t`

The abstract protocol has no idea about a network or network addresses
or ports or whatever... it may not even be hooked up to one.

If the transport it is bound to wants things like that, they are passed
in using an array of `lws_token_map_t` at instantiation time.

For example this is passed to the raw socket protocol in the smtp client
minimal example to control where it would connect to:

```
static const lws_token_map_t smtp_abs_tokens[] = {
{
	.u = { .value = "127.0.0.1" },
	.name_index = LTMI_PEER_DNS_ADDRESS,
}, {
	.u = { .lvalue = 25l },
	.name_index = LTMI_PEER_PORT,
}};
```

## Steps for adding new abstract protocols

 - add the public header in `./include/libwebsockets/abstract/protocols/`
 - add a directory under `./lib/abstract/protocols/`
 - add your protocol sources in the new directory
 - in CMakeLists.txt:
   - add an `LWS_WITH_xxx` for your protocol
   - search for "using any abstract protocol" and add your `LWS_WITH_xxx` to
     the if so it also sets `LWS_WITH_ABSTRACT` if any set
   - add a clause to append your source to SOURCES if `LWS_WITH_xxx` enabled
 - add your `lws_abs_protocol` to the list `available_abs_protocols` in
   `./lib/abstract/abstract.c`

## Steps for adding new abstract transports

 - add the public header in `./include/libwebsockets/abstract/transports/`
 - add your transport sources under `./lib/abstract/transports/`
 - in CMakeLists.txt append your transport sources to SOURCES if `LWS_WITH_ABSTRACT`
   and any other cmake conditionals
 - add an extern for your transport `lws_protocols` in `./lib/core-net/private.h`
 - add your transport `lws_protocols` to `available_abstract_protocols` in
   `./lib/core-net/vhost.c`
 - add your `lws_abs_transport` to the list `available_abs_transports` in
   `./lib/abstract/abstract.c`

# Protocol testing

## unit tests

lws features an abstract transport designed to facilitate unit testing.  This
contains an lws_sequencer that performs the steps of tests involving sending the
protocol test vector buffers and confirming the response of the protocol matches
the test vectors.

## test-sequencer

test-sequencer is a helper that sequences running an array of unit tests and
collects the statistics and gives a PASS / FAIL result.

See the SMTP client api test for an example of how to use.
