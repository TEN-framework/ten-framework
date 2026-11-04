# spatialreal_avatar_python

Drives a [SpatialReal](https://spatialreal.ai) avatar into the Agora channel
from a TEN graph's TTS audio.

## What it does

The extension takes the `pcm_frame` audio the TTS node produces, resamples it
to the SpatialReal session rate and streams it into a SpatialReal session
configured for Agora egress. SpatialReal's egress joins the channel as its own
user (`params.agora_uid`) and publishes two things: the TTS audio and the
avatar's facial and pose animation as an SEI track riding on a placeholder
video track. **There are no rendered video frames.** The 3D Gaussian Splatting
avatar is rendered on the viewer's side, so the viewing client must use the
SpatialReal web SDK in Agora mode; a plain Agora player shows audio and a
black 64x64 picture. Nothing flows back into the graph: the extension emits
no audio or video frames, only `flush` (forwarded downstream) and `error`.

One SpatialReal request is opened per TTS request: the first `pcm_frame` after
idle opens it and the first of data `tts_audio_end` (reason `REQUEST_END`,
`ERROR` or unknown; `INTERRUPTED` only when no `flush` has been processed since
the request opened) or data `finalize` closes it. End signals are queued in
order behind the audio that preceded them, so a request is closed only after
all of its audio has been sent. A `flush` command drops the queued audio and
end signals, interrupts the avatar and continues downstream. Within a request
audio is sent as fast as it arrives; a small amount of silence is inserted when
the sentence gap would otherwise let the server's playout buffer run dry (see
Limits). Errors are reported as `ten_ai_base.message.ModuleError` on data
`error`: `-1012` for a configuration problem, `-1` when the session is gone for
good (credits exhausted, authentication refused, reconnection given up),
`1001` while the extension is reconnecting or degraded. `vendor_info.vendor`
is `spatialreal` and `vendor_info.code` carries the server's WebSocket close
code (for example `4001`), the HTTP status of a failed session-token exchange
(`401` or `403` for a refused API key, reported once as `-1`) or the exception
class name.

## Configuration

Two top-level properties: `channel`, which the TEN server owns, and `params`,
which holds everything the operator sets. Defaults are the package
`property.json`; `${env:NAME|}` reads the variable and falls back to empty.

| Property | Env | Default | Range / notes |
|---|---|---|---|
| `channel` | — | `""` | Agora channel. Filled in by the TEN server per `/start` call; must be present (empty) in the graph node's `property`. |
| `params.spatialreal_api_key` | `SPATIALREAL_API_KEY` | `""` | **Required.** SpatialReal API key. |
| `params.spatialreal_avatar_id` | `SPATIALREAL_AVATAR_ID` | `""` | **Required.** Avatar id from the SpatialReal console. |
| `params.spatialreal_app_id` | `SPATIALREAL_APP_ID` | `""` | Optional; sent as `X-App-ID` when set. |
| `params.environment` | — | `us-west` | SDK named environment. |
| `params.console_endpoint_url` | — | `""` | Overrides the environment's console URL (test: `https://api.spatialreal.dev`). |
| `params.ingress_endpoint_url` | — | `""` | Overrides the environment's ingress URL (test: `wss://test-driven.spatialreal.dev/v2/driveningress`). |
| `params.sample_rate` | — | `24000` | One of 8000, 16000, 22050, 24000, 32000, 44100, 48000. |
| `params.egress_idle_reconnect_ms` | — | `1080000` | Idle time after which the session is reconnected so the avatar stays in the channel; must stay below the server's inactivity timeout (20 min today). |
| `params.agora_uid` | — | `""` | **Required.** Avatar uid in the channel; must parse as a non-zero integer. |
| `params.agora_token` | — | `""` | Used verbatim when set; otherwise a token is minted per connect. |
| `params.agora_appid` | `AGORA_APP_ID` | `""` | Required unless `agora_token` is set. |
| `params.agora_appcert` | `AGORA_APP_CERTIFICATE` | `""` | Empty for a project without a certificate (the App ID is then used as the token). |
| `params.agora_channel` | — | `""` | Explicit channel; overrides `channel` when set. |

`params.sample_rate` is the SpatialReal **session** rate, not the rate of the
incoming TTS audio. The TTS rate is read from each `AudioFrame` and frames at
another rate are resampled, so a 44.1 kHz TTS feeding a 24 kHz session is
fine; 24000 matches what most TTS extensions emit and is then pass-through.

A missing or invalid value is reported once at start as a `-1012` error whose
message names the key (`params.<key>`) and the extension stays idle; the
configuration is logged with the secrets masked.

## Graph

