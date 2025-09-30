# Voice Assistant (Speech Diarization)

This variant of the voice assistant swaps in the Speechmatics realtime engine with speaker diarization enabled. It keeps the same LLM/TTS/tooling pipeline as the default assistant, but every speech turn now carries `metadata.diarization` with per-speaker segments.

## Prerequisites

Set the following environment variables in `ai_agents/.env` (or your shell):

- `SPEECHMATICS_API_KEY` – required for the Speechmatics realtime ASR.
- `AGORA_APP_ID` / `AGORA_APP_CERTIFICATE` – for the RTC ingress/egress.
- Existing keys used by the base assistant (`OPENAI_API_KEY`, `ELEVENLABS_TTS_KEY`, etc.).

The `property.json` file contains a starter `additional_vocab` list you can edit to bias diarization toward known speaker names.

## Run

```bash
cd ai_agents
task use AGENT=voice-assistant-diarization
task run
```

Connect with your usual TEN client (e.g. the playground). As soon as audio comes in, the worker log prints entries such as:

```
[MainControlExtension] diarization speaker=S1 channel=None text=hello there
send_asr_result: ... "metadata": { "diarization": { "segments": [...] }}
```

Those lines confirm diarization is active. Downstream, the message collector now forwards the full metadata bundle so UI clients see per-speaker transcripts.

## Notes

- `speaker_diarization_config` in `property.json` controls sensitivity and the number of concurrent speakers.
- `additional_vocab` accepts objects with `content` and `sounds_like` arrays (mirroring the Speechmatics SDK). Add real names there to stabilise labels.
- To revert to the classic assistant, run `task use AGENT=voice-assistant` instead.
