Speechmatics Diarization Web (Next.js)

Standalone UI for the Speechmatics diarization agent. Launches the `diarization_demo` graph, joins Agora with microphone audio only, streams ASR transcripts, and plays back LLM+TTS replies for each identified speaker.

Setup
- In this folder, create `.env` with `AGENT_SERVER_URL=http://localhost:8080` (or your TEN server base URL).
- From the repo root run `task use AGENT=speechmatics-diarization` so the server exposes this graph.
- Ensure server-side `.env` at repo root has `AGORA_APP_ID`, Speechmatics, ElevenLabs, and OpenAI keys configured.

Run
- Copy `.env.example` to `.env` and set `AGENT_SERVER_URL`.
- `pnpm i` or `npm i`
- `pnpm dev` or `npm run dev`
- Visit http://localhost:3000

Notes
- Start triggers POST `/start` on `AGENT_SERVER_URL` with graph `diarization_demo` and sets Agora to subscribe to your browser user ID.
- Mic audio publishes via Agora RTC; message collector data (user + assistant turns) streams back via RTC `stream-message`.
- Remote Agora audio from the agent is auto-subscribed so ElevenLabs TTS replies play through your speakers.
