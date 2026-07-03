# lws logging

# `lwsl_` logging apis

LWS has traditionally provided logging arrangements that are not indirected
through the lws context, because logging may be needed before and after the
context existence.  For that reason the original logging arrangements are
processwide.

By default the logs are emitted on stdout, but this can be overridden
using `lws_set_log_level()` and either syslog (provided by `lwsl_emit_syslog()`)
or custom log emission is possible if you point it to your own.

Currently the following log levels are defined

|name|function|release|meaning|
|---|---|---|---|
|`LLL_ERR`|`lwsl_err()`|y|Serious operation errors anyone needs to know|
|`LLL_WARN`|`lwsl_warn()`|y|Operation errors you may need to know|
|`LLL_USER`|`lws_user()`|y|Information user code wants you to know|
|`LLL_NOTICE`|`lwsl_notice()`|y|Information about what lws is doing useful for logging|
|`LLL_INFO`|`lwsl_info()`|n|Detailed information about what lws is doing|
|`LLL_DEBUG`|`lwsl_debug()`|n|Very detailed information about what lws is doing|
|`LLL_PARSER`|`lwsl_parser()`|n|Very detailed information about parsing|
|`LLL_HEADER`|`lwsl_header()`|n|Very detailed information about header processing|
|`LLL_EXT`|`lwsl_ext()`|n|Very detailed information about ws extensions|
|`LLL_CLIENT`|`lwsl_client()`|n|Very detailed information about client connections|
|`LLL_LATENCY`|`lwsl_latency()`|n|detailed latency stats|
|`LLL_THREAD`|`lwsl_thread()`|n|detailed threadpool information|

The first four log levels are built into lws even on Release builds, the others
are only built in Debug builds.

You can select between Debug and Release builds using cmake `-DCMAKE_BUILD_TYPE=`
`DEBUG` or `Release`

`lws_set_log_level()` is used to OR together the logging bitfields you want to
see emitted, only log levels that were built in can be enabled since the code for them
is just not there otherwise.

## Finegrained control of log level build

You can deviate from the default log inclusion for release / debug by overriding it
at cmake, using `LWS_LOGGING_BITFIELD_SET` and `LWS_LOGGING_BITFIELD_CLEAR`.

For example you can set `-DLWS_LOGGING_BITFIELD_SET="LLL_INFO|LLL_DEBUG"`, which will
cause those log level traces to be built in even in Release mode.  Clear works
similarly to defeat build of specific log levels.

## Object tags in lws

Commonly logging wants to refer to an object in a repeatable way, the usual way to
do this is with `%p` to print the object pointer.  But this has a couple of drawbacks,
first the same memory may be freed and reallocated for a different instance of the same
or another object, causing confusion, and second when multiple processes are allocating
objects and logging, the same address may be allocated in different process also causing
confusion.

Lws has introduced unique tag strings to refer to object identity in logging instead, these
contain various information such as a 64-bit ordinal for the group the object belongs
to that won't repeat even if reallocated to the same address (until 2^64 allocations,
anyway).

Tags are fixed at object creation time for the whole object lifetime, although in some
cases the tag may be appended to... accepted server wsis for example don't have much
information available to form the tag until they start to indicate what they want to
do.

At their simplest the tags look like this (in a log indicating creation)

```
[2020/12/27 08:49:19:2956] N:  ++ (4) [wsi|5|h2]
```

It means a wsi has been created with the tag `[wsi|5|h2]`, and after that, there are 4
active objects in the wsi group.

The corresponding object destruction log with the tag is

```
[2020/12/27 08:49:24:4226] N:  -- (3)   5.126s [wsi|5|h2]
```

it indicates the object's tag, that it lived for 5.126s and after its destruction,
there are 3 objects in its group left.

### Compound tags

If the object has bindings, the tag can reflect that, eg

```
[2020/12/27 08:49:19:4787] N:  ++ (2) [wsiSScli|6|d_h1]
[2020/12/27 08:49:19:4793] N:  ++ (2) [wsicli|6|GET/h1/httpbin.org/([wsiSScli|6|d_h1])]
```

