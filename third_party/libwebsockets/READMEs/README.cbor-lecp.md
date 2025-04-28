# RFC8949 CBOR Stream Parsing and Writing

|||
|---|---|---|
|cmake| `LWS_WITH_CBOR`, `LWS_WITH_CBOR_FLOAT`|
|Header| ./include/libwebsockets/lws-lecp.h|
|api-test| ./minimal-examples/api-tests/api-test-lecp/|
|test app| ./test-apps/test-lecp.c -> libwebsockets-test-lecp|

LECP is the RFC8949 CBOR stream parsing counterpart to LEJP for JSON.

## Features

 - Completely immune to input fragmentation, give it any size blocks of CBOR as
   they become available; 1 byte, or 100K at a time give identical parsing
   results
 - Input chunks discarded as they are parsed, whole CBOR never needed in memory
 - Nonrecursive, fixed stack usage of a few dozen bytes
 - No heap allocations at all, just requires ~500 byte context usually on
   caller stack
 - Creates callbacks to a user-provided handler as members are parsed out
 - No payload size limit, supports huge / endless strings or blobs bigger than
   system memory
 - Collates utf-8 text and blob payloads into a 250-byte chunk buffer for ease
   of access
 - Write apis don't use any heap allocations or recursion either
 - Write apis use an explicit context with its own lifecycle, and printf style
   vaargs including sized blobs, C strings, double, int, unsigned long etc
 - Completely immune to output fragmentation, supports huge strings and blobs
   into small buffers, api returns to indicates unfinished if it needs to be
   called again to continue; 1 byte or 100K output buffer give same results
 - Write apis completely fill available buffer and if unfinished, continues
   into same or different buffer when called again with same args; no
   requirement for subsequent calls to be done sequentially or even from same
   function

## Type limits

CBOR allows negative integers of up to 64 bits, these do not fit into a `uint64_t`.
LECP has a union for numbers that includes the types `uint64_t` and `int64_t`,
but it does not separately handle negative integers.  Only -2^63.. 2^64 -1 can
be handled by the C types, the oversize negative numbers wrap and should be
avoided.

## Floating point support

Floats are handled using the IEEE memory format, it means they can be parsed
from the CBOR without needing any floating point support in the build.  If
floating point is available, you can also enable `LWS_WITH_CBOR_FLOAT` and
a `float` and `double` types are available in the number item union.  Otherwise
these are handled as `ctx->item.u.u32` and `ctx->item.u.u64` union members.

Half-float (16-bit) is defined in CBOR and always handled as a `uint16_t`
number union member `ctx->item.u.hf`.

## Callback reasons

The user callback does not have to handle any callbacks, it only needs to
process the data for the ones it is interested in.

|Callback reason|CBOR structure|Associated data|
|---|---|---|
|`LECPCB_CONSTRUCTED`|Created the parse context||
|`LECPCB_DESTRUCTED`|Destroyed the parse context||
|`LECPCB_COMPLETE`|The parsing completed OK||
|`LECPCB_FAILED`|The parsing failed||
|`LECPCB_VAL_TRUE`|boolean true||
|`LECPCB_VAL_FALSE`|boolean false||
|`LECPCB_VAL_NULL`|explicit NULL||
|`LECPCB_VAL_NUM_INT`|signed integer|`ctx->item.u.i64`|
|`LECPCB_VAL_STR_START`|A UTF-8 string is starting||
|`LECPCB_VAL_STR_CHUNK`|The next string chunk|`ctx->npos` bytes in `ctx->buf`|
|`LECPCB_VAL_STR_END`|The last string chunk|`ctx->npos` bytes in `ctx->buf`|
|`LECPCB_ARRAY_START`|An array is starting||
|`LECPCB_ARRAY_END`|An array has ended||
|`LECPCB_OBJECT_START`|A CBOR map is starting||
|`LECPCB_OBJECT_END`|A CBOR map has ended||
|`LECPCB_TAG_START`|The following data has a tag index|`ctx->item.u.u64`|
|`LECPCB_TAG_END`|The end of the data referenced by the last tag||
|`LECPCB_VAL_NUM_UINT`|Unsigned integer|`ctx->item.u.u64`|
|`LECPCB_VAL_UNDEFINED`|CBOR undefined||
|`LECPCB_VAL_FLOAT16`|half-float available as host-endian `uint16_t`|`ctx->item.u.hf`|
|`LECPCB_VAL_FLOAT32`|`float` (`uint32_t` if no float support) available|`ctx->item.u.f`|
|`LECPCB_VAL_FLOAT64`|`double` (`uint64_t` if no float support) available|`ctx->item.u.d`|
|`LECPCB_VAL_SIMPLE`|CBOR simple|`ctx->item.u.u64`|
|`LECPCB_VAL_BLOB_START`|A binary blob is starting||
|`LECPCB_VAL_BLOB_CHUNK`|The next blob chunk|`ctx->npos` bytes in `ctx->buf`|
|`LECPCB_VAL_BLOB_END`|The last blob chunk|`ctx->npos` bytes in `ctx->buf`|
|`LECPCB_ARRAY_ITEM_START`|A logical item in an array is starting|
|`LCEPDB_ARRAY_ITEM_END`|A logical item in an array has completed|

