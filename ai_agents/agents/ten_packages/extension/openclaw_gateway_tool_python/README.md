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
- `chat_session_key`
- `request_timeout_ms`
- `connect_timeout_ms`
