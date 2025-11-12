Transcription Example

Overview
- A minimal transcription pipeline using Agora RTC for audio ingress, Deepgram (or configured STT) for speech-to-text, and an OpenAI LLM pass to clean up the transcript before it is shown in the UI.

Folders
- `property.json`: Defines the graph `transcription` wiring Agora → streamid_adapter → STT → main_python → message_collector → Agora data.
- `manifest.json`: App manifest and dependencies.
- `ten_packages/extension/main_python`: Control extension that forwards raw ASR results and, on final utterances, sends them through an LLM prompt to produce a corrected transcript.
- `web`: Minimal Next.js UI for joining channel and viewing transcripts.

-Required Env
- In repo `.env` (server):
  - `AGORA_APP_ID=...`
  - `AGORA_APP_CERTIFICATE=...` (if token requires)
  - `DEEPGRAM_API_KEY=...` or use alternative STT credentials per your addon
  - `OPENAI_API_KEY=...` (+ optional `OPENAI_MODEL`, `OPENAI_BASE_URL`, `OPENAI_PROXY_URL`)
  - Optional: `STT_LANGUAGE=en-US`
- In `web/.env`:
  - `AGENT_SERVER_URL=http://localhost:8080` (TEN server URL)

## Quick Start

1. **Install dependencies:**
   ```bash
   task install
   ```

2. **Run the transcription service:**
   ```bash
   task run
   ```

3. **Access the application:**
   - Web UI: http://localhost:3000
   - API Server: http://localhost:8080
   - TMAN Designer: http://localhost:49483

## Available Tasks

- `task install` - Install all dependencies
- `task run` - Start all services
- `task release` - Build release package

## Notes

- The UI calls `/api/agents/start` (proxied server-side) with `graph_name=transcription`, then joins Agora and publishes microphone audio.
- Transcripts are streamed back via Agora RTC `stream-message`; the UI assembles chunked payloads and renders raw vs corrected text side by side.
