# kalpa_tts_python

Text-to-speech from [Kalpa](https://kalpalabs.ai) for TEN voice agents, over
Kalpa's streaming WebSocket
([`WSS /v1/tts/{voice_id}/stream`](https://docs.kalpalabs.ai/reference#wss-v1-tts-voice-id-stream)).
The default model, `kalpa-tts-1`, speaks English, Hindi and code-switched
Hinglish.

## Configuration

Create an API key at <https://kalpalabs.ai/dashboard/keys/> and pass it in
`params`. Only `api_key` is required.

```json
{
  "params": {
    "api_key": "${env:KALPA_TTS_API_KEY}",
    "voice_id": "18c26f49-9f9c-400c-bec9-7467f28e1db5",
    "model": "kalpa-tts-1",
    "sample_rate": 24000
  }
}
```

| Property | Default | Meaning |
|---|---|---|
| `params.api_key` | required | Kalpa API key. |
| `params.voice_id` | Arjun | A voice id from `GET /v1/voices`. A voice is offered for some models only; `GET /v1/voices?model=kalpa-tts-1` lists that model's voices. |
| `params.model` | `kalpa-tts-1` | A model id from `GET /v1/models`. |
| `params.sample_rate` | `24000` | Output rate in Hz: `8000`, `16000`, `22050`, `24000`, `32000`, `44100` or `48000`. Audio is 16-bit mono PCM. |
| `params.chunk_length_schedule` | Kalpa's default, `[50, 80, 120, 150]` | Characters Kalpa buffers before each successive part of a request starts generating: 1 to 10 values from 50 to 2000. Lower values start audio earlier with less prosodic context. |
| `params.base_url` | `wss://api.kalpalabs.ai` | Host of another Kalpa deployment. |
| `params.url` | unset | Full WebSocket endpoint, used as given; `{voice_id}` in it is replaced by the voice. Wins over a top-level `url`, which wins over `params.base_url`. |
| `dump`, `dump_path` | `false`, `/tmp` | Write each request's audio to `kalpa_dump_<request_id>.pcm`. |

Every other key in `params` is passed to Kalpa as a generation parameter:
`temperature`, `acoustic_temperature` and `audio_quality` are described in
the [Kalpa documentation](https://docs.kalpalabs.ai/text-to-speech#generation-parameters).

## Behavior

- **Streaming.** The session opens when the extension starts. Each
  `tts_text_input` is sent as it arrives. A request is one Kalpa
  utterance: Kalpa starts speaking at the first complete sentences and keeps
  the prosody continuous across the request, and `text_input_end` ends it.
  A request longer than 8,000 characters continues in a second utterance,
  and one longer than 160,000 characters fails with `request_too_long`.
- **Interruption.** `tts_flush` drops the session and opens a new one, so no
  text or audio of the interrupted request reaches the next one. The
  interrupted request gets `tts_audio_end` with reason `INTERRUPTED`.
- **TTFB** runs from the first text of a request sent to Kalpa until its
  first audio arrives. Connection setup is not part of it.
- **Empty requests.** A request with no text, or only whitespace, ends at
  once with a `tts_audio_end` and no audio.
- **Errors.** A rejected key, a suspended account, a used-up allowance, an
  unknown voice or model, and invalid session settings are `FATAL_ERROR`;
  the extension does not retry them. Dropped connections and failed
  generations are `NON_FATAL_ERROR`: the extension reconnects with backoff,
  stops after five consecutive failures, and tries again on the next
  request. A request whose connection drops before any of
  its audio arrived is sent again, up to twice. So is one that Kalpa refuses
  as `overloaded` (at capacity, nothing generated), on a new connection
  after about one second. One that fails after audio started ends with
  reason `ERROR`, since sending it again would repeat speech.
  A request also fails, with `response_timeout`, if Kalpa sends nothing for
  30 seconds while it owes audio; the next request runs on a new session.
- **Subtitles.** Kalpa does not return word timestamps, so the extension
  sends no `tts_text_result`.

## Tests

The standalone tests run the extension against a local server that speaks
Kalpa's protocol (`tests/mock_server.py`); they need no key.

```bash
task test-extension EXTENSION=agents/ten_packages/extension/kalpa_tts_python
```

The guarder runs against the Kalpa API with `KALPA_TTS_API_KEY` set:

```bash
task tts-guarder-test EXTENSION=kalpa_tts_python CONFIG_DIR=tests/configs
```
