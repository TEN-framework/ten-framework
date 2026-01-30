Programmers' Guide
==================

Architecture
------------

The most notable point in nghttp2 library architecture is it does not
perform any I/O.  nghttp2 only performs HTTP/2 protocol stuff based on
input byte strings.  It will call callback functions set by
applications while processing input.  The output of nghttp2 is just
byte string.  An application is responsible to send these output to
the remote peer.  The callback functions may be called while producing
output.

Not doing I/O makes embedding nghttp2 library in the existing code
base very easy.  Usually, the existing applications have its own I/O
event loops.  It is very hard to use nghttp2 in that situation if
nghttp2 does its own I/O.  It also makes light weight language wrapper
for nghttp2 easy with the same reason.  The down side is that an
application author has to write more code to write complete
application using nghttp2.  This is especially true for simple "toy"
application.  For the real applications, however, this is not the
case.  This is because you probably want to support HTTP/1 which
nghttp2 does not provide, and to do that, you will need to write your
own HTTP/1 stack or use existing third-party library, and bind them
together with nghttp2 and I/O event loop.  In this point, not
performing I/O in nghttp2 has more point than doing it.

The primary object that an application uses is :type:`nghttp2_session`
object, which is opaque struct and its details are hidden in order to
ensure the upgrading its internal architecture without breaking the
backward compatibility.  An application can set callbacks to
:type:`nghttp2_session` object through the dedicated object and
functions, and it also interacts with it via many API function calls.

An application can create as many :type:`nghttp2_session` object as it
wants.  But single :type:`nghttp2_session` object must be used by a
single thread at the same time.  This is not so hard to enforce since
most event-based architecture applications use is single thread per
core, and handling one connection I/O is done by single thread.

To feed input to :type:`nghttp2_session` object, one can use
`nghttp2_session_recv()` or `nghttp2_session_mem_recv2()` functions.
They behave similarly, and the difference is that
`nghttp2_session_recv()` will use :type:`nghttp2_read_callback` to get
input.  On the other hand, `nghttp2_session_mem_recv2()` will take
input as its parameter.  If in doubt, use
`nghttp2_session_mem_recv2()` since it is simpler, and could be faster
since it avoids calling callback function.

To get output from :type:`nghttp2_session` object, one can use
`nghttp2_session_send()` or `nghttp2_session_mem_send2()`.  The
difference between them is that the former uses
:type:`nghttp2_send_callback` to pass output to an application.  On
the other hand, the latter returns the output to the caller.  If in
doubt, use `nghttp2_session_mem_send2()` since it is simpler.  But
`nghttp2_session_send()` might be easier to use if the output buffer
an application has is fixed sized.

In general, an application should call `nghttp2_session_mem_send2()`
when it gets input from underlying connection.  Since there is great
chance to get something pushed into transmission queue while the call
of `nghttp2_session_mem_send2()`, it is recommended to call
`nghttp2_session_mem_recv2()` after `nghttp2_session_mem_send2()`.

There is a question when we are safe to close HTTP/2 session without
waiting for the closure of underlying connection.  We offer 2 API
calls for this: `nghttp2_session_want_read()` and
`nghttp2_session_want_write()`.  If they both return 0, application
can destroy :type:`nghttp2_session`, and then close the underlying
connection.  But make sure that the buffered output has been
transmitted to the peer before closing the connection when
`nghttp2_session_mem_send2()` is used, since
`nghttp2_session_want_write()` does not take into account the
transmission of the buffered data outside of :type:`nghttp2_session`.

Includes
--------

To use the public APIs, include ``nghttp2/nghttp2.h``::

    #include <nghttp2/nghttp2.h>

The header files are also available online: :doc:`nghttp2.h` and
:doc:`nghttp2ver.h`.

Remarks
-------

Do not call `nghttp2_session_send()`, `nghttp2_session_mem_send2()`,
`nghttp2_session_recv()` or `nghttp2_session_mem_recv2()` from the
nghttp2 callback functions directly or indirectly. It will lead to the
crash.  You can submit requests or frames in the callbacks then call
these functions outside the callbacks.

