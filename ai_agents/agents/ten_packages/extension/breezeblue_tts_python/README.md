# breezeblue_tts_python

TEN text-to-speech extension for [BreezeBlue](https://docs.breezeblue.ai/),
using its HTTP streaming API (`POST /v1/text-to-speech/{voice_id}/stream`).

## Features

- Streaming synthesis: raw PCM16, 24 kHz, mono. No conversion is needed, the
  stream matches the TEN `pcm_frame` contract.
- Public voice catalog and your own saved/cloned voices (`voice_id`).
- Models `breeze-tts-2` (English, Chinese) and `breeze-tts-2-multilingual`.
  When `model_id` is omitted the vendor picks one from `language_code`.
- Expressive controls: `instructions`, `speed`, `volume`, `guidance_scale`,
  `use_speaker_boost`.

## Configuration

Everything goes in `params`:

- `api_key` (string, required): BreezeBlue key (`brz_...`), sent as the
  `xi-api-key` header. Create one at
  <https://breezeblue.ai/app/developer/api-keys>.
- `voice_id` (string, required): voice to synthesize with. The voice is part of
  the request path and there is no server-side default. List the public
  catalog with:
  `curl -H "xi-api-key: $BREEZE_API_KEY" "https://api.breeze.blue/v1/voices?voice_type=default&language_code=en"`
- `model_id` (string): defaults to the vendor's automatic choice.
- `language_code` (string): ISO 639-1 two-letter code, e.g. `en`, `zh`.
- `instructions` (string): performance direction, in the language of the text.
- `speed` (0.5-2.0), `volume` (0.01-2.0), `guidance_scale` (1.0-10.0),
  `use_speaker_boost` (bool): may also be nested under `voice_settings`. Flat
  keys win over nested ones.
- `base_url` (string): API base (default `https://api.breeze.blue`).

Any other key in `params` is ignored. The vendor rejects unknown request
fields with HTTP 422, so only the fields above are forwarded. The frontend's
generic `sample_rate` is dropped for the same reason (output is fixed at
24 kHz).

```json
{
  "type": "extension",
  "name": "tts",
  "addon": "breezeblue_tts_python",
  "extension_group": "tts",
  "property": {
    "params": {
      "api_key": "${env:BREEZE_API_KEY}",
      "voice_id": "${env:BREEZE_TTS_VOICE_ID}",
      "language_code": "en"
    }
  }
}
```

## Limits and behavior

- Text is limited to 1000 characters per request by default (up to 2000 for
  approved accounts). Longer text is rejected, not truncated.
- `speed` and `volume` apply to HTTP streaming (not to the realtime WebSocket,
  which this extension does not use).
- Billing is per character: 10 units = 1 credit, and CJK characters count as 2
  units.
- A new account is limited to 2 concurrent generations.
- 401/403 responses are reported as an invalid-key error. Other HTTP errors,
  such as 402 (no credits), 422 and 429, are reported as general errors with
  the vendor's `detail` message.

## Tests

Unit tests (`tests/test_config.py`, `tests/test_client.py`) mock the HTTP layer.
The TTS guarder needs `BREEZE_API_KEY` and `BREEZE_TTS_VOICE_ID` in
`ai_agents/.env`:

```bash
task tts-guarder-test EXTENSION=breezeblue_tts_python CONFIG_DIR=tests/configs
```

## Architecture

- **Extension**: `BreezeBlueTTSExtension` extends `AsyncTTS2HttpExtension`.
- **Config**: `BreezeBlueTTSConfig` extends `AsyncTTS2HttpConfig`.
- **Client**: `BreezeBlueTTSClient` extends `AsyncTTS2HttpClient` and streams
  the response with httpx (HTTP/2, shared SSL context).