## CBOR indeterminite lengths

Indeterminite lengths are supported, but are concealed in the parser as far as
possible, the CBOR lengths or its indeterminacy are not exposed in the callback
interface at all, just chunks of data that may be the start, the middle, or the
end.

## Handling CBOR UTF-8 strings and blobs

When a string or blob is parsed, an advisory callback of `LECPCB_VAL_STR_START` or
`LECPCB_VAL_BLOB_START` occurs first.  The `_STR_` callbacks indicate the
content is a CBOR UTF-8 string, `_BLOB_` indicates it is binary data.

Strings or blobs may have indeterminite length, but if so, they are composed
of logical chunks which must have known lengths.  When the `_START` callback
occurs, the logical length either of the whole string, or of the sub-chunk if
indeterminite length, can be found in `ctx->item.u.u64`.

Payload is collated into `ctx->buf[]`, the valid length is in `ctx->npos`.

For short strings or blobs where the length is known, the whole payload is
delivered in a single `LECPCB_VAL_STR_END` or `LECPCB_VAL_BLOB_END` callback.

For payloads larger than the size of `ctx->buf[]`, `LECPCB_VAL_STR_CHUNK` or
`LECPCB_VAL_BLOB_CHUNK` callbacks occur delivering each sequential bufferload.
If the CBOR indicates the total length, the last chunk is delievered in a
`LECPCB_VAL_STR_END` or `LECPCB_VAL_BLOB_END`.

If the CBOR indicates the string end after the chunk, a zero-length `..._END`
callback is provided.

## Handling CBOR tags

CBOR tags are exposed as `LECPCB_TAG_START` and `LECPCB_TAG_END` pairs, at
the `_START` callback the tag index is available in `ctx->item.u.u64`.

## CBOR maps

You can check if you are on the "key" part of a map "key:value" pair using the
helper api `lecp_parse_map_is_key(ctx)`.

## Parsing paths

LECP maintains a "parsing path" in `ctx->path` that represents the context of
the callback events.  As a convenience, at LECP context creation time, you can
pass in an array of path strings you want to match on, and have any match
checkable in the callback using `ctx->path_match`, it's 0 if no active match,
or the match index from your path array starting from 1 for the first entry.

|CBOR element|Representation in path|
|---|---|
|CBOR Array|`[]`|
|CBOR Map|`.`|
|CBOR Map entry key string|`keystring`|

## Accessing raw CBOR subtrees

Some CBOR usages like COSE require access to selected raw CBOR from the input
stream.  `lecp_parse_report_raw(ctx, on)` lets you turn on and off buffering of
raw CBOR and reporting it in the parse callback with `LECPCB_LITERAL_CBOR`
callbacks.  The callbacks mean the temp buffer `ctx->cbor[]` has `ctx->cbor_pos`
bytes of raw CBOR available in it.  Callbacks are triggered when the buffer
fills, or reporting is turned off and the buffer has something in it.

By turning the reporting on and off according to the outer CBOR parsing state,
it's possible to get exactly the raw CBOR subtree that's needed.

Capturing and reporting the raw CBOR does not change that the same CBOR is being
passed to the parser as usual as well.

## Comparison with LEJP (JSON parser)

LECP is based on the same principles as LEJP and shares most of the callbacks.
The major differences:

 - LEJP value callbacks all appear in `ctx->buf[]`, ie, floating-point is
   provided to the callback in ascii form like `"1.0"`.  CBOR provides a more
   strict typing system, and the different type values are provided either in
   `ctx->buf[]` for blobs or utf-8 text strtings, or the `item.u` union for
   converted types, with additional callback reasons specific to each type.

 - CBOR "maps" use `_OBJECT_START` and `_END` parsing callbacks around the
   key / value pairs.  LEJP has a special callback type `PAIR_NAME` for the
   key string / integer, but in LECP these are provided as generic callbacks
   dependent on type, ie, generic string callbacks or integer ones, and the
   value part is represented according to whatever comes.


# Writing CBOR

CBOR is written into a `lws_lec_pctx_t` object that has been initialized to
point to an output buffer of a specified size, using printf type formatting.

Output is paused if the buffer fills, and the write api may be called again
later with the same context object, to resume emitting to the same or different
buffer.

This allows bufferloads of encoded CBOR to be produced on demand, it's designed
to fit usage in WRITEABLE callbacks and Secure Streams tx() callbacks where the
buffer size for one packet is already fixed.

CBOR array and map lengths are deduced from the format string, as is whether to
use indeterminite length formatting or not.  For indeterminite text or binary
strings, a container of < > 

