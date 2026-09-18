# Common TEN TTS extension review guide

Load this guide for every in-scope TTS extension review. Apply it only to changed
files inside the extension directory.

## Scope and source of truth

An extension is in scope when its manifest imports the TTS interface or its
implementation inherits a TTS base class. Review implementation code, vendor
clients, addon/configuration code, extension manifest/property files, dependency
metadata, and extension-local tests.

Do not produce findings for `ten_ai_base`, `integration_tests/tts_guarder`,
examples, graphs, or any other directory. The installed base implementation may
be read only to understand which behavior the extension inherits.

Use these sources in descending order of authority:

1. the changed extension and its local tests;
2. the public contract of the installed base class it consumes;
3. a recent extension with the same transport shape;
4. repository documentation and older implementations.

Copied code is comparison evidence, not proof that a pattern is correct.

## Request lifecycle and ordering

Trace every request ID separately through success, empty input, vendor error,
cancellation, and shutdown.

Verify:

- Non-empty output sends `tts_audio_start` before the first PCM frame and exactly
  one `tts_audio_end` after the last frame.
- Appended text with the same request ID stays one request. A non-final segment
  does not complete it, and a final segment does not leave it stuck.
- Empty or whitespace final input does not hang or emit bogus audio.
- Explicit vendor end, iterator exhaustion, error, timeout, and exception paths
  converge on one terminal path without duplicate end events or cleanup.
- Request metadata and frames remain associated with the correct request when
  request IDs are interleaved.
- Completion releases subsequent work and cleans up request state, recorders,
  counters, timestamps, and pending messages owned by the extension.

Do not require an HTTP extension to reproduce lifecycle work that it inherits
unchanged from `AsyncTTS2HttpExtension`.

## `tts_audio_end` accounting — mandatory

Audit every changed extension-owned path that sends `tts_audio_end`. The two
numeric fields have different meanings and must be calculated independently.

### `request_total_audio_duration_ms`

This is the media duration represented by PCM frames actually emitted for the
request:

```text
duration_ms = emitted_pcm_bytes * 1000
              / (sample_rate_hz * bytes_per_sample * channels)
```

Verify:

- Count bytes after decoding, decompression, resampling, format conversion, and
  frame alignment. Do not count headers, compressed bytes, base64 text,
  conversion input, or a partial sample that was never emitted.
- Use the rate, width, and channels attached to that request's emitted frames.
- Accumulate the whole request ID, including appended text, and reset only at the
  request boundary.
- Sum bytes first and convert once; per-chunk rounding accumulates error.
- Success reports all emitted PCM. Error/interruption reports only PCM already
  emitted. No emitted audio reports `0`.
- Duplicate terminal handlers cannot reset or reuse counters before the winning
  path constructs its payload.

### `request_event_interval_ms`

This is elapsed real time from emitting `tts_audio_start` to emitting the matching
`tts_audio_end`. It is neither audio duration nor total vendor request latency.

Verify:

- Capture the start exactly once when the first `tts_audio_start` is sent. Starting
  at connect, vendor request, or text-send time incorrectly includes TTFB.
- Capture the matching end when `tts_audio_end` is sent. Appended segments do not
  replace the original start.
- Prefer a monotonic clock; wall-clock adjustment can corrupt elapsed time.
- Success, error-after-audio, and interrupted-after-audio use the same start/end
  boundary. If no audio-start event was sent, report `0`.
- Keep timestamps per request or prove that callbacks cannot pair different
  requests.
- Do not substitute PCM duration: synthesis may run faster or slower than real
  time.

A wrong formula, origin, association, or reset boundary is a correctness defect,
not a metrics preference.

## Audio correctness and latency

Verify:

- `pcm_frame` bytes are raw signed PCM matching the declared sample rate, width,
  channels, and endianness—not WAV/MP3 headers or vendor-specific floats.
- Resampling and conversion preserve samples split across chunk boundaries.
- Float conversion maps non-finite values safely, clamps before scaling, and does
  not overflow or wrap.
- Frame lengths align to `sample_width * channels`; final leftover handling does
  not invent, drop silently, or count an incomplete sample.
- Useful chunks are forwarded promptly instead of buffering the whole response.
- A configurable sample rate is proven by extension-local tests; a fixed rate is
  acceptable when it matches the vendor.

## Configuration, security, errors, and metrics

Verify:

- Manifest name, addon decorator, dependencies, and imported interface agree
  within the extension.
- Required configuration is validated before use. Internal fields such as API
  keys, dump controls, base URL, and framework-only audio settings are not
  forwarded as vendor payload parameters unless the vendor requires them.
- Keys, authorization headers, signed URLs, tokens, sensitive config, and user
  text are not exposed in logs, exceptions, metadata, or fixtures.
- Provider errors include useful vendor information and use fatal/non-fatal
  severity consistent with recoverability.
- Error paths terminate the current request without poisoning later requests.
- TTFB is measured from the actual vendor request to the first non-empty audio and
  emitted once per request. It must not be reused for audio-end event interval.
- Usage, byte, chunk, character, and duration counters are request-scoped.
- Dump writers write emitted PCM, are isolated per request, and flush on every
  terminal path without path traversal through request IDs.

## Extension-local tests

Apply the complete merge-gating matrix in [tests.md](tests.md). The suite must
exercise the extension-owned config, client/stream adapter, conversion, event,
and cleanup paths with deterministic fakes; tests of only a base class, shared
guarder, or external integration graph do not satisfy the requirement.

## Common false positives

Do not report:

- inherited base behavior as a defect in an extension that does not override it;
- lack of subtitle alignment when the vendor exposes no timestamps;
- a fixed rate that matches a fixed-rate vendor;
- absence of `tts_audio_start` for a request that emits no frames;
- old defects outside the diff unless the change makes them reachable or worse;
- speculative races without a concrete ordering and observable impact;
- formatting or naming preferences already enforced by CI;
- findings against out-of-scope directories.
