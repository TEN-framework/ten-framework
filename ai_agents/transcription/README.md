# TEN Transcription (Independent UI)

This folder contains an independent, minimal UI for recording audio with a live waveform and sending it for transcription + LLM correction. By default, it uses local mock APIs so you can see the UI and flow without any keys.

If you want a full end‑to‑end transcription powered by the TEN Framework (RTC/RTM + graphs), see the “Use with TEN Framework” section below.

## Quick Start (Mock STT/LLM)

1) Run the app

```
cd ai_agents/transcription
pnpm install
pnpm dev
```

Open http://localhost:3000

2) Use it
- Click the circular “Rec” button to start/stop recording
- A waveform animates while recording
- Click “Send to Model” to invoke mock STT then mock LLM correction
- Results render as “Raw Transcript” and “Corrected”

Notes
- No keys are required in this mode (local mock endpoints)
- Uses MediaRecorder (`audio/webm`) and an `AnalyserNode` for the waveform

## Use with TEN Framework (End‑to‑End)

The end‑to‑end transcription pipeline is provided by the TEN Agent server and graphs under `ai_agents/agents`. This UI is intentionally independent and does not directly join RTC/RTM yet. To run the full TEN pipeline:

1) Put keys in `ai_agents/.env` (you already did)
- `AGORA_APP_ID=...`
- `AGORA_APP_CERTIFICATE=...` (if required)
- `SERVER_PORT=8080`
- `LOG_PATH=./logs`
- `LOG_STDOUT=true`
- `WORKERS_MAX=4`
- `WORKER_QUIT_TIMEOUT_SECONDES=60`
- `DEEPGRAM_API_KEY=...` (STT)
- `OPENAI_API_KEY=...` (LLM)
- `OPENAI_MODEL=gpt-4o-mini` (or the model you have access to)

2) Ensure the server reads the transcription graph
- The server reads `ai_agents/agents/property.json`, which in this repo is already set to a single graph named `transcription` (based on `examples/transcription/property.json`).

3) Start the server from the repo’s `ai_agents/` directory so it loads `ai_agents/.env`

```
cd ai_agents
./server/bin/api
```

4) Start a client to publish mic audio and view RTM transcripts
- Option A: Use the demo app in `ai_agents/demo` (recommended)
  - Ensure `ai_agents/demo/.env` has `AGENT_SERVER_URL=http://localhost:8080`
  - `cd ai_agents/demo && pnpm install && pnpm dev`
  - Open http://localhost:3000 and join a channel
  - Start the transcription graph via curl (below), the demo will receive RTM transcript messages
- Option B: Any other RTC/RTM client that joins the same channel

5) Start the graph (curl example)

Get `channel` and `userId` from your client (e.g., the demo header). Then:

```
curl 'http://localhost:8080/start' \
  -H 'Content-Type: application/json' \
  --data-raw '{
    "request_id": "test-1",
    "channel_name": "<your_channel>",
    "user_uid": <your_userId>,
    "graph_name": "transcription"
  }'
```

The server graph subscribes to audio and publishes transcript messages via RTM.

### Why this UI is independent
This app showcases a clean, focused recording UX. It runs out‑of‑the‑box without credentials (mock mode). For TEN‑powered transcription, use the TEN server + a client that joins RTC/RTM. If you prefer, we can evolve this app to join Agora RTC and subscribe to RTM directly, but it’s currently not wired to TEN by design.

## Extending the Mock APIs
- `src/app/api/stt/route.ts`: replace mock logic with your STT provider call (OpenAI Whisper, Azure Speech, etc.)
- `src/app/api/llm/route.ts`: replace mock logic with your LLM correction/proofreading call
- For secret keys, use a local `ai_agents/transcription/.env.local` (not committed)

