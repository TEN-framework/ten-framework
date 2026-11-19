# Rime Prism realtime speech

`rime_mllm_python` connects TEN's standard MLLM interface to [Rime Prism](https://docs.rime.ai/docs/prism/overview), a speech-to-speech model. It uses `aiohttp` WebSockets and supports Python 3.10.

## Scope

- Continuous microphone input: headerless, little-endian PCM16, 16 kHz, mono. The extension sends 20 ms frames, padding silence between microphone frames so server turn detection continues while muted.
- Spoken replies: headerless, little-endian PCM16, 24 kHz, mono, delivered through TEN's MLLM audio helper.
- Automatic server turn detection and standard TEN interruption notifications.
- Final caller and assistant captions through the existing controller and `message_collector2`.
- Standard TEN user-text and create-response messages, including the controller's greeting. Greetings wait for Prism's typed-input readiness; new caller speech supersedes queued greetings.

Tools, history loading, partial captions, live configuration, manual turn controls, response review, and recording controls are outside this version. Tool registration/results and history loading report recoverable errors rather than simulating support.

## Add to an existing realtime app

Start with `ai_agents/agents/examples/voice-assistant-realtime`. Keep its controller, caption collector, and audio connections. No shared extensions or predefined graphs need to change in the repository.

1. Replace the realtime provider dependency in your app's `manifest.json` with the path to this extension, relative to your app directory:

   ```json
   { "path": "../../../ten_packages/extension/rime_mllm_python" }
   ```

2. Replace the existing `v2v` node's addon and properties; remove the previous provider's settings:

   ```json
   {
     "type": "extension",
     "name": "v2v",
     "addon": "rime_mllm_python",
     "property": {
       "api_key": "${env:RIME_API_KEY}",
       "voice": "aurora",
       "prompt": "You are a concise, friendly voice assistant."
     }
   }
   ```

3. Configure `agora_rtc` with `subscribe_audio_sample_rate: 16000` and `subscribe_audio_num_of_channels: 1` for incoming audio. Replies carry their 24 kHz sample rate in TEN audio frames. Keep the existing `streamid_adapter` route to `v2v`.
4. Remove the weather tool node and its `tool_register` connection from your app graph. This version does not support tools.
5. Set `RIME_API_KEY` in the backend environment, install TEN dependencies with `tman install`, and install this extension's `requirements.txt` in the app's Python environment. Keep credentials out of source files and browser code.

Changing a local app graph is an application configuration step; this package does not add a second predefined graph or change another provider's defaults.

## Configuration

| Property | Default | Meaning |
| --- | --- | --- |
| `api_key` | `RIME_API_KEY` from `property.json` | Backend Rime API credential. Masked in provider configuration logs. |
| `endpoint` | `wss://prism.api.rime.ai/v1/realtime` | Prism WebSocket endpoint. Requires TLS except for localhost tests. |
| `voice` | `aurora` | Prism voice selected when the session starts. |
| `prompt` | empty | Instructions selected when the session starts. |
| `startup_timeout` | `15` | Startup budget in seconds, also the maximum duration of a WebSocket write, including waiting for another writer. |
| `startup_retries` | `2` | Retries for transient connection failures before initialization. Authentication and protocol errors are not retried. |

An established conversation is never reconnected or replayed automatically. Start a new call after a terminal failure. Delayed writes catch up to the original audio schedule. Input buffering and the sender's catch-up window are bounded to one second; exceeding either limit ends the provider session instead of sending stale microphone audio or a long burst of accumulated silence.

## Interruption and errors

The extension emits TEN's standard interruption event when Prism confirms caller speech. The existing controller sends `flush` to RTC. Old response audio and final text are discarded by response identity after interruption.

RTC flush clears local pending audio and its sender buffer. It does not acknowledge what the receiving browser played or remove all audio buffered there. A short tail may remain audible. No playback position, client completion, or conversation truncation is inferred. Prism uses its server-side playback estimate.

Only final captions are forwarded. Caller transcription can arrive after the associated reply, so captions can appear out of conversational order. An interrupted reply may not produce a final caption. This avoids requiring partial-caption queue changes in the shared collector.

Fatal errors stop and join this extension's audio/transport work and emit TEN's standard MLLM `error` data with vendor code. The host application must consume fatal errors and end the room as appropriate; the stock controller does not close the app for these events. Recoverable errors leave the session active. Provider error messages exclude raw server payloads and credentials.

## Tests

After installing TEN and test dependencies (`pytest` and `pytest-asyncio`), run `tests/bin/start`. Without a native install, set `TEN_AI_BASE_INTERFACE_PATH` to the directory containing `ten_ai_base`; adapter tests use those real base models/helpers. The transport tests use a local WebSocket peer and need no API key or external network.
