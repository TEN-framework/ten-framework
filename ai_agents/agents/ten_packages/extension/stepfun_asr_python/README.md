# StepFun ASR Python Extension

As of: 2026-10-26 UTC
Codebase commit: `3cac62982579929acdee4403761a610a09f850ae` (with local changes)

StepFun StepAudio 2.5 streaming ASR integration for TEN Framework.

## Configuration

The extension connects to `wss://api.stepfun.com/v1/realtime/asr/stream` with
Bearer authentication. The default model is `stepaudio-2.5-asr-stream`.
StepFun accepts signed 16-bit little-endian mono PCM at 16 kHz.

Set the key in the agent environment:

```bash
STEPFUN_API_KEY=your_stepfun_api_key
```

Example graph node:

```json
{
  "type": "extension",
  "name": "stt",
  "addon": "stepfun_asr_python",
  "extension_group": "stt",
  "property": {
    "params": {
      "api_key": "${env:STEPFUN_API_KEY}",
      "language": "zh",
      "prompt": "专业术语：阶跃星辰、StepAudio"
    }
  }
}
```

Supported parameters are exposed in `manifest.json`. `prompt` is sent in
`session.update` as StepFun transcription context (domain terms or
instructions). `server_vad` enables StepFun server-side VAD; otherwise the
extension commits audio on TEN's `asr_finalize` input. `full_rerun_on_commit`
and `enable_timestamp_align` are passed through to StepFun.

Audio received while disconnected is buffered by `AsyncASRBaseExtension`
(up to 10 MiB, dropping the oldest frames on overflow). After reconnect,
the input consumer drains this buffer even if no new frame arrives.

Audio is sent directly to StepFun without retaining PCM until final results.
Previously sent audio is not replayed after disconnect, so an interrupted
utterance may be incomplete. Finalize requests still complete or time out
with their original request and session metadata. Provider timestamps are
offset across connections to preserve the audio timeline.
