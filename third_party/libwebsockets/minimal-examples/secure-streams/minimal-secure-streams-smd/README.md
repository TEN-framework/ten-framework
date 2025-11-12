# lws minimal secure streams SMD

This application creates a Secure Stream link to LWS SMD, System
Message Distribution.

The SS is able to receive system messages matching a specified
class filter, and issue system messages also using SS payload
semantics.

Both a direct api lws_smd participant and an SS based one are instantiated.
They both filter on system messages.

When the Secure Stream is created, it asks to send using normal the SS api.
In the SS tx callback, it prepares a header and then send a NETWORK class
message.

Numbers of messages received each way and sent is compared after 2s and the
test exits with a success or a fail.

### Building and testing

Build with

 -DLWS_WITH_SECURE_STREAMS=1
 -DLWS_WITH_SECURE_STREAMS_PROXY_API=1
 -DLWS_WITH_MINIMAL_EXAMPLES=1

The run ./bin/lws-minimal-secure-streams-smd alone (local SS and direct SMD tests)
and after run ./bin/lws-minimal-secure-streams-proxy in one console and
./bin-lws-minimal-secure-streams-smd-client in the other (SS proxy tests)

### What's going on in the -client test

The -client build version contains the test logic as usual, but outsources the
policy and smd_ server part to the Secure Streams Proxy.

 - start lws-minimal-secure-streams-proxy first
 
 - start lws-minimal-secure-streams-smd-client
 
1) When the client starts, we waits to hear the client state is OPERATIONAL in
a direct smd participant callback.  When it is, he creates a Secure Stream of
streamtype "_lws_smd", creating a local SS handle.

2) The SS creation request is proxied to the SS proxy process over Unix Domain
Sockets.  There it creates a Secure Stream object proxyside, and registers as
an SMD participant... this smd-related behaviour is tied to the special
streamtype name "_lws_smd".  The SMD registration uses a class mask passed to
the proxy in the tx credit field of the serialization.

3) SMD messages that pass the class mask filter are proxied back to the client
over the connection.

4) SMD messages created at the client are passed to the proxy and added to the
proxy's SMD queue, if the same connection's class mask accepts the message then
it will be proxied back to the client same as other messages.

The minimal example produces a variety of messages on the SS link, including
CPD detect trigger.  The SS link is set up to only accept messages of classes
LWSSMDCL_SYSTEM_STATE and LWSSMDCL_NETWORK, INTERACTION type messages are
not accepted.

### multi via proxy

If the -client version is run with `--multi`, it spawns four worker processes
which send and confirm SMD messages between each other via the SS proxy.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
--multi|Fork four worker processes that send and check messages to each other over sspc proxy

```
$ ./bin/lws-minimal-secure-streams-smd -d 1151
[2020/06/18 21:44:54:5148] U: LWS Secure Streams SMD test client [-d<verb>]
[2020/06/18 21:44:54:5601] I: Initial logging level 1151
[2020/06/18 21:44:54:5605] I: Libwebsockets version: 4.0.99-v4.0.0-174-ga8a2eb954 v4.0.0-174-ga8a2eb954
[2020/06/18 21:44:54:5607] I: IPV6 not compiled in
...
[2020/06/18 21:44:54:7906] D: _lws_state_transition: system: changed 11 'AUTH2' -> 12 'OPERATIONAL'
[2020/06/18 21:44:54:7906] D: _realloc: size 81: lws_smd_msg_alloc
[2020/06/18 21:44:54:7907] I: lws_cancel_service
[2020/06/18 21:44:54:7912] I: lws_state_transition_steps: CONTEXT_CREATED -> OPERATIONAL
[2020/06/18 21:44:54:7919] N: myss_tx: sending SS smd
[2020/06/18 21:44:54:7940] D: _realloc: size 84: lws_smd_msg_alloc
[2020/06/18 21:44:54:7944] I: lws_cancel_service
[2020/06/18 21:44:54:7966] D: direct_smd_cb: class: 0x2, ts: 3139600721554
[2020/06/18 21:44:54:7972] D: 
[2020/06/18 21:44:54:7990] D: 0000: 7B 22 73 74 61 74 65 22 3A 22 49 4E 49 54 49 41    {"state":"INITIA
[2020/06/18 21:44:54:7998] D: 0010: 4C 49 5A 45 44 22 7D                               LIZED"}         
[2020/06/18 21:44:54:8001] D: 
[2020/06/18 21:44:54:8016] I: myss_rx: len 39, flags: 3
[2020/06/18 21:44:54:8018] I: 
[2020/06/18 21:44:54:8021] I: 0000: 00 00 00 00 00 00 00 02 00 00 02 DA FE C9 26 92    ..............&.
[2020/06/18 21:44:54:8022] I: 0010: 7B 22 73 74 61 74 65 22 3A 22 49 4E 49 54 49 41    {"state":"INITIA
[2020/06/18 21:44:54:8023] I: 0020: 4C 49 5A 45 44 22 7D                               LIZED"}         
[2020/06/18 21:44:54:8023] I: 
[2020/06/18 21:44:54:8029] D: direct_smd_cb: class: 0x2, ts: 3139600724243
[2020/06/18 21:44:54:8029] D: 
[2020/06/18 21:44:54:8030] D: 0000: 7B 22 73 74 61 74 65 22 3A 22 49 46 41 43 45 5F    {"state":"IFACE_
[2020/06/18 21:44:54:8031] D: 0010: 43 4F 4C 44 50 4C 55 47 22 7D                      COLDPLUG"}      
[2020/06/18 21:44:54:8032] D: 
...
[2020/06/18 21:44:54:8112] D: direct_smd_cb: class: 0x4, ts: 3139600732952
[2020/06/18 21:44:54:8112] D: 
[2020/06/18 21:44:54:8114] D: 0000: 7B 22 73 6F 6D 74 68 69 6E 67 22 3A 22 6E 6F 74    {"somthing":"not
[2020/06/18 21:44:54:8115] D: 0010: 73 65 65 6E 62 79 73 73 72 78 22 7D                seenbyssrx"}    
[2020/06/18 21:44:54:8115] D: 
[2020/06/18 21:44:57:5823] I: 11 12 1
[2020/06/18 21:44:57:5838] I: lws_context_destroy: ctx 0x4f61db0
[2020/06/18 21:44:57:5849] D: _lws_state_transition: system: changed 12 'OPERATIONAL' -> 13 'POLICY_INVALID'
[2020/06/18 21:44:57:5851] D: _realloc: size 84: lws_smd_msg_alloc
[2020/06/18 21:44:57:5853] I: lws_cancel_service
[2020/06/18 21:44:57:5871] I: lws_destroy_event_pipe
[2020/06/18 21:44:57:5906] I: lws_pt_destroy: pt destroyed
[2020/06/18 21:44:57:5913] I: lws_context_destroy2: ctx 0x4f61db0
[2020/06/18 21:44:57:5936] D: lwsac_free: head (nil)
[2020/06/18 21:44:57:5947] D: 0x455970: post vh listl
[2020/06/18 21:44:57:5950] D: 0x455970: post pdl
[2020/06/18 21:44:57:5961] D: 0x455970: baggage
[2020/06/18 21:44:57:5968] D: 0x455970: post dc2
[2020/06/18 21:44:57:6010] D: lws_context_destroy3: ctx 0x4f61db0 freed
[2020/06/18 21:44:57:6014] U: Completed: OK
```