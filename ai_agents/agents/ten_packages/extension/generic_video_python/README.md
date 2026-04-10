# generic_video_python

TEN avatar/video extension for ConvoAI-compatible providers using REST session
setup plus a WebSocket audio stream.

## Features

- starts and stops remote avatar sessions using the current ConvoAI contract
- sends `init`, `voice`, `voice_end`, `voice_interrupt`, and `heartbeat`
- forwards the actual incoming TEN audio frame sample rate without resampling
- supports dynamic channel injection via the canonical `channel` property
- masks API keys in config logging

## API

Refer to the `api.property.properties` schema in
[manifest.json](manifest.json) and defaults in [property.json](property.json).

Canonical properties:
- `channel`
- `agora_avatar_uid`
- `generic_video_api_key`
- `avatar_id`
- `quality`
- `version`
- `video_encoding`
- `enable_string_uid`
- `activity_idle_timeout`
- `area`
- `start_endpoint`
- `stop_endpoint`
- `input_audio_sample_rate`
- `params`

Backward-compatible aliases still accepted by the loader:
- `agora_channel_name` -> `channel`
- `agora_video_uid` -> `agora_avatar_uid`

Vendor passthrough:
- `params` can contain vendor-specific top-level fields
- known keys are normalized onto the named config fields first
- unknown keys are forwarded as top-level keys in both the session start body
  and the WebSocket `init` message
- `api_key` inside `params` is accepted as an alias for
  `generic_video_api_key` and is not forwarded downstream

## Protocol Notes

- REST start payload includes `area`
- WebSocket `init` payload includes `area`
- stop requests send both `session_id` and `session_token` in the DELETE body
- WebSocket auth uses `Authorization: Bearer {session_token}`

## Development

### Unit test

```bash
tests/bin/start
```

## Misc

This package is validated against the checked-out `convoai_to_video`
reference contract, but the tests use local fixtures and mocks rather than
importing that repo directly.