`nghttp2_session_send()` and `nghttp2_session_mem_send2()` send first
24 bytes of client magic string (MAGIC)
(:macro:`NGHTTP2_CLIENT_MAGIC`) on client configuration.  The
applications are responsible to send SETTINGS frame as part of
connection preface using `nghttp2_submit_settings()`.  Similarly,
`nghttp2_session_recv()` and `nghttp2_session_mem_recv2()` consume
MAGIC on server configuration unless
`nghttp2_option_set_no_recv_client_magic()` is used with nonzero
option value.

.. _http-messaging:

HTTP Messaging
--------------

By default, nghttp2 library checks HTTP messaging rules described in
`HTTP/2 specification, section 8
<https://tools.ietf.org/html/rfc7540#section-8>`_.  Everything
described in that section is not validated however.  We briefly
describe what the library does in this area.  In the following
description, without loss of generality we omit CONTINUATION frame
since they must follow HEADERS frame and are processed atomically.  In
other words, they are just one big HEADERS frame.  To disable these
validations, use `nghttp2_option_set_no_http_messaging()`.  Please
note that disabling this feature does not change the fundamental
client and server model of HTTP.  That is, even if the validation is
disabled, only client can send requests.

For HTTP request, including those carried by PUSH_PROMISE, HTTP
message starts with one HEADERS frame containing request headers.  It
is followed by zero or more DATA frames containing request body, which
is followed by zero or one HEADERS containing trailer headers.  The
request headers must include ":scheme", ":method" and ":path" pseudo
header fields unless ":method" is not "CONNECT".  ":authority" is
optional, but nghttp2 requires either ":authority" or "Host" header
field must be present.  If ":method" is "CONNECT", the request headers
must include ":method" and ":authority" and must omit ":scheme" and
":path".

For HTTP response, HTTP message starts with zero or more HEADERS
frames containing non-final response (status code 1xx).  They are
followed by one HEADERS frame containing final response headers
(non-1xx).  It is followed by zero or more DATA frames containing
response body, which is followed by zero or one HEADERS containing
trailer headers.  The non-final and final response headers must
contain ":status" pseudo header field containing 3 digits only.

All request and response headers must include exactly one valid value
for each pseudo header field.  Additionally nghttp2 requires all
request headers must not include more than one "Host" header field.

HTTP/2 prohibits connection-specific header fields.  The following
header fields must not appear: "Connection", "Keep-Alive",
"Proxy-Connection", "Transfer-Encoding" and "Upgrade".  Additionally,
"TE" header field must not include any value other than "trailers".

Each header field name and value must obey the field-name and
field-value production rules described in `RFC 7230, section
3.2. <https://tools.ietf.org/html/rfc7230#section-3.2>`_.
Additionally, all field name must be lower cased.  The invalid header
fields are treated as stream error, and that stream is reset.  If
application wants to treat these headers in their own way, use
`nghttp2_on_invalid_header_callback
<https://nghttp2.org/documentation/types.html#c.nghttp2_on_invalid_header_callback>`_.

For "http" or "https" URIs, ":path" pseudo header fields must start
with "/".  The only exception is OPTIONS request, in that case, "*" is
allowed in ":path" pseudo header field to represent system-wide
OPTIONS request.

With the above validations, nghttp2 library guarantees that header
field name passed to `nghttp2_on_header_callback()` is not empty.
Also required pseudo headers are all present and not empty.

nghttp2 enforces "Content-Length" validation as well.  All request or
response headers must not contain more than one "Content-Length"
header field.  If "Content-Length" header field is present, it must be
parsed as 64 bit signed integer.  The sum of data length in the
following DATA frames must match with the number in "Content-Length"
header field if it is present (this does not include padding bytes).

