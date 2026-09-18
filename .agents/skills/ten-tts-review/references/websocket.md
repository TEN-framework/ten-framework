# WebSocket and custom TTS extension review guide

Load this guide when the extension inherits `AsyncTTS2BaseExtension` directly or
owns a comparable long-lived/custom transport. These extensions own substantially
more lifecycle and concurrency behavior than HTTP-base extensions.

## Connection and protocol lifecycle

Trace connect, authenticate/handshake, send text, finalize, receive audio, vendor
end, error, reconnect, cancel, and close.

Verify:

- The vendor's endpoint, event names, encoding, request/finalize messages, and
  close semantics match the actual protocol.
- Connection state changes reflect usable transport state. A socket is not marked
  connected before authentication/handshake succeeds, or disconnected while an
  active receive task can still publish data.
- Send and receive loops have clear ownership and termination. Background tasks,
  timers, callbacks, pings, and queues cannot survive shutdown unnoticed.
- Malformed events, server errors, close frames, ping/keepalive failure, and
  inter-chunk stalls terminate or recover the correct request instead of hanging.
- SDK callbacks crossing threads or loops schedule work safely and cannot mutate
  completed request state.

## Request identity and concurrency

Verify:

- Each incoming vendor event is correlated to the correct TEN request. A mutable
  global `current_request_id` is safe only when the protocol and task ownership
  prove that old callbacks cannot overlap a new request.
- Appended text shares the intended vendor context; different request IDs do not
  mix contexts, audio, timestamps, counters, metadata, or terminal events.
- Locks are not held while awaiting operations that need the receive/cancel path
  to acquire the same lock.
- Queued or buffered text is released exactly once after completion and is not
  replayed during reconnect.
- Per-request start/end timestamps and emitted-byte counters follow the common
  audio-end accounting rules on every terminal path.

## Flush and cancellation

Treat flush as a transport boundary, not only a local queue clear.

Verify:

- The active vendor operation is cancelled, closed, or assigned a generation that
  prevents all later callbacks from publishing old audio.
- The interrupted request emits at most one appropriate `tts_audio_end`, while
  base-owned cleanup is not duplicated.
- No old frame or terminal event can appear after flush completion. A new request
  after flush still works.
- Cancellation interrupts connect, send, receive, first-audio wait, reconnect
  backoff, and finalization without deadlock.
- `asyncio.CancelledError` is not converted to an ordinary vendor error or
  swallowed while the producer keeps running.
- Cleanup is idempotent when vendor end, receive-loop exit, flush, and shutdown
  race with each other.

## Reconnect and recovery

Verify:

- Reconnect is bounded and only claimed when wired into the active request path.
- Retry state resets after success and terminal failure is surfaced when the
  ceiling is reached.
- Reconnect does not resend already accepted text, duplicate audio, reuse a closed
  context, or attach pre-disconnect audio to a new request.
- Whether an interrupted in-flight request is resumed, replayed, or failed is
  explicit and proven by tests.
- A request after a recoverable failure succeeds without restarting the extension.

## WebSocket/custom transport tests

Use a deterministic fake socket or SDK callback source to cover:

- handshake success/failure and server-side error events;
- fragmented/binary audio and control messages;
- close before audio, after partial audio, and without an explicit vendor end;
- flush and shutdown while connect/send/receive is blocked;
- late callback from an old request after a new request starts;
- reconnect without replay, duplication, or request-ID leakage;
- exactly-once audio end and accurate partial duration/interval accounting.

