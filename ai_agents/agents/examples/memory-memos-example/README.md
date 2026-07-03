# Voice Assistant (with MemOS)

A voice assistant enhanced with MemOS for persistent, personalized memory. The agent searches related memories before LLM responses and saves each turn afterward.


## Environment Variables

Set these in your shell or in `.env` at the repo root:

- `OPENAI_API_KEY`
- `ELEVENLABS_TTS_KEY` (TTS)
- `DEEPGRAM_API_KEY` (ASR)
- `AGORA_APP_ID` and optionally `AGORA_APP_CERTIFICATE` (RTC)
- `MEMOS_API_KEY` (MemOS)

Example:

```bash
export OPENAI_API_KEY=sk-...
export ELEVENLABS_TTS_KEY=...
export DEEPGRAM_API_KEY=...
export AGORA_APP_ID=...
export MEMOS_API_KEY=...
# Optional (defaults to the value in the extension property file):
export MEMOS_BASE_URL=https://memos.memtensor.cn/api/openmem/v1
```

Notes:
- The example defaults `memos_base_url` to `https://memos.memtensor.cn/api/openmem/v1` at `agents/examples/memory-memos-example/tenapp/ten_packages/extension/main_python/property.json:7`. You can override via `MEMOS_BASE_URL`.

## MemOS Setup

- Docs: https://memos-docs.openmem.net/
- Quick trial available via their cloud service.
- Obtain an API key, then set `MEMOS_API_KEY` (and optionally `MEMOS_BASE_URL`).

This example uses the MemOS endpoints:
- `POST {MEMOS_BASE_URL}/add/message` with `Authorization: Token <MEMOS_API_KEY>`
- `POST {MEMOS_BASE_URL}/search/memory` with `Authorization: Token <MEMOS_API_KEY>`

## Quick Start

From this directory:

1. Install dependencies
   ```bash
   task install
   ```

2. Run everything (API server, frontend, TMAN Designer)
   ```bash
   task run
   ```

3. Access the app
- Frontend: http://localhost:3000
- API Server: http://localhost:8080
- TMAN Designer: http://localhost:49483

## How It Works

- Before each LLM request, the agent retrieves related memory and merges it into the LLM prompt (see `agents/examples/memory-memos-example/tenapp/ten_packages/extension/main_python/extension.py:298`).
- After each final LLM response, the agent stores the current turn (user + assistant) in MemOS (see `agents/examples/memory-memos-example/tenapp/ten_packages/extension/main_python/extension.py:333`).

## Troubleshooting

- If memory isn’t applied: ensure `MEMOS_API_KEY` is set and `MEMOS_BASE_URL` is reachable.
- If audio isn’t working: verify `AGORA_APP_ID`, `DEEPGRAM_API_KEY`, and `ELEVENLABS_TTS_KEY`.
- Check runtime logs in the terminal for MemOS calls and errors.
