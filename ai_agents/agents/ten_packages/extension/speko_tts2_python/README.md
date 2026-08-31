# Speko Router TTS2 extension

Routes TEN text-to-speech requests through Speko and emits PCM audio through
the standard TEN TTS2 interface. Each TEN text chunk becomes one Speko
utterance, so sentence-level streaming starts before `text_input_end`.

## Configuration

Set `SPEKO_API_KEY`, then use the extension defaults or override `params` in
your graph.

| Property | Default | Description |
| --- | --- | --- |
| `params.base_url` | `https://router.speko.dev` | Speko Router origin |
| `params.sample_rate` | `24000` | PCM output sample rate |
| `params.channels` | `1` | PCM output channel count |
| `params.language` | `en` | Optional routing/voice hint |
| `params.voice` | empty | Optional provider voice identifier |
| `params.routing` | auto, balanced | Speko routing selection |

Output is raw signed 16-bit little-endian PCM. TEN `tts_flush` maps to Speko
`input.cancel`; no audio is forwarded after the flush completes.

```json
{
  "type": "extension",
  "name": "tts",
  "addon": "speko_tts2_python",
  "extension_group": "tts",
  "property": {
    "params": {
      "api_key": "${env:SPEKO_API_KEY}",
      "routing": {"mode": "auto", "objective": "quality"},
      "language": "en",
      "sample_rate": 24000
    }
  }
}
```
