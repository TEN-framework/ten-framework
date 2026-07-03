# JWT support in lws

lws supports the common usage scenarios of JWS (signed) JWT generation,
parsing and transferring in and out as http cookies.  Care is taken to provide
helpers that implement the current security best practices for cookie handling
and JWT validation.  All of the common algorithms like ES512 are supported
along with JWK generation and handling apis.

The build options needed are `-DLWS_WITH_JOSE=1` `-DLWS_WITH_GENCRYPTO=1`.

Underlying JOSE primitives are exposed as apis, some JWT specific primitives
and finally a JWT-via http cookie level creation apis each building on top of
what was underneath.

The higher level APIs are provided additionally because they have the most
opportunity for implementation pitfalls like not validating alg carefully, or
not using the latest cookie security options; the provided APIs handle that
centrally for you.  If your needs vary from what the higher level apis are
doing, you can cut-and-paste out those implementations and create your own
using the public lower level apis.

## LWS JWT fields

Lws JWT uses mainly well-known fields

Field|Std|Meaning
---|---|---
iss|yes|Issuer, typically the domain like "warmcat.com"
aud|yes|Audience, typically a url path like "https://warmcat.com/sai"
iat|yes|Unix-time "Issued At"
nbf|yes|Unix-time "Not Before"
exp|yes|Unix-time "Expired"
sub|yes|Subject, eg, a username or user email
csrf|no|A random 16-char hex token generated with the JWT for use in links specific to the JWT bearer
ext|no|Application-specific JSON sub-object with whatever fields you need, eg, `"authorization": 1`

## Approach for JWT as session token

Once JWTs are produced, they are autonomous bearer tokens, if they are not kept
secret between the browser and the site, they will be accepted as evidence for
having rights to the session from anyone.

Requiring https, and various other cookie hardening techniques make it more
difficult for them to leak, but it is still necessary to strictly constrain the
token's validity time, usually to a few tens of minutes or how long it takes a
user to login and get stuff done on the site in one session.

## CSRF mitigation

Cross Site Request Forgery (CSRF) is a hacking scenario where an authorized
user with a valid token is tricked into clicking on an external link that
performs some action with side-effects on the site he has active auth on.  For
example, he has a cookie that's logged into his bank, and the link posts a form
to the bank site transferring money to the attacker.

Lws JWT mitigates this possibility by putting a random secret in the generated
JWT; when the authorized user presents his JWT to generate the page, generated
links that require auth to perform their actions include the CSRF string from
that user's current JWT.

When the user clicks those links intentionally, the CSRF string in the link
matches the CSRF string in the currently valid JWT that was also provided to
the server along with the click, and all is well.

An attacker does not know the random, ephemeral JWT CSRF secret to include in
forged links, so the attacker-controlled action gets rejected at the server as
having used an invalid link.

The checking and link manipulation need to be implemented in user code / JS...
lws JWT provides the random CSRF secret in the JWT and makes it visible to the
server when the incoming JWT is processed.

## Need for client tracking of short JWT validity times

Many links or references on pages do not require CSRF strings, only those that
perform actions with side-effects like deletion or money transfer should need
protecting this way.

Due to CSRF mitigation, generated pages containing the protected links
effectively have an expiry time linked to that of the JWT, since only the bearer
of the JWT used to generate the links on the page can use them; once that
expires actually nobody can use them and the page contents, which may anyway
be showing content that only authenticated users can see must be invalidated and
re-fetched.  Even if the contents are visible without authentication, additional
UI elements like delete buttons that should only be shown when authenticated
will wrongly still be shown 

For that reason, the client should be informed by the server along with the
authentication status, the expiry time of it.  The client should then by itself
make arrangements to refresh the page when this time is passed,
either showing an unauthenticated version of the same page if it exists, or by
redirecting to the site homepage if showing any of the contents required
authentication.  The user can then log back in using his credientials typically
stored in the browser's password store and receive a new short-term JWT with a
new random csrf token along with a new page using the new csrf token in its
links.

## Considerations for long-lived connections

Once established as authorized, websocket links may be very long-lived and hold
their authorization state at the server.  Although the browser monitoring the
JWT reloading the page on auth expiry should mitigate this, an attacker can
choose to just not do that and have an immortally useful websocket link.

At least for actions on the long-lived connection, it should not only confirm
the JWT authorized it but that the current time is still before the "exp" time
in the JWT, this is made available as `expiry_unix_time` in the args struct
after successful validation.

Ideally the server should close long-lived connections according to their auth
expiry time.

## JWT lower level APIs

The related apis are in `./include/libwebsockets/lws-jws.h`

### Validation of JWT

```
int
lws_jwt_signed_validate(struct lws_context *ctx, struct lws_jwk *jwk,
			const char *alg_list, const char *com, size_t len,
			char *temp, int tl, char *out, size_t *out_len);
```

### Composing and signing JWT

```
int
lws_jwt_sign_compact(struct lws_context *ctx, struct lws_jwk *jwk,
		     const char *alg, char *out, size_t *out_len, char *temp,
		     int tl, const char *format, ...);
```

## JWT creation and cookie get / set API

Both the validation and signing apis use the same struct to contain their
aguments.

```
struct lws_jwt_sign_set_cookie {
	struct lws_jwk			*jwk;
	/**< entry: required signing key */
	const char			*alg;
	/**< entry: required signing alg, eg, "ES512" */
	const char 			*iss;
	/**< entry: issuer name to use */
	const char			*aud;
	/**< entry: audience */
	const char			*cookie_name;
	/**< entry: the name of the cookie */
	char				sub[33];
	/**< sign-entry, validate-exit: subject */
	const char			*extra_json;
	/**< sign-entry, validate-exit:
	 * optional "ext" JSON object contents for the JWT */
	size_t				extra_json_len;
	/**< validate-exit:
	 * length of optional "ext" JSON object contents for the JWT */
	const char			*csrf_in;
	/**< validate-entry:
	 * NULL, or an external CSRF token to check against what is in the JWT */
	unsigned long			expiry_unix_time;
	/**< sign-entry: seconds the JWT and cookie may live,
	 * validate-exit: expiry unix time */
};

int
lws_jwt_sign_token_set_http_cookie(struct lws *wsi,
				   const struct lws_jwt_sign_set_cookie *i,
				   uint8_t **p, uint8_t *end);
int
lws_jwt_get_http_cookie_validate_jwt(struct lws *wsi,
				     struct lws_jwt_sign_set_cookie *i,
				     char *out, size_t *out_len);
```
