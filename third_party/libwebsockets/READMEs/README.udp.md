## Using UDP in lws

UDP is supported in lws... the quickest way is to use the api
`lws_create_adopt_udp()` which returns a wsi bound to the provided
vhost, protocol, `lws_retry` struct, dns address and port.

The wsi can be treated normally and `lws_write()` used to write on
it.

## Implementing UDP retries

Retries are important in udp but there's no standardized ack method
unlike tcp.  Lws allows you to bind an `lws_retry` struct describing
the policy to the udp wsi, but since one UDP socket may have many
transactions in flight, the `lws_sul` and `uint16_t` to count the
retries must live in the user's transaction object like this

```
...
	lws_sorted_usec_list_t	sul;
	uint16_t		retry;
...
```

in the `LWS_CALLBACK_RAW_WRITEABLE` callback, before doing the write,
set up the retry like this

```
	if (lws_dll2_is_detached(&transaction->sul_write.list) &&
	    lws_retry_sul_schedule_retry_wsi(wsi, &transaction->sul_write,
					     transaction_retry_write_cb,
					     &transaction->retry_count_write)) {
			/* we have reached the end of our concealed retries */
		lwsl_warn("%s: concealed retries done, failing\n", __func__);
		goto retry_conn;
	}
```

This manages the retry counter in the transaction object, guards against it wrapping,
selects the timeout using the policy bound to the wsi, and sets the `lws_sul` in the
transaction object to call the given callback if the sul time expires.

In the callback, it should simply call `lws_callback_on_writable()` for the udp wsi.

## Simulating packetloss

You can simulate udp packetloss at tx and rx by using the Fault Injection apis
with the well-known fault names "udp_tx_loss" and "udp_rx_loss", typically
with the probabilistic setting, in commandline format something like
`--fault-injection "wsi/udp_tx_loss(10%)"`