the first log is describing a proxied SS client connection at the proxy, and the second
is a wsi bound to the SS object from the first log to do the outgoing client action.

## Tags in user code

When user code wants to refer to a tagged object like a wsi or vhost, there are helpers
that return a `const char *` containing the tag

|tag accessors|
|---|
|`lws_wsi_tag(wsi)`|
|`lws_vh_tag(vh)`|
|`lws_ss_tag(h)`|

# New logging context apis

From v4.3 on lws additionally provides wrappers that issue logs into a
"log context" object, one of these is embedded in the lws_context, lws_vhost,
wsi, ss and sspc handles.  These follow the same general approach as before, but
allow logs to be issued in "the context" of any of those objects, and to fall
back sanely if the object pointer is NULL.

The traditional process scope logs and emit management remain available as
before, and if you do not set custom log contexts, the new log apis use the
processwide log context emit and mask as before too.

Here's a summary of the differences:

|Traditional process scope logs|New log context apis|
|---|---|
|Single processwide log context|Defaults to processwide, but object can use custom log contexts|
|Single processwide emit function|Emit function per log context|
|Single processwide log mask|log mask is in log context, objects can be bound to custom log contexts at creation time|
|Require trailing `\n` in format|Trailing `\n` added if not present|
|Manual `__func__`|`__func__` added in wrapper macros automatically|
|Manual tag addition|Object tag prepended automatically|
|No hierarchy|Log contexts may refer to parent log contexts, which may prepend to child logs|
|Macros per level (eg, `lwsl_err(...)`)|Macros per object type / level (eg, `lwsl_wsi_err(wsi, ...)`)|

In addition to being able to control the emit function and log level for
individual log contexts, eg, for a particular wsi, the log functions understand
how to prepend object-specific information such as tags and `__func__`
automatically.  They also do not need a trailing `\n` in the format string.  So
the new context aware logs remove boilerplate from the logging calls while
making the log information more consistent.

So comparing this kind of logging the processwide and log context aware ways:

```
[2021/06/25 09:39:34:7050] N: [669282|wsicli|4|GET/h1/libwebsockets.org|default]: _lws_generic_transaction_completed_active_conn:  ...
```

|Type|Example code|
|---|---|
|Process scope apis|`lwsl_notice("%s: %s: mylog %d\n", __func__, lws_wsi_tag(wsi), n);`|
|New log context apis|`lwsl_wsi_notice(wsi, "mylog %d", n);`|

The log context / object-aware apis do not replace the processwide logging but
augment it, and the new apis default to use the original processwide emit
function and log mask, so the behaviours are the same.  The original processwide
log apis themselves are unchanged.

At lws_context creation time, you can set the context info `.log_cx` to a user
defined log context which is inherited by objects created in that lws_context by
default.  Vhost creation, wsi creation and ss / sspc creation all allow passing
a user log_cx to customize how logs for that object are handled.

## Using the new logging apis

This table describes the different ways to issue an ERROR verbosity log, it
works the same for info, notice, warn, etc.

|Scope|Api example|Functionality|
|---|---|---|
|Old, Processwide|lwsl_err(...)|Traditional processwide error log|
|lws_context|lwsl_cx_err(context, ...)|error log bound to lws_context|
|lws_vhost|lwsl_vhost_err(vh, ...)|error log bound to lws_vhost|
|lws_wsi|lwsl_wsi_err(wsi, ...)|error log bound to wsi|
|lws_ss|lwsl_ss_err(handle, ...)|error log bound to secure stream|

Similarly hexdumps can be bound to different log contexts

|Scope|Api example|Functionality|
|---|---|---|
|Old, Processwide|lwsl_hexdump_err(...)|Traditional processwide error hexdump|
|lws_context|lwsl_hexdump_cx_err(context, ...)|error hexdump bound to lws_context|
|lws_vhost|lwsl_hexdump_vhost_err(vh, ...)|error hexdump bound to lws_vhost|
|lws_wsi|lwsl_hexdump_wsi_err(wsi, ...)|error hexdump bound to wsi|
|lws_ss|lwsl_hexdump_ss_err(handle, ...)|error hexdump bound to secure stream|

