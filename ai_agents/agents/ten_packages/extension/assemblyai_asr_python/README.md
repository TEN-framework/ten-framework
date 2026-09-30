# AssemblyAI ASR Extension

Real-time speech-to-text for TEN agents using the
[AssemblyAI Streaming API (v3)](https://www.assemblyai.com/docs/api-reference/streaming-api/streaming-api).
Defaults to `universal-3-6-pro` (the API's current default); `universal-3-5-pro`,
future `universal-3-*-pro` releases and the Universal Streaming models are also
supported.

## Features

- Universal-3 Pro family: `mode` presets, contextual `prompt`,
  `keyterms_prompt`, `language_codes` steering, `language_detection`,
  `voice_focus`, conversational context carry-over and `agent_context`.
- Punctuation-based turn detection with `min_turn_silence` /
  `max_turn_silence` / `vad_threshold` / `interruption_delay` tuning.
- Speaker labels, PII redaction, profanity filtering and Medical Mode
  (`domain: "medical-v1"`) pass through.
- Word-level results (`words[]`) and per-turn `metadata.asr_info`
  (`turn_order`, `end_of_turn`, `turn_is_formatted`, `end_of_turn_confidence`,
  detected `language_code`, `speaker_label`).
- Reconnect with exponential backoff; auth failures (HTTP 401/403, close code
  1008) are reported as fatal errors with `vendor_info`.

## Configuration

Set `ASSEMBLYAI_API_KEY` in `ai_agents/.env`. All vendor settings live under
`params`:

```json
{
  "params": {
    "api_key": "${env:ASSEMBLYAI_API_KEY}",
    "speech_model": "universal-3-6-pro",
    "sample_rate": 16000,
    "language": "en-US",
    "format_turns": true,
    "mode": "balanced",
    "prompt": "Customer support call for an online bookstore.",
    "keyterms_prompt": ["TEN Framework", "AssemblyAI"]
  }
}
```

| Param | Type | Default | Notes |
| ----- | ---- | ------- | ----- |
| `api_key` | string | | Required. Sent as the `Authorization` header. |
| `ws_url` | string | `wss://streaming.assemblyai.com/v3/ws` | Use `wss://streaming.eu.assemblyai.com/v3/ws` for EU residency. |
| `speech_model` | string | `universal-3-6-pro` | `universal-3-*-pro`, `universal-streaming-english`, `universal-streaming-multilingual`. |
| `sample_rate` | int | `16000` | Input PCM sample rate. |
| `encoding` | string | `pcm_s16le` | Only `pcm_s16le`: TEN delivers PCM16 mono and the extension does not transcode. |
| `format_turns` | bool | `true` | Punctuation, casing and inverse text normalization on final turns. |
| `language` | string | `en-US` | Locale reported in `asr_result.language`; also derives `language_codes` when that is unset. |
| `language_codes` | string[] | derived | Pro only. Steers output toward these ISO 639-1 codes. `[]` disables steering. |
| `language_detection` | bool | | Pro / multilingual. Reports `language_code` per turn (used for `asr_result.language`). |
| `mode` | string | server default `balanced` | Pro only. `min_latency`, `balanced`, `max_accuracy`. |
| `prompt` | string | | Pro only. Plain-language description of the audio (max 1750 chars). Not instructions. |
| `keyterms_prompt` | string[] | `[]` | Up to 100 terms, 50 chars each. |
| `agent_context` | string | | Pro only. Seeds the session with the agent's opening line. See below. |
| `previous_context_n_turns` | int | server default `5` | Pro only. Prior entries carried as context (0 disables). |
| `min_turn_silence` / `max_turn_silence` | int (ms) | mode dependent | Turn end timing. |
| `vad_threshold` | float | mode dependent | Silence classification threshold. |
| `interruption_delay` | int (ms) | mode dependent | Pro only. Time before the first partial. |
| `continuous_partials` | bool | `true` | Pro only. Extra partials every ~3 s during long turns. |
| `include_partial_turns` | bool | `true` | Set `false` to receive only final turns. |
| `voice_focus` / `voice_focus_threshold` | string / float | | Pro only. `near-field` or `far-field` noise suppression. |
| `speaker_labels` / `max_speakers` | bool / int | | Diarization. Note: `speaker_labels` replaces the `mode` preset server-side. |
| `domain` | string | | `medical-v1` enables Medical Mode. |
| `redact_pii` / `redact_pii_policies` / `redact_pii_sub` | bool / string[] / string | | Streaming PII redaction (final turns only). |
| `filter_profanity` | bool | | |
| `inactivity_timeout` | int (s) | | Session auto-close after silence. |
| `session_heartbeat` | bool | | Server `Heartbeat` messages (logged as vendor events). |
| `finalize_timeout_ms` | int (ms) | `3000` | Extension-side: complete the `asr_finalize` handshake even if no final turn arrives in time. Not sent to the vendor. |
| `end_of_turn_confidence_threshold` / `min_end_of_turn_silence_when_confident` | float / int | | Universal Streaming models only; ignored by the Pro family. |

Parameters that only the Pro family understands are dropped automatically
for Universal Streaming models, and vice versa. Any key in `params` that is
not listed above is forwarded to the connection URL verbatim, so new API
parameters can be used without a code change (add them to `manifest.json` if
your graph relies on schema validation).

## agent_context

Universal-3 Pro models can use the agent's most recent spoken reply as context for
the next user turn, which markedly improves short answers and spelled-out
entities. This extension accepts the standard `tts_text_input` data message:
it buffers the text per `request_id` and, when `text_input_end` arrives,
sends `UpdateConfiguration { agent_context }` (last 1750 characters). The
latest reply is re-sent after a reconnect.

Because `main_control` sends `tts_text_input` with an explicit destination,
graph connections alone do not deliver it here. The `voice-assistant`
example's `main_python` fans the same message out when its
`agent_context_dest` property names this node:

```json
{
  "name": "main_control",
  "addon": "main_python",
  "property": {
    "greeting": "TEN Agent connected. How can I help you today?",
    "agent_context_dest": "stt"
  }
}
```

See the `voice_assistant_assemblyai` graph in
`agents/examples/voice-assistant/tenapp/property.json` for a complete wiring.

## Notes

- With `format_turns: true` AssemblyAI emits an unformatted `end_of_turn`
  turn followed by the formatted one; only the formatted turn is reported as
  `final: true`.
- `metadata.asr_info.end_of_turn_confidence` is present on partial results as
  well, so a controller can start eager LLM inference before the final turn.
  It is exposed as metadata only; no separate event is emitted.
- Audio is discarded while disconnected (`ASRBufferConfigModeDiscard`):
  AssemblyAI rejects audio sent faster than real time (close code 3007), so a
  backlog cannot be replayed after a reconnect.
- Errors are reported by WebSocket close code. `1008` (auth/account) and
  handshake `401`/`402`/`403` are fatal; `3005`-`3009`, `1006`, `1011` are
  non-fatal and trigger a reconnect with exponential backoff (5 attempts,
  then fatal). A clean `1000` close reconnects silently, so an
  `inactivity_timeout` will cause periodic reconnects during long silences.
- Every `asr_finalize` produces exactly one `asr_finalize_end` echoing its
  own `finalize_id` and `session_id`: after the final turn, after an empty
  end-of-turn (silence), immediately when disconnected, on stop, or after
  `finalize_timeout_ms`. Overlapping requests are completed in order.
- Logs never contain credentials or conversational text: secret-looking
  keys in `params` (for example a `token`) and signed `ws_url` query values
  are masked, and control messages are logged by type and field names only.
- Parameters the selected model does not accept are logged as ignored at
  connect time rather than sent.

## Testing

```bash
# Standalone unit tests (inside the ten_agent_dev container)
task test-extension EXTENSION=agents/ten_packages/extension/assemblyai_asr_python

# ASR guarder against the live API (needs ASSEMBLYAI_API_KEY in .env)
task asr-guarder-test EXTENSION=assemblyai_asr_python
```
