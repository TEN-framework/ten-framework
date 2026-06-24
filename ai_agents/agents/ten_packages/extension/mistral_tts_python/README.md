# mistral_tts_python

Mistral (Voxtral) TTS Extension for TEN Framework — text-to-speech synthesis
using Mistral's OpenAI-compatible HTTP API (`POST /v1/audio/speech`).

## Features

- Streaming TTS synthesis via Mistral's `/v1/audio/speech` endpoint
- Voxtral models (e.g. `voxtral-mini-tts-2603`)
- Preset voices (`voice`), saved cloned voices (`voice_id`), or one-off
  reference clips (`ref_audio`) — forwarded straight through to the vendor
- Robust WAV → PCM16 mono conversion (handles int16 / int24 / int32 and
  IEEE-float WAV payloads), output at 24 kHz
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
- `voice_id` (string): A preset voice (e.g. `casual_male`, `jane_confident`)
  or a saved/cloned voice id — this is the cloud API's voice field
- `base_url` (string): API base (default: `https://api.mistral.ai/v1`)

> A voice is not required by this extension: Voxtral accepts a preset or saved
> `voice_id`, or a one-off `ref_audio` clip, and otherwise uses a default
> voice. Any extra params you set (e.g. `ref_audio`) are forwarded to the
> vendor unchanged. `response_format` is always set to `wav` internally and
> converted to PCM16.
>
> Note: the cloud API (`api.mistral.ai`) uses `voice_id`. The self-hosted
> vLLM-Omni server uses `voice` instead — both pass through unchanged, so set
> whichever your deployment expects.

### Example Configuration

```json
{
  "dump": false,
  "dump_path": "/tmp/mistral_tts_dump",
  "params": {
    "api_key": "${env:MISTRAL_API_KEY}",
    "model": "voxtral-mini-tts-2603",
    "voice_id": "casual_male"
  }
}
```

## Notes

- Mistral's TTS API applies content moderation; disallowed input is rejected
  with HTTP 403. The extension surfaces this as an error event.
- Mistral's raw `pcm` format is float32 LE, so this extension requests `wav`
  (self-describing) and converts to the PCM16 mono that the TEN `pcm_frame`
  contract expects.

## Architecture

This extension follows the `AsyncTTS2HttpExtension` pattern:

- **Extension**: `MistralTTSExtension` — inherits from `AsyncTTS2HttpExtension`
- **Config**: `MistralTTSConfig` — extends `AsyncTTS2HttpConfig`
- **Client**: `MistralTTSClient` — extends `AsyncTTS2HttpClient`, handles the
  Mistral API call and WAV → PCM16 conversion (`WavToPcm16`)

## API Reference

Refer to the `api` definition in [manifest.json](manifest.json) and default
values in [property.json](property.json).
