# Required extension-local test matrix

Every TTS extension must own a deterministic local test suite under its
extension directory. Treat this matrix as a merge gate for every TTS extension
touched by the pull request. Shared base-class, guarder, graph, example, and live
service tests may supplement the suite but cannot replace it.

Prefer fake clients, fake streams, controlled clocks, and fixed PCM fixtures.
Tests must traverse the extension-owned adapter and assert emitted TEN events;
tests that only instantiate config objects or mock the method under test do not
cover the behavior.

## Core success and request lifecycle

Require tests that prove:

- a normal request emits playable PCM with the declared sample rate, sample
  width, channel count, and deterministic event order;
- each request emits `tts_audio_start` and exactly one matching `tts_audio_end`
  when audio is produced, with the correct request ID;
- `text_input_end` finalizes the correct request without losing buffered text or
  terminating another request;
- sequential requests transition through queued, processing, finalizing, and
  completed states without sharing audio, counters, or terminal events;
- appended and rapidly interleaved input remains associated with its request;
- a successful request still works after a recoverable error, cancellation,
  flush, retry, or reconnect.

## Exact audio-end accounting

These tests are mandatory, not optional metric coverage:

- Feed known emitted PCM bytes and assert
  `request_total_audio_duration_ms == emitted_bytes * 1000 /
  (sample_rate * sample_width_bytes * channels)` using the final output format.
- When conversion or alignment changes the bytes, assert against bytes actually
  emitted, including chunks split across input boundaries. Sum bytes first and
  calculate once so per-chunk rounding cannot accumulate.
- Use a controlled monotonic clock and assert `request_event_interval_ms` is the
  elapsed time from the matching `tts_audio_start` to `tts_audio_end`, rather
  than request latency, TTFB, vendor processing time, or audio duration.
- Cover success, partial audio followed by error, flush/cancellation after audio
  starts, and no-audio termination. Each path must use the correct zero/non-zero
  values and emit at most one end event.

## Flush and cancellation

Require tests for flush after audio has started, cancellation while connecting
or awaiting the first byte, cancellation during streaming, and cancellation near
normal completion. Assert that:

- already emitted audio is accounted for exactly;
- no audio or stale terminal event is emitted after the flush boundary;
- resources and dump writers close on every terminal path;
- a late callback cannot revive or complete the cancelled request;
- a subsequent request starts and completes normally.

## Configuration and parameter mapping

Require fixtures or equivalent parameterized tests for default settings, custom
settings, at least two supported audio-output configurations, dump enabled,
missing required settings, and invalid settings. Assert that:

- required credentials and endpoints fail before a request is sent;
- supported voice, speed, language, sample-rate, and audio controls map to the
  client request correctly; unsupported controls are rejected or ignored by an
  explicit contract test;
- free-form parameters pass through, while framework-only and internal fields do
  not leak into the service payload;
- URL/base-URL and authorization-header precedence is deterministic;
- empty values, booleans used as numbers, non-finite numbers, and out-of-range
  values are rejected or normalized to documented safe defaults;
- metadata uses canonical field names and omits empty values;
- string representations, logs, exceptions, and fixtures redact credentials,
  authorization headers, signed URLs, tokens, and user text.

## Error behavior and recovery

Use deterministic failures to cover invalid credentials or required parameters,
authentication rejection, quota or rate limiting, timeout, network failure,
invalid input, and a generic service failure. Where the service exposes language
constraints, include unsupported-language behavior. Assert that:

- unrecoverable configuration/authentication errors are fatal and transient
  transport/service errors are non-fatal when retry or later requests can work;
- emitted errors carry useful service metadata and diagnostic context without
  exposing secrets or user text;
- an error before audio and an error after partial audio terminate exactly once;
- request state, buffers, metrics, and connections are clean for the next
  successful request.

## Metrics and dump output

Require tests for normal, long or multi-chunk, and very fast responses, plus a
flush or cancellation path. Assert that TTFB is measured from service-request
start to the first non-empty audio chunk and emitted once per request. Assert
request-scoped byte, chunk, character, duration, and timing values do not leak
between requests.

With dumping enabled, assert the dump file exists, contains exactly the PCM bytes
emitted to TEN in the same order, and is finalized on success, error, flush, and
cancellation.

## Robustness

Require deterministic tests for concurrent requests, rapid request sequences,
long text, large or many audio chunks, cancellation under load, and retry after a
transient network failure. Assert bounded buffering, correct request ownership,
no duplicated audio, no deadlock, and recovery without restarting the extension.

## Transport additions

In addition to the common matrix:

- HTTP implementations must cover non-success status before audio, invalid or
  unexpected content type, chunked stream exhaustion, cancellation closing the
  response, and conversion whose input sample boundaries span HTTP chunks.
- WebSocket/custom streaming implementations must cover reconnect after a
  dropped connection, replacement-connection ownership, disabling callbacks on
  the old connection, disconnect/cancel ordering, failure while reporting a
  disconnect, and concurrent cancellation that must not close a replacement
  connection.

If a transport or capability is genuinely absent, the inapplicable conditional
case may be omitted. The reviewer must verify that absence from implementation
and manifest/config rather than accepting an unexplained test gap.

## Reporting gaps

- No extension-local suite: report one P1 merge-blocking finding.
- Missing mandatory sections or transport cases: group closely related omissions
  into the smallest useful P2 findings; do not list every test name separately.
- Name the unprotected runtime behavior and expected assertion. Do not give vague
  advice such as "add more tests."
