# fpt_mllm_python

Realtime audio-in/audio-out TEN extension for FPT Voice Agent integrations.

## Scope

This extension follows the TEN `mllm-interface.json` contract, but the transport
is implemented around FPT's websocket bridge protocol rather than OpenAI-style
realtime events.

## Notes

- `token_url` should point to the base `.../get-token` endpoint. The extension
  appends `agent_type` and `agent_id` as query parameters and fetches it with
  `GET` plus `x-api-key`.
- `websocket_url` should be the full `ws://` or `wss://` endpoint.
- The websocket auth flow is:
  - connect websocket
  - send `{"type":"auth", ...}`
  - wait for `auth_success`
  - send `{"type":"bridge_connect","call_id":"..."}`
- Audio is sent as binary websocket frames.
- Set `dump` to `true` to log all JSON websocket traffic and token fetches. Audio
  frames are intentionally summarized as byte counts instead of printing payloads.
- The message parsing is intentionally narrow and isolated in `realtime/struct.py`.
- Unsupported `mllm` features such as tool calling and explicit response creation
  are logged and ignored by this minimal bridge.

## Properties

Refer to `manifest.json` and `property.json`.
