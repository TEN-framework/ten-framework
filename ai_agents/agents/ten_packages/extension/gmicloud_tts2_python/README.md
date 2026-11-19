# GMI Cloud TTS2 Python Extension

Speech synthesis for the TEN Framework through the GMI Cloud request-queue
API. One extension covers the speech model families GMI Cloud hosts:
`inworld-tts-*` (low latency), `minimax-tts-speech-*`, and `Chatterbox-tts`.

## Install

```
export GMI_API_KEY=<your GMI Cloud API key>
```

The default `property.json` reads the key from `GMI_API_KEY` via
`${env:GMI_API_KEY|}`. Set `params.api_key` explicitly to override.

## Config

| Parameter | Default | Notes |
|---|---|---|
| `api_key` | `${env:GMI_API_KEY\|}` | GMI Cloud API key (Bearer) |
| `base_url` | `https://console.gmicloud.ai` | Request-queue host |
| `model` | `inworld-tts-2` | GMI Cloud model id |
| `voice` | `Dennis` | `voice_id` (Inworld/MiniMax) or a Resemble `voice_uuid` (Chatterbox) |
| `speed` | `1.0` | Speaking rate |
| `temperature` | none | Inworld only (0.0-2.0); omitted from the payload when unset |
| `temperature` | none | Inworld only |
| `audio_format` | `WAV` | WAV only: PCM frames are emitted |
| `sample_rate` | `24000` | Output rate; vendor audio is resampled to it |
| `channels` | `1` | Must be 1: vendor audio is downmixed to mono |
| `text_chunk_chars` | `1000` | Sentence-bounded chunking cap (see Long text below) |
| `request_timeout_sec` | `60.0` | HTTP timeout |
| `poll_interval_sec` | `2.0` | Queue poll interval |
| `poll_attempts` | `90` | Polls before giving up |

## Models and request shapes

| Family | Model ids | Voice field | Notes |
|---|---|---|---|
| Inworld | `inworld-tts-*` | `voice_id` | Low latency; long text is chunked at sentence boundaries and concatenated as continuous PCM |
| MiniMax | `minimax-tts-speech-*` | `voice_id` | Queue-backed generation |
| Chatterbox | `Chatterbox-tts` | `voice_uuid` | Requires a Resemble voice UUID; the extension rejects the configuration at startup without one |

## Streaming and TTFB limits

The GMI request-queue API is job-based: a request submits a synthesis job and
the audio arrives as one complete WAV at the end. Incremental streaming is
not available through this transport, so time-to-first-audio includes the
full generation time for the part. Audio is then emitted to TEN in ~20 ms
mono s16 frames as soon as it is available. Requests are serialized per
call; long inputs split into parts stream part by part.

## Long text

GMI rejects request text above 2,000 characters (verified against the live
API), and long generations can outrun the request-queue gateway's timeout.
Long input is therefore split at sentence boundaries (CJK punctuation
included) into parts of at most `text_chunk_chars` (default 1,000, the size
verified live for both English and Chinese) and the parts are concatenated
as continuous PCM. CJK sentences are joined without inserted spaces.

## Output

Mono signed 16-bit PCM at `params.sample_rate`. Vendor audio is parsed from
WAV, downmixed to `channels`, and resampled to the declared rate before
frames are emitted. Non-WAV responses are reported as errors, never passed
through as garbage frames.

## Errors

Failures are mapped onto the TTS2 HTTP event contract and the base extension
turns them into `ModuleError` envelopes (module `TTS`, vendor `gmicloud`):

- 401/403 from GMI Cloud yield `INVALID_KEY_ERROR`, reported as
  `FATAL_ERROR` (a wrong key will not heal on retry).
- Every other failure (HTTP errors, poll timeouts, non-WAV audio, generation
  failures) yields `ERROR`, reported as `NON_FATAL_ERROR` so the pipeline can
  retry.
- Cancelled requests yield `FLUSH` and stop cleanly.

Chatterbox without a `voice_uuid` is rejected at startup by config
validation, with the fix named in the message.

Known vendor behavior: GMI's Inworld gateway intermittently returns 500
with an upstream timeout (`api.inworld.ai ... context deadline exceeded`)
on long or Chinese generations; the same request often succeeds on retry.
Verified against the raw API as well as through this client, so it is
vendor-side. The event contract surfaces it as `ERROR`/`NON_FATAL_ERROR`
so pipelines can retry.

## Word timing / subtitles

Not applicable: the GMI request-queue API returns complete audio without
word or character timestamps, so this adapter does not supply
`tts_text_result` timing. No subtitle alignment hook is required.

## Tests

```
tests/bin/start
```

The suite covers family mapping, request payload shapes, sentence-boundary
chunking, WAV parsing, stereo downmix, resampling, the synchronous and
queued response paths, fail-fast polling, error-event mapping
(`INVALID_KEY_ERROR` vs `ERROR` vs `FLUSH`), long-text chunk concatenation,
and config validation including key masking.