## Creating and using custom log contexts

The log context object is public, in `libwebsockets/lws-logs.h`, currently it
is like this

```
typedef void (*lws_log_emit_t)(int level, const char *line);
typedef void (*lws_log_emit_cx_t)(struct lws_log_cx *cx, int level,
				  const char *line, size_t len);
typedef void (*lws_log_prepend_cx_t)(struct lws_log_cx *cx, void *obj,
				     char **p, char *e);
typedef void (*lws_log_use_cx_t)(struct lws_log_cx *cx, int _new);
typedef struct lws_log_cx {
	union {
		lws_log_emit_t		emit; /* legacy emit function */
		lws_log_emit_cx_t	emit_cx; /* LLLF_LOG_CONTEXT_AWARE */
	} u;
	lws_log_use_cx_t		refcount_cb;
	/**< NULL, or a function called after each change to .refcount below,
	 * this enables implementing side-effects like opening and closing
	 * log files when the first and last object binds / unbinds */
	lws_log_prepend_cx_t		prepend;
	/**< NULL, or a cb to optionally prepend a string to logs we are a
	 * parent of */
	struct lws_log_cx		*parent;
	/**< NULL, or points to log ctx we are a child of */
	void				*opaque;
	/**< ignored by lws, used to pass config to emit_cx, eg, filepath */
	void				*stg;
	/**< ignored by lws, may be used a storage by refcount_cb / emit_cx */
	uint32_t			lll_flags;
	/**< mask of log levels we want to emit in this context */
	int32_t				refcount;
	/**< refcount of objects bound to this log context */
} lws_log_cx_t;
```

The emit function is a union because the traditional logs and the old emit
functions are also implemented using the new log contexts internally.  For
new log context-aware code, you would use `.u.emit_cx` and set the flag
`LLLF_LOG_CONTEXT_AWARE` on `.lll_flags`.

Lws also exports some common emit and refcount functions so you don't have to
reinvent the wheel

|Dest|emit member|`.lll_flags`|emit|`.refcount_cb`|`.opaque`|
|---|---|---|---|---|---|
|stderr|`.u.emit`|-|`lwsl_emit_stderr`|NULL|NULL|
|file|`.u.emit_cx`|`LLLF_LOG_CONTEXT_AWARE`|`lws_log_emit_cx_file`|`lws_log_use_cx_file`|`(const char *)filepath`|

For example, a custom log context that emits to a configurable file can be
declared like this (lws exports the needed helpers already)

```
static lws_log_cx_t my_log_cx = {
        .lll_flags      = LLLF_LOG_CONTEXT_AWARE |
                          LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_USER,
        .refcount_cb    = lws_log_use_cx_file,
        .u.emit_cx      = lws_log_emit_cx_file,
        .opaque	        = "/tmp/mylogpath.log" /* also settable at runtime */
};
```

To bind the lws_context to this log context, set `log_cx` in the context
creation info struct

```
	info.log_cx = &my_log_cx;
```

### Log context hierarchy

Log contexts may also point to a parent log context... the top level log context
defines the emit function to be used, but parent log contexts are consulted by
calling their prepend function if any, to annotate logs with information from
parent levels.

### Log context prepend function

Logs contexts may define a "prepend" function callback, that knows how to
represent the object in a brief string to be prepended to other logs.  For
example the wsi-aware log context layer knows how to provide the wsi tag
when called.

Prepend functions should add `:<space>` after their output, if any, since these
will appear before the start of other logs.

### Log context opaque member

The `.opaque` member is available for passing in configuration to the emit and
refcount_cb members.  Lws does not use this itself at all.

### Log context refcounting

An expected use for custom log contexts is emitting to a specific file, and
then binding one or more objects to that log context.  Since it's too expensive
to keep opening and closing the output file per log, it means we need to know
when we bind to the first object and unbind from the last, so we can keep the
file handle open.

For this reason the log contexts have a refcount, and an opaque `void *stg`
availble for the emit and refounct_cb to use how they see fit, eg, for storing
the output log file descriptor.
