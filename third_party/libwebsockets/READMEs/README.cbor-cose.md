# RFC8152 COSE apis

|||
|---|---|---|
|cmake| `LWS_WITH_COSE`|
|Header| ./include/libwebsockets/lws-cose.h|
|api-test| ./minimal-examples/api-tests/api-test-cose/|
|README| ./READMEs/README.cbor-cose.md

COSE is the CBOR equivalent of the JOSE suite of crypto objects and operations.
You can represent public and private EC, RSA and SYMMETRIC keys, and sets of
keys of various types; import the logical keys to and from CBOR; and sign /
verify and encrypt / decrypt payloads using structured CBOR.  Key generation is
also supported.

|type|operations|algs|
|---|---|---|
|lws_cose_key_t|import, export, generation|EC / RSA / SYMMETRIC|
|cose_sign1|sign, validate|ES256/384/512, RS256/384/512|
|cose_sign|sign, validate|ES256/384/512, RS256/384/512|
|cose_mac0|sign, validate|HS256/HS256_64/384/512|
|cose_mac|validate only|HS256/HS256_64/384/512|

The lws COSE support uses the lws gencrypto layer, which calls through to the
tls crypto library, and so works on both OpenSSL and mbedTLS the same.

An increasing number of higher-level IETF specifications use COSE underneath.

## cose_key and sets

Lws provides an `lws_cose_key_t` object to contain a single key's metadata and
key material for EC, RSA and SYMMETRIC key types.

There is a commandline tool wrapping the key dumping and generation apis
available at `./minimal-examples/crypto/lws-crypto-cose-key`

### cose_key and sets import from CBOR and destroying

```
lws_cose_key_t *
lws_cose_key_import(lws_dll2_owner_t *pkey_set, lws_cose_key_import_callback cb,
		    void *user, const uint8_t *in, size_t len);
void
lws_cose_key_destroy(lws_cose_key_t **ck);

void
lws_cose_key_set_destroy(lws_dll2_owner_t *o);
```

