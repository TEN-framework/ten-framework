# TEN Agent Demo (with UI)

This example bundles the agent graphs and the Next.js demo UI together.

## Layout

- Agent configs and extensions: `ai_agents/agents/examples/demo`
- Next.js UI: `ai_agents/agents/examples/demo/web`

## Prerequisites

- Node.js 20+
- A package manager (pnpm recommended as per `package.json`)
- Backend server environment variables (e.g. `AGORA_APP_ID`, Azure keys, etc.)

## Start the backend server

1. Point the active agent to this example (optional, but recommended):
   - From repo root: `task -d ai_agents use AGENT=agents/examples/demo`
2. Run the Go API server:
   - From repo root: `task -d ai_agents run-server`

The server exposes `/start`, `/stop`, `/ping`, `/token/generate`, and vector endpoints.

## Start the UI

1. In another terminal:
   - `cd ai_agents/agents/examples/demo/web`
   - `pnpm install`
   - `pnpm dev`
2. Ensure the UI knows where the backend is:
   - `web/.env` contains `AGENT_SERVER_URL=http://localhost:8080` by default. Adjust if your server runs elsewhere.

## How it connects

- The UI proxies `/api/agents/*`, `/api/vector/*`, and `/api/token/*` to `AGENT_SERVER_URL` (see `web/src/middleware.tsx`).
- `web/src/app/api/agents/start/route.ts` constructs graph properties and POSTs to `${AGENT_SERVER_URL}/start`.
- `web/src/manager/rtc/rtc.ts` joins the Agora channel, publishes mic/camera, and consumes text/audio from the agent.

## Notes

- The UI was relocated from `ai_agents/demo` to `ai_agents/agents/examples/demo/web` to co-locate with the graphs.
- If you previously used the old path in scripts or docs, update those references to the new location.

