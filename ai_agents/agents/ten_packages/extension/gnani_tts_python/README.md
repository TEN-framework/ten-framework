# Gnani AI Timbre TTS

Set `GNANI_API_KEY` in the voice-assistant process environment. In local TEN
Designer, replace the TTS node with `gnani_tts_python`. Edit the node's
`params` to select a Timbre model, its voice, language and sample rate.

| Model | Example voice | Example language |
| --- | --- | --- |
| timbre-v3.0 (default) | Shivani | en-IN |
| timbre-v2.5 | Nalini | hi-IN |

The extension streams raw mono signed 16-bit PCM over HTTPS using the Gnani
SSE endpoint. The default sample rate is 24000 Hz. Supported rates are 8000,
16000, 22050, 24000, 44100 and 48000 Hz. Select a voice from the selected
model's catalog. TEN handles request IDs, flush, audio lifecycle, metrics and
optional PCM dumps. The provider stream supplies audio without word alignment.

[Streaming protocol](https://docs.gnani.ai/api/TTS/tts-sse) ·
[Voice catalog](https://docs.gnani.ai/api/TTS/available-voices)

From `ai_agents` inside the TEN development container:

```sh
task lint-extension EXTENSION=gnani_tts_python
task test-extension EXTENSION=agents/ten_packages/extension/gnani_tts_python
task tts-guarder-test EXTENSION=gnani_tts_python
```

Set the API key to include both authenticated standalone tests and run Guarder.
