Transcription Example

Overview
- A minimal transcription pipeline using Agora RTC for audio ingress, Deepgram (or configured STT) for speech-to-text, and a lightweight Next.js UI to display transcripts.

Folders
- `property.json`: Defines the graph `transcription` wiring Agora → streamid_adapter → STT → main_python → message_collector → Agora data.
- `manifest.json`: App manifest and dependencies.
- `ten_packages/extension/main_python`: Control extension that forwards ASR results to message collector. No LLM required.
- `web`: Minimal Next.js UI for joining channel and viewing transcripts.

Required Env
- In repo `.env` (server):
  - `AGORA_APP_ID=...`
  - `AGORA_APP_CERTIFICATE=...` (if token requires)
  - `DEEPGRAM_API_KEY=...` or use alternative STT credentials per your addon
  - Optional: `STT_LANGUAGE=en-US`
- In `web/.env`:
  - `AGENT_SERVER_URL=http://localhost:8080` (TEN server URL)

Run
1) Start the TEN server with this agent: set `AGENT=agents/examples/transcription` and run your normal dev command (see project README).
2) UI: in `web` run `npm i` then `npm run dev` and open `http://localhost:3000`.

Notes
- The UI calls `/api/agents/start` (proxied server-side) with `graph_name=transcription`, then joins Agora and publishes microphone audio.
- Transcripts are streamed back via Agora RTC `stream-message`; the UI assembles chunked payloads and renders them.

