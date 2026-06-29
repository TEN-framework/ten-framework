# mistral_tts_python

Mistral (Voxtral) TTS Extension for TEN Framework — text-to-speech synthesis
using Mistral's OpenAI-compatible HTTP API (`POST /v1/audio/speech`).

## Features

- Streaming TTS synthesis via Mistral's `/v1/audio/speech` endpoint
- Voxtral models (e.g. `voxtral-mini-tts-2603`)
- Preset voices (`voice`), saved cloned voices (`voice_id`), or one-off
  reference clips (`ref_audio`) — forwarded straight through to the vendor
- Streaming float32 → PCM16 mono conversion of Voxtral's raw `pcm` output, at
  24 kHz
- Optional audio dumping for debugging
- API-key authentication via the `Authorization` header

## Configuration

### Top-level Properties

- `dump` (bool): Enable audio dumping for debugging (default: false)
- `dump_path` (string): Path to save dumped audio (default: extension dir +
  `mistral_tts_in.pcm`)

### TTS Parameters (under `params`)

- `api_key` (string, required): Mistral API key
- `model` (string): Voxtral model (default: `voxtral-mini-tts-2603`)
- `voice_id` / `voice` (string): The voice to synthesize with. The cloud API
  (`api.mistral.ai`) uses `voice_id`; the self-hosted vLLM-Omni server uses
  `voice`. Both are forwarded unchanged, so set whichever your deployment
  expects. Available voices vary by account/plan — list them with:
  `curl -H "Authorization: Bearer $MISTRAL_API_KEY" https://api.mistral.ai/v1/audio/voices`
  (e.g. `en_paul_neutral`, `gb_oliver_neutral`).
- `base_url` (string): API base (default: `https://api.mistral.ai/v1`)

> The live cloud API **requires** a voice (or a one-off `ref_audio` clip): a
> request with neither is rejected with HTTP 400. This extension does not
> enforce it locally — whatever you put in `params` is forwarded to the vendor
> unchanged (including `ref_audio`) — so make sure your config sets one.
> `response_format` is always set to `pcm` internally and converted to PCM16.
>
> The bundled test configs read the voice from `${env:MISTRAL_TTS_VOICE}` (e.g.
> `en_paul_neutral`); set that env var when running the guarder tests.

### Example Configuration

```json
{
  "dump": false,
  "dump_path": "/tmp/mistral_tts_dump",
  "params": {
    "api_key": "${env:MISTRAL_API_KEY}",
    "model": "voxtral-mini-tts-2603",
    "voice_id": "en_paul_neutral"
  }
}
```

## Notes

- Mistral's TTS API applies content moderation; disallowed input is rejected
  with HTTP 403. The extension surfaces this as an error event.
- Mistral's raw `pcm` format is headerless float32 LE at 24 kHz mono, so this
  extension requests `pcm` (lowest latency — no container header to buffer) and
  rescales each sample to the PCM16 mono that the TEN `pcm_frame` contract
  expects.

## Architecture

This extension follows the `AsyncTTS2HttpExtension` pattern:

- **Extension**: `MistralTTSExtension` — inherits from `AsyncTTS2HttpExtension`
- **Config**: `MistralTTSConfig` — extends `AsyncTTS2HttpConfig`
- **Client**: `MistralTTSClient` — extends `AsyncTTS2HttpClient`, handles the
  Mistral API call and float32 → PCM16 conversion (`Float32ToPcm16`)

## API Reference

Refer to the `api` definition in [manifest.json](manifest.json) and default
values in [property.json](property.json).