RFC 7230 says that server must not send "Content-Length" in any
response with 1xx, and 204 status code.  It also says that
"Content-Length" is not allowed in any response with 200 status code
to a CONNECT request.  nghttp2 enforces them as well.

Any deviation results in stream error of type PROTOCOL_ERROR.  If
error is found in PUSH_PROMISE frame, stream error is raised against
promised stream.

The order of transmission of the HTTP/2 frames
----------------------------------------------

This section describes the internals of libnghttp2 about the
scheduling of transmission of HTTP/2 frames.  This is pretty much
internal stuff, so the details could change in the future versions of
the library.

libnghttp2 categorizes HTTP/2 frames into 4 categories: urgent,
regular, syn_stream, and data in the order of higher priority.

The urgent category includes PING and SETTINGS.  They are sent with
highest priority.  The order inside the category is FIFO.

The regular category includes frames other than PING, SETTINGS, DATA,
and HEADERS which does not create stream (which counts toward
concurrent stream limit).  The order inside the category is FIFO.

The syn_stream category includes HEADERS frame which creates stream,
that counts toward the concurrent stream limit.

The data category includes DATA frame, and the scheduling among DATA
frames are determined by HTTP/2 dependency tree.

If the application wants to send frames in the specific order, and the
default transmission order does not fit, it has to schedule frames by
itself using the callbacks (e.g.,
:type:`nghttp2_on_frame_send_callback`).

RST_STREAM has special side effect when it is submitted by
`nghttp2_submit_rst_stream()`.  It cancels all pending HEADERS and
DATA frames whose stream ID matches the one in the RST_STREAM frame.
This may cause unexpected behaviour for the application in some cases.
For example, suppose that application wants to send RST_STREAM after
sending response HEADERS and DATA.  Because of the reason we mentioned
above, the following code does not work:

.. code-block:: c

    nghttp2_submit_response2(...)
    nghttp2_submit_rst_stream(...)

RST_STREAM cancels HEADERS (and DATA), and just RST_STREAM is sent.
The correct way is use :type:`nghttp2_on_frame_send_callback`, and
after HEADERS and DATA frames are sent, issue
`nghttp2_submit_rst_stream()`.  FYI,
:type:`nghttp2_on_frame_not_send_callback` tells you why frames are
not sent.

Implement user defined HTTP/2 non-critical extensions
-----------------------------------------------------

As of nghttp2 v1.8.0, we have added HTTP/2 non-critical extension
framework, which lets application send and receive user defined custom
HTTP/2 non-critical extension frames.  nghttp2 also offers built-in
functionality to send and receive official HTTP/2 extension frames
(e.g., ALTSVC frame).  For these built-in handler, refer to the next
section.

To send extension frame, use `nghttp2_submit_extension()`, and
implement :type:`nghttp2_pack_extension_callback`.  The callback
implements how to encode data into wire format.  The callback must be
set to :type:`nghttp2_session_callbacks` using
`nghttp2_session_callbacks_set_pack_extension_callback()`.

For example, we will illustrate how to send `ALTSVC
<https://tools.ietf.org/html/rfc7838>`_ frame.

.. code-block:: c

    typedef struct {
      const char *origin;
      const char *field;
    } alt_svc;

    nghttp2_ssize pack_extension_callback(nghttp2_session *session, uint8_t *buf,
                                          size_t len, const nghttp2_frame *frame,
                                          void *user_data) {
      const alt_svc *altsvc = (const alt_svc *)frame->ext.payload;
      size_t originlen = strlen(altsvc->origin);
      size_t fieldlen = strlen(altsvc->field);

      uint8_t *p;

      if (len < 2 + originlen + fieldlen || originlen > 0xffff) {
        return NGHTTP2_ERR_CANCEL;
      }

      p = buf;
      *p++ = originlen >> 8;
      *p++ = originlen & 0xff;
      memcpy(p, altsvc->origin, originlen);
      p += originlen;
      memcpy(p, altsvc->field, fieldlen);
      p += fieldlen;

      return p - buf;
    }