To convert a single key, `pkey_set` should be NULL and the created key will be
returned, for a cose_key set, which is simply a CBOR array of cose_keys, it
should be a prepared (ie, zero'd down if nothing in it) lws_dll2_owner_t that
will contain the resulting list of `lws_cose_key_t` objects that were created.
In both cases the return is NULL if there was a fatal error and anything created
has been cleaned up, the return has no other meaning in the cose_key set case.

`lws_cose_key_destroy()` destroys a single `lws_cose_key_t` and sets the
contents of the pointer to NULL, for cose_key sets you instead pass a pointer to
the owner object to `lws_cose_key_set_destroy()` to destroy all the keys in the
set in one step.

cose_key has some confusions about type, kty and alg may be either ints,
representing well-known standardized key and alg types, or freeform strings.
We convert the well-known ints to their string representations at import, so
there can be no confusion later.

### cose_key generation

```
lws_cose_key_t *
lws_cose_key_generate(struct lws_context *context, int cose_kty, int use_mask,
		       int bits, const char *curve, const char *kid);
```

This creates an `lws_cose_key_t`, generates a key (SYMMETRIC) or keypair into
it and returns a pointer to it.

`cose_kty` is one of `LWSCOSE_WKKTV_OKP`, `LWSCOSE_WKKTV_EC2`, `LWSCOSE_WKKTV_RSA`,
or `LWSCOSE_WKKTV_SYMMETRIC`.  `bits` is valid for RSA keys and for EC keys,
`curve` should be a well-known curve name, one of `P-256`, `P-384` and `P-521`
currently.  `use_mask` is a bitfield made up of  (1 << LWSCOSE_WKKO_...) set to
enable the usage on the key.

### cose_key export to CBOR

The export api uses the same CBOR write context as `lws_lec_printf()` uses to
emit the key into an output buffer.  Like the CBOR output apis, it may return
`LWS_LECPCTX_RET_AGAIN` to indicate it filled the buffer and should be called
again to fill another buffer.  `lws_lec_init()` should be used to prepare the
write context and `lws_lec_setbuf()` to reset the output buffer on subsequent
calls, exactly the same as the CBOR write apis.

```
enum lws_lec_pctx_ret
lws_cose_key_export(lws_cose_key_t *ck, lws_lec_pctx_t *ctx, int flags);
```

`flags` may be 0 to only output the public key pieces, or `LWSJWKF_EXPORT_PRIVATE`
to output everything.

## Signing and signature validation

COSE specifies three kinds of signed object, `cose_sign1` which signs a payload
with a single algorithm and key, `cose_sign` which may sign a payload with
multiple algorithms and keys, and `countersign`.

`cose_sign1` has the advantage it can be validated with a single pass through
the signed object; `cose_sign` unfortunately specifies the parameters of the
signatures after the payload and must be done with multiple passes through the
payload, for inline payloads, by caching it in heap.

`cose_sign` and `cose_sign1` objects are supported by lws, Countersigned
objects are not yet supported.

`cose_mac0` is supported using HMAC for signing and validation, `cose_mac` is
only supported for validation.

There is a commandline tool wrapping the signing and validation apis
available at `./minimal-examples/crypto/lws-crypto-cose-sign`

### Signature validation

Signature validation does not have to be done synchronously, to facilitate this
first you create a validation context specifying the type (eg, `SIGTYPE_SINGLE`)
and a keyset of public keys the signature might use to validate (notice even a
single key is passed in an lws_dll2_owner_t keyset).

Creation uses a public `lws_cose_validate_create_info_t` info struct

```
typedef struct lws_cose_validate_create_info {
	struct lws_context		*cx;
	/**< REQUIRED: the lws context */
	lws_dll2_owner_t		*keyset;
	/**< REQUIRED: one or more cose_keys */

	enum lws_cose_sig_types		sigtype;
	/**<  0 if a CBOR tag is in the sig, else one of SIGTYPE_MULTI,
	 * SIGTYPE_SINGLE, etc*/

	lws_cose_validate_pay_cb_t	pay_cb;
	/**< optional: called back with unvalidated payload pieces */
	void				*pay_opaque;
	/**< optional: passed into pay_cb callback along with payload chunk */

	lws_cose_sign_ext_pay_cb_t	ext_cb;
	/**< optional extra application data provision callback */
	void				*ext_opaque;
	/**< optional extra application data provision callback opaque */
	size_t				ext_len;
	/**< if we have extra app data, this must be set to the length of it */
} lws_cose_validate_create_info_t;
```

```
struct lws_cose_validate_context *
lws_cose_validate_create(const lws_cose_validate_create_info_t *info);

void
lws_cose_validate_destroy(struct lws_cose_validate_context **cps);
```

after that as pieces of the signature CBOR become available, they can be
processed by the validation context

```
int
lws_cose_validate_chunk(struct lws_cose_validate_context *cps,
			const uint8_t *in, size_t in_len, size_t *used_in);
```

The parsing of the signature yields a list of result objects indicating
information about each signature it encountered and whether it was validated or
not.  The parsing itself only fails if there is an unrecoverable error, the
completion of parsing does not indicate validation, it may yield zero or more
result objects indicating the validation failed.

```
lws_dll2_owner_t *
lws_cose_validate_results(struct lws_cose_validate_context *cps);

typedef struct {
	lws_dll2_t		list;

	const lws_cose_key_t	*cose_key;
	cose_param_t		cose_alg;

	int			result; /* 0 = validated */

} lws_cose_validate_res_t;
```

It's like this because for multiple signatures, we may only have keys for some
of them, and we may have different policies for validation that can only be
assessed as a whole, eg, we may inisit that signatures pass with specific
algorithms, or all signatures for specific keys must be present and pass.  This
way user code can assess the situation after the signature parsing and make its
decision about overall validity according to its own policies.

## Signing

Signing is again done by creating a signing context using an info struct to pass
in the paramter (a `lws_cose_sign_create_info_t`).

```
#define LCSC_FL_ADD_CBOR_TAG		(1 << 0)
#define LCSC_FL_ADD_CBOR_PREFER_MAC0	(1 << 1)

typedef struct lws_cose_sign_create_info {
	struct lws_context		*cx;
	/**< REQUIRED: the lws context */
	lws_dll2_owner_t		*keyset;
	/**< REQUIRED: one or more cose_keys */

	lws_lec_pctx_t			*lec;
	/**< REQUIRED: the cbor output context to emit to, user must
	 * initialize with lws_lec_init() beforehand */

	lws_cose_sign_ext_pay_cb_t	ext_cb;
	/**< optional extra application data provision callback */
	void				*ext_opaque;
	/**< optional extra application data provision callback opaque */
	size_t				ext_len;
	/**< if we have extra app data, this must be set to the length of it */

	size_t				inline_payload_len;
	/**< REQUIRED: size of the inline payload we will provide */

	int				flags;
	/**< bitmap of  LCSC_FL_* */
	enum lws_cose_sig_types		sigtype;
	/**< 0, or sign type hint */
} lws_cose_sign_create_info_t;
```

```
struct lws_cose_sign_context *
lws_cose_sign_create(const lws_cose_sign_create_info_t *info);
```

After creating the signing context, you call `lws_cose_sign_add()` one or more
times to add algorithms and keys to sign with (since cose_sign allows multiple
recipients with the same payload signed in different ways).

```
int
lws_cose_sign_add(struct lws_cose_sign_context *csc, cose_param_t alg,
		  const lws_cose_key_t *ck);
```

The payload does not have to be provided all at once and can be passed in chunk
by chunk over time via `lws_cose_sign_payload_chunk()`.

Output is mediated via an lws CBOR output context provided in the info at
creation-time, it's only emitted during the `lws_cose_sign_payload_chunk()`
phase.  If it returns `LWS_LECPCTX_RET_AGAIN`, you must call that api again
after using the CBOR output context data and resetting its buffer by
`lws_lec_setbuf()`, so it can continue to output.

```
enum lws_lec_pctx_ret
lws_cose_sign_payload_chunk(struct lws_cose_sign_context *csc,
			    const uint8_t *in, size_t in_len);
```

Finally the signing context is destroyed.

```
void
lws_cose_sign_destroy(struct lws_cose_sign_context **csc);
```

