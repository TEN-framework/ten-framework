# `lws_system`

See `include/libwebsockets/lws-system.h` for function and object prototypes.

## System integration api

`lws_system` allows you to set a `system_ops` struct at context creation time,
which can write up some function callbacks for system integration.  The goal
is the user code calls these by getting the ops struct pointer from the
context using `lws_system_get_ops(context)` and so does not spread system
dependencies around the user code, making it directly usable on completely
different platforms.

```
typedef struct lws_system_ops {
	int (*reboot)(void);
	int (*set_clock)(lws_usec_t us);
	int (*attach)(struct lws_context *context, int tsi, lws_attach_cb_t cb,
		      lws_system_states_t state, void *opaque,
		      struct lws_attach_item **get);
} lws_system_ops_t;
```

|Item|Meaning|
|---|---|
|`(*reboot)()`|Reboot the system|
|`(*set_clock)()`|Set the system clock|
|`(*attach)()`|Request an event loop callback from another thread context|

### `reboot`

Reboots the device

### `set_clock`

Set the system clock to us-resolution Unix time in seconds

### `attach`

Request a callback from the event loop from a foreign thread.  This is used, for
example, for foreign threads to set up their event loop activity in their
callback, and eg, exit once it is done, with their event loop activity able to
continue wholly from the lws event loop thread and stack context.

## Foreign thread `attach` architecture

When lws is started, it should define an `lws_system_ops_t` at context creation
time which defines its `.attach` handler.  In the `.attach` handler
implementation, it should perform platform-specific locking around a call to
`__lws_system_attach()`, a public lws api that actually queues the callback
request and does the main work.  The platform-specific wrapper is just there to
do the locking so multiple calls from different threads to the `.attach()`
operation can't conflict.

User code can indicate it wants a callback from the lws event loop like this:

```
lws_system_get_ops(context)->attach(context, tsi, cb, state, opaque, NULL)
```

`context` is a pointer to the lws_context, `tsi` is normally 0, `cb` is the user
callback in the form  

```
void (*lws_attach_cb_t)(struct lws_context *context, int tsi, void *opaque);
```

`state` is the `lws_system` state we should have reached before performing the
callback (usually, `LWS_SYSTATE_OPERATIONAL`), and `opaque` is a user pointer that
will be passed into the callback.

`cb` will normally want to create scheduled events and set up lws network-related
activity from the event loop thread and stack context.

Once the event loop callback has been booked by calling this api, the thread and
its stack context that booked it may be freed.  It will be called back and can
continue operations from the lws event loop thread and stack context.  For that
reason, if `opaque` is needed it will usually point to something on the heap,
since the stack context active at the time the callback was booked may be long
dead by the time of the callback. 

See ./lib/system/README.md for more details.

## `lws_system` blobs

"Blobs" are arbitrary binary objects that have a total length.  Lws lets you set
them in two ways

 - "directly", by pointing to them, which has no heap implication

 - "heap", by adding one or more arbitrary chunk to a chained heap object

In the "heap" case, it can be incrementally defined and the blob doesn't all
have to be declared at once.

For read, the same api allows you to read all or part of the blob into a user
buffer. 

The following kinds of blob are defined 

|Item|Meaning|
|---|---|
|`LWS_SYSBLOB_TYPE_AUTH`|Auth-related blob 1, typically a registration token|
|`LWS_SYSBLOB_TYPE_AUTH + 1`|Auth-related blob 2, typically an auth token|
|`LWS_SYSBLOB_TYPE_CLIENT_CERT_DER`|Client cert public part|
|`LWS_SYSBLOB_TYPE_CLIENT_KEY_DER`|Client cert key part|
|`LWS_SYSBLOB_TYPE_DEVICE_SERIAL`|Arbitrary device serial number|
|`LWS_SYSBLOB_TYPE_DEVICE_FW_VERSION`|Arbitrary firmware version|
|`LWS_SYSBLOB_TYPE_DEVICE_TYPE`|Arbitrary Device Type identifier|
|`LWS_SYSBLOB_TYPE_NTP_SERVER`|String with the ntp server address (defaults to pool.ntp.org)|