This implements :type:`nghttp2_pack_extension_callback`.  We have to
set this callback to :type:`nghttp2_session_callbacks`:

.. code-block:: c

    nghttp2_session_callbacks_set_pack_extension_callback(
        callbacks, pack_extension_callback);

To send ALTSVC frame, call `nghttp2_submit_extension()`:

.. code-block:: c

  static const alt_svc altsvc = {"example.com", "h2=\":8000\""};

  nghttp2_submit_extension(session, 0xa, NGHTTP2_FLAG_NONE, 0,
                           (void *)&altsvc);

Notice that ALTSVC is use frame type ``0xa``.

To receive extension frames, implement 2 callbacks:
:type:`nghttp2_unpack_extension_callback` and
:type:`nghttp2_on_extension_chunk_recv_callback`.
:type:`nghttp2_unpack_extension_callback` implements the way how to
decode wire format.  :type:`nghttp2_on_extension_chunk_recv_callback`
implements how to buffer the incoming extension payload.  These
callbacks must be set using
`nghttp2_session_callbacks_set_unpack_extension_callback()` and
`nghttp2_session_callbacks_set_on_extension_chunk_recv_callback()`
respectively.  The application also must tell the library which
extension frame type it is willing to receive using
`nghttp2_option_set_user_recv_extension_type()`.  Note that the
application has to create :type:`nghttp2_option` object for that
purpose, and initialize session with it.

We use ALTSVC again to illustrate how to receive extension frames.  We
use different ``alt_svc`` struct than the previous one.

First implement 2 callbacks.  We store incoming ALTSVC payload to
global variable ``altsvc_buffer``.  Don't do this in production code
since this is not thread safe:

.. code-block:: c

    typedef struct {
      const uint8_t *origin;
      size_t originlen;
      const uint8_t *field;
      size_t fieldlen;
    } alt_svc;

    /* buffers incoming ALTSVC payload */
    uint8_t altsvc_buffer[4096];
    /* The length of byte written to altsvc_buffer */
    size_t altsvc_bufferlen = 0;

    int on_extension_chunk_recv_callback(nghttp2_session *session,
                                         const nghttp2_frame_hd *hd,
                                         const uint8_t *data, size_t len,
                                         void *user_data) {
      if (sizeof(altsvc_buffer) < altsvc_bufferlen + len) {
        altsvc_bufferlen = 0;
        return NGHTTP2_ERR_CANCEL;
      }

      memcpy(altsvc_buffer + altsvc_bufferlen, data, len);
      altsvc_bufferlen += len;

      return 0;
    }

    int unpack_extension_callback(nghttp2_session *session, void **payload,
                                  const nghttp2_frame_hd *hd, void *user_data) {
      uint8_t *origin, *field;
      size_t originlen, fieldlen;
      uint8_t *p, *end;
      alt_svc *altsvc;

      if (altsvc_bufferlen < 2) {
        altsvc_bufferlen = 0;
        return NGHTTP2_ERR_CANCEL;
      }

      p = altsvc_buffer;
      end = altsvc_buffer + altsvc_bufferlen;

      originlen = ((*p) << 8) + *(p + 1);
      p += 2;

      if (p + originlen > end) {
        altsvc_bufferlen = 0;
        return NGHTTP2_ERR_CANCEL;
      }

      origin = p;
      field = p + originlen;
      fieldlen = end - field;

      altsvc = (alt_svc *)malloc(sizeof(alt_svc));
      altsvc->origin = origin;
      altsvc->originlen = originlen;
      altsvc->field = field;
      altsvc->fieldlen = fieldlen;

      *payload = altsvc;

      altsvc_bufferlen = 0;

      return 0;
    }

Set these callbacks to :type:`nghttp2_session_callbacks`:

.. code-block:: c

    nghttp2_session_callbacks_set_on_extension_chunk_recv_callback(
        callbacks, on_extension_chunk_recv_callback);

    nghttp2_session_callbacks_set_unpack_extension_callback(
        callbacks, unpack_extension_callback);


