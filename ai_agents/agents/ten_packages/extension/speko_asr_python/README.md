# Speko Router ASR extension

Streams TEN PCM audio to the Speko Router and emits the standard TEN
`asr_result`, `asr_finalize_end`, `metrics`, `error`, and connection-status
messages.

## Configuration

Set `SPEKO_API_KEY`, then use the extension defaults or override `params` in
your graph.

| Property | Default | Description |
| --- | --- | --- |
| `params.base_url` | `https://router.speko.dev` | Speko Router origin |
| `params.sample_rate` | `16000` | PCM input sample rate |
| `params.channels` | `1` | PCM input channel count |
| `params.language` | `en-US` | Optional routing/transcription hint |
| `params.routing` | auto, balanced | Speko routing selection |
| `params.options` | `{}` | Diarization, keywords, noise reduction, or namespaced provider options |
| `params.buffer_duration_ms` | `5000` | Audio retained while disconnected; `0` discards |

The transport is raw signed 16-bit little-endian PCM. A TEN `asr_finalize`
message sends Speko `input.commit` and `asr_finalize_end` is emitted after the
corresponding final transcript.

```json
{
  "type": "extension",
  "name": "asr",
  "addon": "speko_asr_python",
  "extension_group": "asr",
  "property": {
    "params": {
      "api_key": "${env:SPEKO_API_KEY}",
      "routing": {"mode": "auto", "objective": "latency"},
      "language": "en-US"
    }
  }
}
```

For an explicit route, set `routing` to
`{"mode":"explicit","provider":"deepgram","model":"nova-3"}`.
