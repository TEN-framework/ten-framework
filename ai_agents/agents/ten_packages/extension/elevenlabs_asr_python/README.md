# ElevenLabs ASR Python Extension

ElevenLabs Speech Recognition Extension based on the ElevenLabs Scribe Realtime v2
speech-to-text WebSocket API.

## Configuration Parameters

### Required Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `api_key` | string | ElevenLabs API key, **must be provided** |

### Optional Parameters

| Parameter | Type | Default Value | Description |
|-----------|------|---------------|-------------|
| `ws_url` | string | `wss://api.elevenlabs.io/v1/speech-to-text/realtime` | WebSocket endpoint URL |
| `sample_rate` | int | `16000` | Audio sample rate |
| `audio_format` | string | `pcm_16000` | Audio format |
| `model_id` | string | `scribe_v2_realtime` | ElevenLabs realtime model ID |
| `language_code` | string | auto-detected | Optional ISO-639-1/3 language code |
| `include_timestamps` | bool | `false` | Whether to include delayed word timestamps; disable for lowest turn latency |
| `include_language_detection` | bool | `false` | Include detected language in delayed timestamp events |
| `commit_strategy` | string | `vad` | Commit strategy |
| `vad_threshold` | float | `0.4` | VAD sensitivity threshold |
| `vad_silence_threshold_secs` | float | `0.35` | Silence required to commit a turn |
| `min_speech_duration_ms` | int | `100` | Minimum speech duration for VAD |
| `min_silence_duration_ms` | int | `100` | Minimum silence duration for VAD |
| `keyterms` | string[] | unset | Up to 50 terms, each at most 20 characters |
| `keepalive_interval_ms` | int | unset | Optional server keepalive interval from 500 to 10000 ms |
| `enable_logging` | bool | `true` | Whether to enable logging |

### Top-level Properties

| Property | Type | Default Value | Description |
|----------|------|---------------|-------------|
| `dump` | bool | `false` | Save sent PCM audio for debugging |
| `dump_path` | string | `/tmp` | Directory for the audio dump |
| `language` | string | unset | Public TEN language (for example `en-US`); copied to the vendor `language_code` as `en` when no explicit vendor code is supplied |

The extension accepts the Scribe Realtime v2 query options for VAD, language
selection, keyterms, no-verbatim mode, background filtering, and keepalives.
TEN audio input is signed PCM16 mono, so `ulaw_8000` is rejected unless an
upstream extension converts the audio first.

When timestamps are disabled, ElevenLabs sends text-only partial and committed
events. The extension estimates segment timing from successfully sent audio so
final text remains usable by turn detection. The estimate may include silence
and is marked as `local_estimate` in `metadata.asr_info.timing_source`; provider
word timing is preserved when timestamp enrichment is enabled.

## Configuration Examples

### Complete Configuration

```json
{
    "dump": false,
    "dump_path": "/tmp",
    "language": "en-US",
    "params": {
        "api_key": "your_elevenlabs_api_key_here",
        "ws_url": "wss://api.elevenlabs.io/v1/speech-to-text/realtime",
        "sample_rate": 16000,
        "audio_format": "pcm_16000",
        "model_id": "scribe_v2_realtime",
        "language_code": "en",
        "include_timestamps": false,
        "commit_strategy": "vad",
        "vad_threshold": 0.4,
        "vad_silence_threshold_secs": 0.35,
        "min_speech_duration_ms": 100,
        "min_silence_duration_ms": 100,
        "enable_logging": true
    }
}
```

## Connection and enrichment boundaries

Connection creation, explicit connects, reconnects, finalize, and stop share a
replacement lock. Sends and transport closure share a client-operation lock;
the network handshake does not hold that lock. Every successful connection
drains retained frames, reading each frame's metadata. Pending committed text
is delivered once with its captured session and timing before replacing the
provider session. Context-local output metadata prevents the base class's
shallow merge from replacing that captured context with a later audio frame.

ElevenLabs documents timestamp enrichment as delayed, but publishes no maximum
delay and no shared transcript identifier for the committed/enriched pair
([event reference](https://elevenlabs.io/docs/eleven-api/guides/how-to/speech-to-text/realtime/event-reference),
[wire schema](https://elevenlabs.io/docs/api-reference/speech-to-text/v-1-speech-to-text-realtime)).
This adapter waits 250 ms, then emits the captured local estimate without
waiting indefinitely. Up to 256 fallback records retain the upper bound of
audio sent at commit receipt. For repeated text, enrichment whose word end
extends beyond an older record's audio bound cannot belong to that older
commit; it is paired with the pending later commit. Older or ambiguous
enrichment is ignored, preserving exactly-once text delivery. Within a pending
window, matching text is paired in commit order.

This is a conservative bounded policy, not a provider identity guarantee.
Missing enrichment followed by identical text with distinguishable word timing
retains the later provider words; indistinguishable timing falls back locally.
After the 256-record history limit, arbitrary late identical enrichment cannot
be disambiguated. Waiting forever would stall final text; matching only text
would consume later enrichment; introducing a public transcript ID would not
be supported by the provider. Protocol regressions cover missing/late repeated
enrichment, missing/delayed enrichment across replacement, yielding handshake
error delivery, active sends/finalize during stop, initial/explicit buffer
draining, and connection-status credential redaction.
