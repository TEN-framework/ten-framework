# h2 long poll in lws

lws server and client can support "immortal" streams that are
not subject to normal timeouts under a special condition.  These
are read-only (to the client).

Network connections that contain at least one immortal stream
are themselves not subject to timeouts until the last immortal
stream they are carrying closes.

Because of this, it's recommended there is some other way of
confirming that the client is still active.

## Setting up lws server for h2 long poll

Vhosts that wish to allow clients to serve these immortal
streams need to set the info.options flag `LWS_SERVER_OPTION_VH_H2_HALF_CLOSED_LONG_POLL`
at vhost creation time.  The JSON config equivalent is to set

```
"h2-half-closed-long-poll": "1"
```

on the vhost.  That's all that is needed.

Streams continue to act normally for timeout with the exception
client streams are allowed to signal they are half-closing by
sending a zero-length DATA frame with END_STREAM set.  These
streams are allowed to exist outside of any timeout and data
can be sent on them at will in the server -> client direction.

## Setting client streams for long poll

An API is provided to allow established h2 client streams to
transition to immortal mode and send the END_STREAM to the server
to indicate it.

```
int
lws_h2_client_stream_long_poll_rxonly(struct lws *wsi);
```

## Example applications

You can confirm the long poll flow simply using example applications.
Build and run `http-server/minimal-http-server-h2-long-poll` in one
terminal.

In another, build the usual `http-client/minimal-http-client` example
and run it with the flags `-l --long-poll`

The client will connect to the server and transition to the immortal mode.
The server sends a timestamp every minute to the client, and that will
stay up without timeouts.

