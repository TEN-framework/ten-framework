# openclaw_gateway_tool_python

TEN tool extension that connects to an OpenClaw gateway over WebSocket.

## What it does

- Registers `claw_task_delegate(summary)` as an LLM tool
- Sends the summary to OpenClaw immediately and returns an ack tool result
- Emits asynchronous `openclaw_reply_event` data when OpenClaw produces a final reply

## Properties

- `gateway_url`
- `gateway_token`
- `gateway_password`
- `gateway_scopes`
- `gateway_client_id`
- `gateway_client_mode`
- `gateway_origin`
- `gateway_device_identity_path`
- `chat_session_key`
- `request_timeout_ms`
- `connect_timeout_ms`

## Troubleshooting

- `origin not allowed`:
  - The extension forwards `gateway_origin` as the websocket `Origin` header.
  - Use the gateway host origin (for example `http://host.docker.internal:18789`) and allow it in `gateway.controlUi.allowedOrigins`, or enable host-header fallback on the gateway.

- `missing scope: operator.write`:
  - This extension now sends signed device identity during connect. Keep identity file persistent across restarts.
  - If pairing is required, the extension emits `openclaw_reply_event` with approval commands and the frontend can show copyable commands.
