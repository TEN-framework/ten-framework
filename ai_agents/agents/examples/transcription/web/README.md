Transcription Web (Next.js)

Quick, minimal UI to start the `transcription` graph, join Agora, publish mic audio, and display streaming transcripts.

Setup
- In this folder, create `.env` with `AGENT_SERVER_URL=http://localhost:8080` (or your TEN server base URL).
- Ensure server-side `.env` at repo root has `AGORA_APP_ID` and STT keys configured for the transcription agent.

Run
- Copy `.env.example` to `.env` and set `AGENT_SERVER_URL`.
- `pnpm i` or `npm i`
- `pnpm dev` or `npm run dev`
- Visit http://localhost:3000

Notes
- Start triggers POST `/start` on `AGENT_SERVER_URL` with graph `transcription`.
- Mic audio publishes via Agora RTC; transcripts stream back via RTC `stream-message` and are assembled client-side.
