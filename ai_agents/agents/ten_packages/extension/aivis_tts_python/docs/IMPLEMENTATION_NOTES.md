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

## Manifest schema is type-only (matches sibling extensions)

The manifest declares each param's `type` (`int64`, `string`,
`float64`, `boolean`) but no `enum` / `minimum` / `maximum`
constraints. This matches `rime_http_tts` and `groq_tts_python`,
which also leave value-level validation to the vendor. The README's
parameter table carries the bounds and enums in human form instead,
so a developer reading the docs sees the full contract while the
manifest stays aligned with the existing extension set.

## Why `use_ssml` defaults to `false` (vendor default is `true`)

Aivis's API default for `use_ssml` is `true`: any `<`-shaped character
in `text` is parsed as a potential SSML tag. For a conversational
voice agent the input comes from an LLM that occasionally emits
literal characters like `<` or `>` in plain prose (URL fragments,
inequalities, template variables). Treating these as SSML causes the
API to either silently drop them or, for malformed tags, return a
422. Overriding to `false` keeps LLM output verbatim. Operators who
*want* SSML control (pauses via `<break time="…"/>`, prosody) can
flip the default in `property.json` or pass `use_ssml: true` in the
graph.

## API field coverage

Every field Aivis's `/v1/tts/synthesize` body accepts (per the public
ReDoc at `https://api.aivis-project.com/v1/docs`) is exposed in the
manifest schema:

- Core: `model_uuid`, `text`, `language`
- Style: `speaker_uuid`, `style_id`, `style_name`
- Prosody: `speaking_rate`, `emotional_intensity`, `tempo_dynamics`,
  `pitch`, `volume`
- Timing: `leading_silence_seconds`, `trailing_silence_seconds`,
  `line_break_silence_seconds`
- Format: `output_format` (forced `wav`), `output_sampling_rate`,
  `output_audio_channels`
- Parsing: `use_ssml`, `use_volume_normalizer`

`request_body()` in `config.py` forwards every non-`None` param that
isn't in `CLIENT_ONLY_KEYS` (`api_key`, `base_url`, `endpoint`). So
adding a new field only requires a manifest schema entry plus the
README table — no client code changes.
