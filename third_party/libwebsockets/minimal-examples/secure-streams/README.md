# Secure Streams

Secure Streams is a client API that strictly decouples the policy for connections
from the payloads.  The user code only deals with the stream type name and payloads,
a policy database set at `lws_context` creation time decides all policy about the
connection, including the endpoint, tls CA, and even the wire protocol.

|name|demonstrates|
---|---
minimal-secure-streams|Minimal secure streams client / proxy example
minimal-secure-streams-tx|Proxy used for client-tx test below
minimal-secure-streams-client-tx|Secure streams client showing tx and rx


