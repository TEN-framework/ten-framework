# fpt_mllm_python

Realtime audio-in/audio-out TEN extension for FPT Voice Agent style integrations.

## Scope

This extension follows the TEN `mllm-interface.json` contract and the implementation
shape of `openai_mllm_python`, but uses a provider-owned token flow before opening
the realtime websocket.

## Notes

- `token_url` is used to fetch an access token before each websocket session.
- `websocket_url` should be the full `ws://` or `wss://` endpoint.
- The websocket connection uses a fixed `Authorization: Bearer <token>` header.
- The PDF protocol body was not machine-readable in this environment, so the
  websocket event mapping is intentionally narrow and isolated in `realtime/struct.py`.
- If FPT uses different JSON field names than the defaults here, update only:
  - `property.json` / `manifest.json` for config fields
  - `realtime/connection.py` for auth header/query construction
  - `realtime/struct.py` for event parsing

## Properties

Refer to `manifest.json` and `property.json`.