In ``unpack_extension_callback`` above, we set unpacked ``alt_svc``
object to ``*payload``.  nghttp2 library then, calls
:type:`nghttp2_on_frame_recv_callback`, and ``*payload`` will be
available as ``frame->ext.payload``:

.. code-block:: c

    int on_frame_recv_callback(nghttp2_session *session,
                               const nghttp2_frame *frame, void *user_data) {

      switch (frame->hd.type) {
      ...
      case 0xa: {
        alt_svc *altsvc = (alt_svc *)frame->ext.payload;
        fprintf(stderr, "ALTSVC frame received\n");
        fprintf(stderr, " origin: %.*s\n", (int)altsvc->originlen, altsvc->origin);
        fprintf(stderr, " field : %.*s\n", (int)altsvc->fieldlen, altsvc->field);
        free(altsvc);
        break;
      }
      }

      return 0;
    }

Finally, application should set the extension frame types it is
willing to receive:

.. code-block:: c

    nghttp2_option_set_user_recv_extension_type(option, 0xa);

The :type:`nghttp2_option` must be set to :type:`nghttp2_session` on
its creation:

.. code-block:: c

    nghttp2_session_client_new2(&session, callbacks, user_data, option);

How to use built-in HTTP/2 extension frame handlers
---------------------------------------------------

In the previous section, we talked about the user defined HTTP/2
extension frames.  In this section, we talk about HTTP/2 extension
frame support built into nghttp2 library.

As of this writing, nghttp2 supports ALTSVC extension frame.  To send
ALTSVC frame, use `nghttp2_submit_altsvc()` function.

To receive ALTSVC frame through built-in functionality, application
has to use `nghttp2_option_set_builtin_recv_extension_type()` to
indicate the willingness of receiving ALTSVC frame:

.. code-block:: c

    nghttp2_option_set_builtin_recv_extension_type(option, NGHTTP2_ALTSVC);

This is very similar to the case when we used to receive user defined
frames.

If the same frame type is set using
`nghttp2_option_set_builtin_recv_extension_type()` and
`nghttp2_option_set_user_recv_extension_type()`, the latter takes
precedence.  Application can implement its own frame handler rather
than using built-in handler.

The :type:`nghttp2_option` must be set to :type:`nghttp2_session` on
its creation, like so:

.. code-block:: c

    nghttp2_session_client_new2(&session, callbacks, user_data, option);

When ALTSVC is received, :type:`nghttp2_on_frame_recv_callback` will
be called as usual.

Stream priorities
-----------------

The stream prioritization scheme described in :rfc:`7540`, which has
been formally deprecated by :rfc:`9113`, has been removed.  An
application is advised to send
:enum:`nghttp2_settings_id.NGHTTP2_SETTINGS_NO_RFC7540_PRIORITIES` of
value of 1 via `nghttp2_submit_settings()`, and migrate to
:rfc:`9218`.

The sender of this settings value disables :rfc:`7540` priorities, and
instead it enables :rfc:`9218` Extensible Prioritization Scheme.  This
new prioritization scheme has 2 methods to convey the stream
priorities to a remote endpoint: Priority header field and
PRIORITY_UPDATE frame.  nghttp2 supports both methods.  In order to
receive and process PRIORITY_UPDATE frame, server has to call
`nghttp2_option_set_builtin_recv_extension_type()` with
NGHTTP2_PRIORITY_UPDATE as type argument (see the above section), and
pass the option to `nghttp2_session_server_new2()` or
`nghttp2_session_server_new3()` to create a server session.  Client
can send Priority header field via `nghttp2_submit_request2()`.  It
can also send PRIORITY_UPDATE frame via
`nghttp2_submit_priority_update()`.  Server processes Priority header
field in a request header field and updates the stream priority unless
HTTP messaging rule enforcement is disabled (see
`nghttp2_option_set_no_http_messaging()`).
