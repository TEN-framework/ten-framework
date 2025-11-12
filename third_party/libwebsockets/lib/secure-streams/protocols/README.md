# Lws Protocol bindings for Secure Streams

This directory contains the code wiring up normal lws protocols
to Secure Streams.

## The lws_protocols callback

This is the normal lws struct lws_protocols callback that handles events and
traffic on the lws protocol being supported.

The various events and traffic are converted into calls using the Secure
Streams api, and Secure Streams events.

## The connect_munge helper

Different protocols have different semantics in the arguments to the client
connect function, this protocol-specific helper is called to munge the
connect_info struct to match the details of the protocol selected.

The `ss->policy->aux` string is used to hold protocol-specific information
passed in the from the policy, eg, the URL path or websockets subprotocol
name.

## The (library-private) ss_pcols export

Each protocol binding exports two things to other parts of lws (they
are not exported to user code)

 - a struct lws_protocols, including a pointer to the callback

 - a struct ss_pcols describing how secure_streams should use, including
   a pointer to the related connect_munge helper.

In ./lib/core-net/vhost.c, enabled protocols are added to vhost protcols
lists so they may be used.  And in ./lib/secure-streams/secure-streams.c,
enabled struct ss_pcols are listed and checked for matches when the user
creates a new Secure Stream.

