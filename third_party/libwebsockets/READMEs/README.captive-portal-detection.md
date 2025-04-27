# Captive Portal Detection

## Background

Wifi devices may face some interception of their connection to the
internet, it's very common for, eg, coffee shop wifi to present some
kind of login or other clickthrough before access to the Internet is
granted.  Devices may need to understand that they are in this
situation, and there are several different techniques for trying to
gague it.

Sequence-wise the device has been granted a DHCP lease and has been
configured with DNS, but the DNS may be wrongly resolving everything
to an address on the LAN or a portal on the net.

Whether there is a captive portal active should be a sticky state for a given
connection if there is not going to be any attempt to login or pass the landing
page, it only needs checking for after DHCP acquisition then.  If there will be
an attempt to satisfy the landing page, the test should be repeated after the
attempt.

## Detection schemes

The most popular detection scheme by numbers is Android's method,
which is to make an HTTP client GET to `http://connectivitycheck.android.com/generate_204`
and see if a 204 is coming back... if intercepted, typically there'll be a
3xx redirect to the portal, perhaps on https.  Or, it may reply on http with
a 200 and show the portal directly... either way it won't deliver a 204
like the real remote server does.

Variations include expecting a 200 but with specific http body content, and
doing a DNS lookup for a static IP that the device knows; if it's resolved to
something else, it knows there's monkey business implying a captive portal.

Other schemes involve https connections going out and detecting that the cert
of the server it's actually talking to doesn't check out, although this is
potentially ambiguous.

Yet more methods are possible outside of tcp or http.

## lws captive portal detect support

lws provides a generic api to start captive portal detection...

```
LWS_EXTERN LWS_VISIBLE int
lws_system_cpd_start(struct lws_context *context);
```

and two states in `lws_system` states to trigger it from, either
`LWS_SYSTATE_CPD_PRE_TIME` which happens after DHCP acquisition but before
ntpclient and is suitable for non https-based scheme where the time doesn't
need to be known, or the alternative `LWS_SYSTATE_CPD_POST_TIME` state which
happens after ntpclient has completed and we know the time.

The actual platform implementation is set using `lws_system_ops_t` function
pointer `captive_portal_detect_request`, ie

```
	int (*captive_portal_detect_request)(struct lws_context *context);
	/**< Check if we can go out on the internet cleanly, or if we are being
	 * redirected or intercepted by a captive portal.
	 * Start the check that proceeds asynchronously, and report the results
	 * by calling lws_captive_portal_detect_result() api
	 */
```

User platform code can provide this to implement whatever scheme they want, when
it has arrived at a result, it can call the lws api `lws_system_cpd_result()` to
inform lws.  If there isn't any captive portal, this will also try to advance the
system state towards OPERATIONAL.

```
/**
 * lws_system_cpd_result() - report the result of the captive portal detection
 *
 * \param context: the lws_context
 * \param result: one of the LWS_CPD_ constants representing captive portal state
 * \param redirect_url: NULL, or the url we were redirected to if result is
 *     LWS_CPD_HTTP_REDIRECT
 *
 * Sets the context's captive portal detection state to result.  User captive
 * portal detection code would call this once it had a result from its test.
 */
LWS_EXTERN LWS_VISIBLE int
lws_system_cpd_result(struct lws_context *context, int result, const char *redirect_url);
```