### Blob handle api

Returns an object representing the blob for a particular type (listed above)

```
lws_system_blob_t *
lws_system_get_blob(struct lws_context *context, lws_system_blob_item_t type,
                    int idx);
```

### Blob Setting apis

Sets the blob to point length `len` at `ptr`.  No heap allocation is used.

```
void
lws_system_blob_direct_set(lws_system_blob_t *b, const uint8_t *ptr, size_t len);
```

Allocates and copied `len` bytes from `buf` into heap and chains it on the end of
any existing.

```
int
lws_system_blob_heap_append(lws_system_blob_t *b, const uint8_t *buf, size_t len)
```

Remove any content from the blob, freeing it if it was on the heap

```
void
lws_system_blob_heap_empty(lws_system_blob_t *b)
```

### Blob getting apis

Get the total size of the blob (ie, if on the heap, the aggreate size of all the
chunks that were appeneded)

```
size_t
lws_system_blob_get_size(lws_system_blob_t *b)
```

Copy part or all of the blob starting at offset ofs into a user buffer at buf.
`*len` should be the length of the user buffer on entry, on exit it's set to
the used extent of `buf`.  This works the same whether the bob is a direct pointer
or on the heap.

```
int
lws_system_blob_get(lws_system_blob_t *b, uint8_t *buf, size_t *len, size_t ofs)
```

If you know that the blob was handled as a single direct pointer, or a single
allocation, you can get a pointer to it without copying using this.

```
int
lws_system_blob_get_single_ptr(lws_system_blob_t *b, const uint8_t **ptr)
```

### Blob destroy api

Deallocates any heap allocation for the blob

```
void
lws_system_blob_destroy(lws_system_blob_t *b)
```


## System state and notifiers

Lws implements a state in the context that reflects the readiness of the system
for various steps leading up to normal operation.  By default it acts in a
backwards-compatible way and directly reaches the OPERATIONAL state just after
the context is created.

However other pieces of lws, and user, code may define notification handlers
that get called back when the state changes incrementally, and may veto or delay
the changes until work necessary for the new state has completed asynchronously.

The generic states defined are:

|State|Meaning|
|---|---|
|`LWS_SYSTATE_CONTEXT_CREATED`|The context was just created.|
|`LWS_SYSTATE_INITIALIZED`|The vhost protocols have been initialized|
|`LWS_SYSTATE_IFACE_COLDPLUG`|Existing network interfaces have been iterated|
|`LWS_SYSTATE_DHCP`|Network identity is available|
|`LWS_SYSTATE_TIME_VALID`|The system knows the time|
|`LWS_SYSTATE_POLICY_VALID`|If the system needs information about how to act from the net, it has it|
|`LWS_SYSTATE_REGISTERED`|The device has a registered identity|
|`LWS_SYSTATE_AUTH1`|The device identity has produced a time-limited access token|
|`LWS_SYSTATE_AUTH2`|Optional second access token for different services|
|`LWS_SYSTATE_OPERATIONAL`|The system is ready for user code to work normally|
|`LWS_SYSTATE_POLICY_INVALID`|All connections are being dropped because policy information is changing.  It will transition back to `LWS_SYSTATE_INITIALIZED` and onward to `OPERATIONAL` again afterwards with the new policy|
|`LWS_SYSTATE_CONTEXT_DESTROYING`|Context is going down and smd with it|

### Inserting a notifier

You should create an object `lws_system_notify_link_t` in non-const memory and zero it down.
Set the `notify_cb` member and the `name` member and then register it using either
`lws_system_reg_notifier()` or the `.register_notifier_list`
member of the context creation info struct to make sure it will exist early
enough to see all events.  The context creation info method takes a list of
pointers to notify_link structs ending with a NULL entry.

