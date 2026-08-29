# Implementation notes for `aivis_tts_python`

A short, reviewer-facing rationale for the non-obvious design choices in
this extension. The README is the "what"; this document is the "why".

## Why `AsyncTTS2HttpExtension` (and not `AsyncTTS2BaseExtension`)

The naming is misleading — `AsyncTTS2HttpExtension` is not "HTTP vs WebSocket"
so much as "single request yields streamed chunks" vs "the extension drives
an interactive session".

Aivis's public API is exactly the former:

```
POST {base_url}/v1/tts/synthesize
Authorization: Bearer <API_KEY>
Content-Type: application/json

{
  "model_uuid": "...",
  "text": "...",
  "output_sampling_rate": 16000,
  ...
}
```

The response is a streamed `audio/wav` body — a complete WAV file
delivered chunk-by-chunk. The whole text is sent in one request; the
extension does not need to drive a session, forward partial text, or
finalize with `end_of_stream`.

This is the same shape as Groq's TTS, which is why Groq uses
`AsyncTTS2HttpExtension`. Gradium uses `AsyncTTS2BaseExtension` because
Gradium's API is a WebSocket session that takes incremental text and
emits `event_end_of_stream`; Tencent / Rime / Stepfun are in the same
bucket.

So the choice is determined by the vendor's protocol, not by whether
"HTTP" appears in the URL.

## Why a custom `WavStreamParser` (copied from Groq)

Aivis returns WAV, not raw PCM. TEN's `AsyncTTS2HttpClient.get()` must
yield raw PCM bytes so the base class can frame them into audio frames
and measure TTFB from the first byte of audio.

Two options:

1. Accumulate the whole WAV in memory, strip the header with `wave`,
   yield the PCM.
2. Stream the WAV and strip the header inline as bytes arrive.

Option 1 simplifies the code but inflates memory and TTFB. For a
realtime conversational extension, TTFB is user-visible latency, and
loading several hundred KB of audio before emitting the first byte is
noticeable.

`WavStreamParser` (lifted from Groq, identical) buffers the first 4 KB
(sufficient for the WAV header), parses the header with `wave.open`,
and exposes:

- `get_format_info()` — channels, sample width, framerate.
- `__aiter__` — yields raw PCM chunks.

It also buffers the first PCM chunk that follows the header inside the
initial 4 KB window. We yield this chunk immediately after
`get_format_info()` so the base class marks TTFB against the first
audio byte, not against the moment the first WAV byte arrives.

## Why `output_format` is forced to `wav`

`AivisTTSConfig.update_params()` calls
`self.params.setdefault("output_format", "wav")` and then
unconditionally `self.params["output_format"] = "wav"`. Aivis's API
also supports `mp3` and `opus`, but TEN requires PCM. Forcing `wav`
keeps the parser contract (one WAV header → many PCM chunks) and
prevents an operator from accidentally passing `mp3` and getting an
unrecognized payload.

## Why `get_extra_metadata` exists

The base class forwards `get_extra_metadata()` to downstream metrics
events. We include `model_uuid`, `language`, and `output_sampling_rate`
so observability dashboards can attribute a TTFB or audio_end to the
specific model and sample rate that produced it.

## Why the test_basic mocks `extension.AivisTTSClient`

`tests/test_basic.py` uses
`@patch("aivis_tts_python.extension.AivisTTSClient")`. This works
because `extension.py` does `from .aivis_tts import AivisTTSClient`,
binding the symbol as a module attribute on `extension`. The mock
target therefore resolves via the standard dotted-name mechanism.

## Why the manifest schema declares `output_audio_channels`

Aivis accepts `mono` / `stereo`. `config.update_params()` defaults
this to `"mono"`. Without a schema entry, a TEN property validator
would silently accept the key without documenting it. Adding it to
the manifest makes the contract match the behaviour.
