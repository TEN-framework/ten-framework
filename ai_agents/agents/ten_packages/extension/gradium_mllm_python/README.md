# gradium_mllm_python

Real-time speech-to-speech (S2S) translation extension for TEN, using
[Gradium](https://gradium.ai/)'s Translation API. Implements the same
`AsyncMLLMBaseExtension` contract as `openai_mllm_python`, `azure_mllm_python`,
`gemini_mllm_python`, `glm_mllm_python`, and `stepfun_mllm_python`, so it can
be dropped into any graph node expecting an "mllm" addon.

## Status: scaffolded, not yet verified against a live Gradium endpoint

This repo already has `gradium_asr_python` and `gradium_tts_python`, which
talk to Gradium's real ASR and TTS websocket APIs. This extension reuses
everything confirmed by those two:

| Confirmed (via gradium_asr_python / gradium_tts_python) | Value |
|---|---|
| Auth | header `x-api-key: <api_key>` |
| Host pattern | `wss://<region>.api.gradium.ai/api/speech/<service>`, region `us` or `eu` |
| Handshake | client sends `{"type": "setup", ...}`, waits for `{"type": "ready"}` before streaming audio |
| Audio frames (both directions) | `{"type": "audio", "audio": "<base64 pcm16le>"}` |
| Text/transcript frames | `{"type": "text", "text": ..., "final": bool, ...}` |
| End of turn | `{"type": "end_of_stream"}` |
| Errors | `{"type": "error", "message": ..., "code": ...}` |
| VAD events | `{"type": "vad", ...}` (received, not currently acted on -- matches gradium_asr_python) |
| Sample rates | 24kHz PCM in, 48kHz PCM out by default (`output_format: "pcm"`) |

**Not confirmed** -- this is the one real gap, isolated in `config.py`'s
`path` field:

- The exact websocket path for the *combined* speech-to-speech endpoint.
  `/api/speech/s2s` (in `property.json`) is extrapolated from the `asr`/`tts`
  pattern plus the "s2s-websocket" label referenced on gradium.ai/translate
  -- not from real docs. If it's wrong, connecting will fail cleanly at
  `start_connection()` with a clear error; fix it in one place
  (`property.json`'s `path`, or `GradiumMLLMConfig.path`'s default).
- Whether Gradium's "text" messages on this combined endpoint carry only the
  translated output, or also a separate source-language transcript. Right
  now only the translated/output side is wired to
  `mllm_server_output_transcript`; there's no `mllm_server_input_transcript`
  emission. Revisit once real behavior is observed.
- Whether `stt_model_name`/`tts_model_name`/`target_language` are the right
  field names for the combined setup payload (they're carried over from the
  gradium.ai/translate marketing page example, not from API docs).

Update this table (and the code) once Gradium shares real docs, then verify
against a live connection -- see the parent repo's implementation plan.

## Properties

Refer to `api` definition in [manifest.json](manifest.json) and default
values in [property.json](property.json).

| **Property** | **Type** | **Description** |
|---|---|---|
| `api_key` | `string` | Gradium API key (sent as the `x-api-key` header) |
| `region` | `string` | `us` or `eu` -- selects the websocket host |
| `base_url` | `string` | Optional explicit host override, skips region lookup |
| `path` | `string` | Websocket path -- see "Not confirmed" above |
| `model_name` | `string` | Gradium speech-to-speech model name |
| `stt_model_name` | `string` | Optional ASR-leg model override |
| `tts_model_name` | `string` | Optional TTS-leg model override |
| `voice_id` | `string` | Voice for the synthesized translated speech |
| `language` | `string` | Source language hint (empty = auto-detect) |
| `target_language` | `string` | Language to translate into |
| `input_format` | `string` | Input audio format (`pcm`) |
| `output_format` | `string` | Output audio format (`pcm`, `pcm_16000`, `pcm_24000`) |
| `input_sample_rate` | `int32` | Input PCM sample rate, Hz |
| `dump` / `dump_path` | `bool` / `string` | Audio dump for debugging (from the shared mllm interface) |

## Not implemented

Gradium's S2S translation is a continuous audio pipe, not a tool-calling
conversational LLM. `send_client_message_item`, `send_client_create_response`,
`send_client_register_tool`, and `send_client_function_call_output` are all
no-ops (logged at debug level) -- there's no known Gradium equivalent for
injecting messages/tools into a translation stream. Revisit if that turns
out to be wrong.

### Audio Frame In / Out

| **Name** | **Description** |
|---|---|
| `pcm_frame` | mic audio in / translated speech out |

### Data Out

`mllm_server_session_ready`, `mllm_server_output_transcript`, `error` (from
the shared `mllm-interface.json` contract).