A complete avatar node. `channel` is left empty on purpose: the TEN server
overwrites it from the start request's `channel_name`, and only for nodes
whose `property` already has the key.

```json
{
  "type": "extension",
  "name": "avatar",
  "addon": "spatialreal_avatar_python",
  "extension_group": "default",
  "property": {
    "channel": "",
    "params": {
      "spatialreal_api_key": "${env:SPATIALREAL_API_KEY|}",
      "spatialreal_avatar_id": "${env:SPATIALREAL_AVATAR_ID|}",
      "spatialreal_app_id": "${env:SPATIALREAL_APP_ID|}",
      "environment": "us-west",
      "console_endpoint_url": "",
      "ingress_endpoint_url": "",
      "sample_rate": 24000,
      "egress_idle_reconnect_ms": 1080000,
      "agora_uid": "12345",
      "agora_token": "",
      "agora_appid": "${env:AGORA_APP_ID|}",
      "agora_appcert": "${env:AGORA_APP_CERTIFICATE|}",
      "agora_channel": ""
    }
  }
}
```

Three edges wire it in, the same shape the HeyGen graph uses in
`voice-assistant-advanced/tenapp/rebuild_property.py`: `flush` from
`main_control`, and `pcm_frame` plus `tts_audio_end` from `tts`. The TTS
`pcm_frame` destination moves from `agora_rtc` to `avatar`, and the matching
`pcm_frame` source entry is removed from the `agora_rtc` connection, because
the avatar's audio reaches the channel through SpatialReal's egress, not
through `agora_rtc`.

```json
[
  {
    "extension": "main_control",
    "cmd": [
      { "name": "flush", "dest": [{ "extension": "avatar" }] }
    ]
  },
  {
    "extension": "tts",
    "audio_frame": [
      { "name": "pcm_frame", "dest": [{ "extension": "avatar" }] }
    ],
    "data": [
      { "name": "tts_audio_end", "dest": [{ "extension": "avatar" }] }
    ]
  }
]
```

A TTS that signals the end of a request with data `finalize` instead of
`tts_audio_end` works as well; route `finalize` to `avatar` the same way. The
`voice_assistant_spatialreal` graph in
`ai_agents/agents/examples/voice-assistant-advanced` is the full example; set
`SPATIALREAL_API_KEY`, `SPATIALREAL_AVATAR_ID` (and optionally
`SPATIALREAL_APP_ID`) in `ai_agents/.env`.

## Development

Unit tests for the pure modules (`pacer`, `audio`, `agora_token`, `config`,
`session_client`) run on any laptop; `tests/runtime/` needs the TEN runtime
and is skipped without it:

```bash
cd ai_agents/agents/ten_packages/extension/spatialreal_avatar_python
pip install -r requirements.txt pytest pytest-asyncio
pytest tests/
```

Everything, including the runtime tests, runs inside the `ten_agent_dev`
container from `ai_agents/`:

```bash
task test-extension EXTENSION=agents/ten_packages/extension/spatialreal_avatar_python
task build-agent-deps   # once, installs every extension's requirements
task lint-extension EXTENSION=spatialreal_avatar_python
task check              # black, line length 80
```

## Limits

- `params.agora_uid` must be an integer; Agora projects that use string user
  accounts are not supported, because the egress identity is an integer uid.
- There is no "avatar finished speaking" signal in Agora mode. Playback
  progress is not reported, and back-to-back requests are spaced by a fixed
  gate (`GATE_MS`, 2000 ms in `pacer.py`) rather than by a server event, so
  two requests that arrive faster than real time see up to that much delay
  between them.
- Concurrency is counted per SpatialReal API key owner. A refused slot (close
  code `4003`) is reported as `1001` and retried on a slow schedule (30, 60,
  120, 300 s, ten attempts) before the extension gives up with `-1`.
- Padding: the server auto-ends a request after 400 ms of empty playout
  buffer, so the extension keeps a virtual playhead and inserts 100 ms
  silence chunks only after the sent audio drains (`PAD_FLOOR_MS`, 0 ms).
  It leaves positive lead alone: filling a reserve there would splice
  silence between speech chunks that arrive in time. Silence costs the same
  inference time as speech, which is why it is bounded: `PAD_MAX_MS`
  (3000 ms of contiguous padding) closes the request, and `TURN_STALL_MS`
  (15 s after real audio drains) is the outer backstop. These are constants
  in `pacer.py`.
- Audio that arrives while the session is down is queued for at most 10 s of
  playout time; older frames are dropped with one warning.
- A session is billed on connection time and holds one concurrency slot for as
  long as the extension runs, idle reconnects included.
