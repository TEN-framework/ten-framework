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
| `params.language` | `en` | Language tag for routing/voice |
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


## Testing

From the extension directory, with TEN runtime/base packages installed and
available on `PYTHONPATH`, run the client, extension lifecycle, and package
contract regressions:

```bash
./tests/bin/start
```

From `ai_agents/`, set `SPEKO_API_KEY` and run the live guarder:

```bash
task tts-guarder-test EXTENSION=speko_tts2_python
```

Run ASR and TTS guarders sequentially. The guarder uses `tests/configs` and
contacts the hosted Router; it requires a funded key and an available route.
For reproducible provider checks, copy the configs to a temporary directory,
set `params.routing` to an explicit supported provider/model, then invoke the
installed guarder’s `EXT_NAME=speko_tts2_python tests/bin/start` with
`--extension_name speko_tts2_python`
and `--config_dir /absolute/path/to/configs`. Keep invalid-key configs invalid.
Client/lifecycle unit tests use mocked transport and do not require credentials.

Text chunks retain their whitespace; a chunk that is only whitespace is not
sent to the Router. An intermediate-chunk error is reported once and resets
the connection while retaining the TEN request until its final input. That
request then ends with reason `ERROR`, because the failed text is missing from
its audio. Failed text is never replayed; later text can continue unless a
permanent admission denial blocks the session.
A terminal close error produces an error completion and still releases the
request. Non-retryable denials are not automatically retried.

Each request emits exactly one `tts_audio_end`. Its duration counts the PCM
bytes actually emitted as frames, and its interval runs from that request's
`tts_audio_start`. TTFB runs from just before the text is sent to the first
audio frame. Stopping the extension closes the Router session without waiting
for final usage, and cleanup failures during stop are logged, not raised.

Standalone regression gate from `ai_agents/`:

```bash
task test-extension EXTENSION=agents/ten_packages/extension/speko_tts2_python
```

Authentication, credit, missing-route and unsupported-capability errors are fatal
and latch the session. Temporary transport failures remain non-fatal. Dump
recorders are released on completion, cancellation, request replacement, and
shutdown, including close-error paths. Connection-status tests inspect emitted
TEN JSON to verify key and endpoint redaction.