|Format|Arg(s)|Meaning|
|---|---|---|
|`123`||unsigned literal number|
|`-123`||signed literal number|
|`%u`|`unsigned int`|number|
|`%lu`|`unsigned long int`|number|
|`%llu`|`unsigned long long int`|number|
|`%d`|`signed int`|number|
|`%ld`|`signed long int`|number|
|`%lld`|`signed long long int`|number|
|`%f`|`double`|floating point number|
|`123(...)`||literal tag and scope|
|`%t(...)`|`unsigned int`|tag and scope|
|`%lt(...)`|`unsigned long int`|tag and scope|
|`%llt(...)`|`unsigned long long int`|tag and scope|
|`[...]`||Array (fixed len if `]` in same format string)|
|`{...}`||Map (fixed len if `}` in same format string)|
|`<t...>`||Container for indeterminite text string frags|
|`<b...>`||Container for indeterminite binary string frags|
|`'string'`||Literal text of known length|
|`%s`|`const char *`|NUL-terminated string|
|`%.*s`|`int`, `const char *`|length-specified string|
|`%.*b`|`int`, `const uint8_t *`|length-specified binary|
|`:`||separator between Map items (a:b)|
|`,`||separator between Map pairs or array items|

Backslash is used as an escape in `'...'` literal strings, so `'\\'` represents
a string consisting of a single backslash, and `'\''` a string consisting of a
single single-quote.

For integers, various natural C types are available, but in all cases, the
number is represented in CBOR using the smallest valid way based on its value,
the long or long-long modifiers just apply to the expected C type in the args.

For floats, the C argument is always expected to be a `double` type following
C type promotion, but again it is represented in CBOR using the smallest valid
way based on value, half-floats are used for NaN / Infinity and where possible
for values like 0.0 and -1.0.

## Examples

### Literal ints

```
	uint8_t buf[128];
	lws_lec_pctx_t cbw;

	lws_lec_init(&cbw, buf, sizeof(buf));
	lws_lec_printf(ctx, "-1");
```
|||
|---|---|
|Return| `LWS_LECPCTX_RET_FINISHED`|
|`ctx->used`|1|
|`buf[]`|20|

### Dynamic ints

```
	uint8_t buf[128];
	lws_lec_pctx_t cbw;
	int n = -1; /* could be long */

	lws_lec_init(&cbw, buf, sizeof(buf));
	lws_lec_printf(ctx, "%d", n); /* use %ld for long */
```
|||
|---|---|
|Return| `LWS_LECPCTX_RET_FINISHED`|
|`ctx->used`|1|
|`buf[]`|20|

### Maps, arrays and dynamic ints

```
	...
	int args[3] = { 1, 2, 3 };

	lws_lec_printf(ctx, "{'a':%d,'b':[%d,%d]}", args[0], args[1], args[2]);
```

|||
|---|---|
|Return| `LWS_LECPCTX_RET_FINISHED`|
|`ctx->used`|9|
|`buf[]`|A2 61 61 01 61 62 82 02 03|

### String longer than the buffer

Using `%s` and the same string as an arg gives same results

```
	uint8_t buf[16];
	lws_lec_pctx_t cbw;

	lws_lec_init(&cbw, buf, sizeof(buf));
	lws_lec_printf(ctx, "'A literal string > one buf'");
	/* not required to be in same function context or same buf,
	 * but the string must remain the same */
	lws_lec_setbuf(&cbw, buf, sizeof(buf));
	lws_lec_printf(ctx, "'A literal string > one buf'");
```

First call

|||
|---|---|
|Return| `LWS_LECPCTX_RET_AGAIN`|
|`ctx->used`|16|
|`buf[]`|78 1A 41 20 6C 69 74 65 72 61 6C 20 73 74 72 69|

Second call

|||
|---|---|
|Return| `LWS_LECPCTX_RET_FINISHED`|
|`ctx->used`|12|
|`buf[]`|6E 67 20 3E 20 6F 6E 65 20 62 75 66|

### Binary blob longer than the buffer

```
	uint8_t buf[16], blob[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 };
	lws_lec_pctx_t cbw;

	lws_lec_init(&cbw, buf, sizeof(buf));
	lws_lec_printf(ctx, "%.*b", (int)sizeof(blob), blob);
	/* not required to be in same function context or same buf,
	 * but the length and blob must remain the same */
	lws_lec_setbuf(&cbw, buf, sizeof(buf));
	lws_lec_printf(ctx, "%.*b", (int)sizeof(blob), blob);
```

First call

|||
|---|---|
|Return| `LWS_LECPCTX_RET_AGAIN`|
|`ctx->used`|16|
|`buf[]`|52 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F|

Second call

|||
|---|---|
|Return| `LWS_LECPCTX_RET_FINISHED`|
|`ctx->used`|3|
|`buf[]`|10 11 12|
