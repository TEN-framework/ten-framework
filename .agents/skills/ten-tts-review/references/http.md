# HTTP TTS extension review guide

Load this guide only when the extension inherits `AsyncTTS2HttpExtension`.

## Ownership boundary

The HTTP base normally owns input queuing, request state, audio start/end events,
metrics, dump lifecycle, and terminal cleanup. The extension normally supplies
configuration, client construction, vendor identity/audio format, and an HTTP
client that yields response events.

First map every changed method to extension-owned or inherited behavior. Treat an
override of `request_tts`, `cancel_tts`, lifecycle hooks, or audio-end helpers as a
larger risk surface: verify why the override is needed and whether it duplicates
or bypasses base guarantees. Do not ask for duplicated lifecycle code when the
base implementation is inherited unchanged.

## HTTP client stream contract

Verify:

- The client checks HTTP status and content type before yielding audio. Error JSON,
  HTML, and authentication responses never become PCM frames.
- Response bytes are streamed incrementally. Calls that buffer the full body,
  await complete synthesis, or parse a container before yielding do not silently
  defeat time-to-first-audio requirements.
- Every request produces an unambiguous sequence of response chunks followed by
  one end or error condition. Iterator exhaustion without an explicit end is
  handled by the inherited contract rather than leaving the request open.
- Empty network chunks do not trigger audio start or TTFB.
- Conversion to the PCM format declared by the extension occurs before chunks are
  yielded to the base. Stateful conversion preserves partial samples across HTTP
  chunk boundaries.
- Authentication/config failures are distinguishable from transient request,
  timeout, and transport failures so the base receives the correct error class.
- Connect, response-header, first-byte, inter-chunk, and total-request timeouts are
  intentional. Cancellation interrupts the active stream promptly.
- Response/session objects close on success, error, cancellation, and shutdown.

## Configuration and client construction

Verify:

- `create_config()` parses and validates the extension schema without mutating a
  shared parameters dictionary across requests.
- `create_client()` receives a validated config and does not log secrets or place
  them in URLs unnecessarily.
- Internal parameters are removed from the vendor payload without accidentally
  deleting supported free-form vendor options.
- The configured output format, sample rate, channels, and sample width match what
  the HTTP client actually yields.
- Configuration updates cannot leave a stale client using old credentials or
  audio settings.
- Client metadata returned to the base is request-safe and redacted.

## Cancellation and tests

The client cancellation primitive must stop the active response body, not merely
set a flag after all bytes have already been buffered. Cleanup must be idempotent
because cancellation can race with normal iterator completion.

Extension-local tests should drive a fake HTTP client through:

- chunked success and stream exhaustion;
- non-2xx response before audio;
- error after partial PCM;
- empty chunks and an empty body;
- cancellation during first-byte wait and during streaming;
- PCM conversion split across HTTP chunks;
- inherited lifecycle behavior without reimplementing the base.

