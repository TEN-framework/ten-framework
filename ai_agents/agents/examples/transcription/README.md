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

-Run
1) From repo root: `cd ai_agents` then run `task use AGENT=transcription`.
2) Still inside `ai_agents`, start the runtime with `task run` (leave it running).
3) In a new terminal, `cd agents/examples/transcription/web`, run `pnpm install` (first time) and `pnpm dev`, then open `http://localhost:3000`.

Notes
- The UI calls `/api/agents/start` (proxied server-side) with `graph_name=transcription`, then joins Agora and publishes microphone audio.
- Transcripts are streamed back via Agora RTC `stream-message`; the UI assembles chunked payloads and renders raw vs corrected text side by side.
