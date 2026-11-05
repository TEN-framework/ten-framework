# Fish Audio Realtime ASR Extension

`fish_asr_python` streams mono PCM16 audio to Fish Audio's native realtime
Speech-to-Text API (`transcribe-stream-1`) over MessagePack WebSocket frames.
It supports live drafts, smart turn detection, eager end-of-turn events, and
final committed transcripts.

## Configuration

```json
{
  "type": "extension",
  "name": "stt",
  "addon": "fish_asr_python",
  "property": {
    "params": {
      "api_key": "${env:FISH_AUDIO_ASR_KEY}",
      "model": "transcribe-stream-1",
      "sample_rate": 16000,
      "turn_mode": "auto",
      "eager": true,
      "drafts": true,
      "diarize": false
    }
  }
}
```

The API endpoint is `wss://api.fish.audio/v1/asr/realtime`. The extension sends
`Authorization: Bearer <key>` and the `model` header during the WebSocket
handshake. Audio must be signed 16-bit little-endian mono PCM at one of Fish
Audio's supported rates (8, 12, 16, 22.05, 24, 32, 44.1 or 48 kHz). Fish Audio
accepts binary MessagePack frames, not JSON or base64 audio.

Language is detected by Fish Audio and returned in the result's `language` field;
there is no language field in the realtime config. `turn_mode` may be `auto`,
`vad`, or `manual`. `auto` enables Fish's user-turn, backchannel,
background-sound, and eager-end detection. `silence_timeout_ms` must be between
3000 and 60000 in `auto` mode, or between 300 and 60000 in `vad` and `manual`
modes. `finalize` data messages commit the current turn and emit
`asr_finalize_end`; stopping the extension sends `audio_done` and waits for the
normal `session_done` response.

Transient connection failures retry automatically with exponential backoff
from 0.5 seconds up to 30 seconds, until the connection succeeds or the
extension stops. Authentication and invalid-configuration failures stop
recovery. The one-hour vendor session limit opens a fresh connection;
sentence and word timestamps retain their position in the full audio stream.
Audio captured during connection setup or recovery is drained as soon as the
new session starts, even when no further audio frame arrives.

In `auto` mode, drafts are held until `user_turn_taken` confirms the turn;
background sounds and non-user turns do not publish interim transcripts.
Vendor result details, including `vendor`, `turn_index`, and `reason`, appear
under `metadata.asr_info`, alongside the input's `metadata.session_id`.
Finalize requests are serialized and each `asr_finalize_end` echoes its own
`finalize_id` and request metadata. A silent or already-ended turn can take
up to ten seconds to acknowledge because Fish may ignore its `finalize`.

## Audio dumps

Set the top-level `dump` property to `true` and `dump_path` to an output
directory to save the PCM audio sent to Fish Audio as `fish_asr_in.pcm`.
Dumping is disabled by default. The file stays open across reconnects and is
flushed and closed when the extension stops.

```json
{
  "dump": true,
  "dump_path": "/tmp/fish-asr",
  "params": {
    "api_key": "${env:FISH_AUDIO_ASR_KEY}"
  }
}
```
