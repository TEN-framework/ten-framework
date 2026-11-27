# Xfyun Dialect ASR

As of: 2026-11-26 16:22:08 UTC (+0000)
Codebase commit: `d1402a0841aed43642870ef22ad67674dde7ef0e`

This extension uses the Xfyun dialect WebSocket recognition API. Configure
credentials through `params.app_id`, `params.access_key_id`, and
`params.access_key_secret` using environment variables.

`params.language` sets the language label on ASR results. The supplier
recognition mode is configured separately through `params.lang` and
`params.recognized_language`. For example, the Singapore endpoint
`api-sgp.iflyaisol.com` can use `lang=autominor_pgs` and
`recognized_language=cn` for Chinese recognition when enabled for the account.

The ASR base reports `connecting` for each connection attempt. The supplier's
`started` event reports `connected`; failed handshakes and closed connections
report `disconnected`. A message-processing error on a live connection does
not report a disconnection.

One background task owns reconnection. Transient failures retry with
exponential backoff from 300 ms to a maximum of 4.8 seconds, without a retry
count limit. Stop cancels that task before closing the current connection.
Callbacks from an earlier connection cannot change the replacement's state.
