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
idle opens it and data `tts_audio_end` or `finalize` closes it. End signals are queued in
order behind the audio that preceded them, so a request is closed only after
all of its audio has been sent. A `flush` command drops the queued audio and
end signals, interrupts the avatar and continues downstream.

Preserve `metadata.session_id` and `metadata.turn_id` on both PCM and end
messages. TEN's TTS PCM carries this metadata, not a standard `request_id`.
Turn IDs must increase within each session, as in TEN's main controller;
the sender rejects an older turn even when its first PCM arrives late.
Different sessions have independent turn counters.
The sender rejects audio and ends from flushed or superseded turns, and an
end for another identified turn cannot close the current one. A matching
`INTERRUPTED` end closes the current request even after an earlier flush.
The `request_id` on end messages suppresses duplicate ends; completing one
TTS request does not discard other requests in the same turn. A new identified
turn closes an unfinished predecessor before its own audio is sent.

Unidentified PCM/end messages remain supported in arrival order. Without a
shared turn identity, stale events cannot be distinguished: the legacy guard
ignores `INTERRUPTED` after a flush until an end closes a request. A `finalize`
without metadata applies to the current request. PCM from two requests in the
same turn cannot be distinguished by metadata alone; keep each request's PCM
before its own end and do not interleave those requests.
If a flush arrives before any identity for the cancelled turn, and that turn's
first PCM arrives before a newer turn is known, the unscoped flush alone cannot
identify that PCM as stale. The producer must stop cancelled output.

Within a request
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
| `params.source_timeline_only` | — | `true` | Current source-audio playback with client-managed entry. Set false only for legacy/native-audio viewers. |
| `params.source_playback_profile` | — | `1` with source-only, otherwise `0` | Normally omit. Integer 1 requests `BOUNDED_BURST_V1`; integer 0 explicitly retains the earlier profile. |
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

Current integrations need neither playback setting: the extension defaults to
source-only bounded burst, and the current Web SDK detects the published profile.
The extension requires Python SDK 0.3.0 or newer for the negotiated capabilities.
With Web SDK 1.0.0-beta.6, explicitly set the viewer
`agora.sourcePlaybackProfile: "bounded-burst-v1"`; automatic viewer detection is
available in the subsequent Web SDK change.

The SDK and extension require both actual confirmation capabilities,
`agora_source_timeline_only` and `agora_source_burst_v1`, before connecting the
sender. A missing confirmation closes the session and reports a configuration
error without retry or silent fallback. Every reconnect checks again. Ordinary
transport failures retain the existing retry policy.

For legacy/native-audio viewers, set `source_timeline_only: false` and omit the
profile (or explicitly set it to zero). For the earlier source-only contract,
keep source-only enabled and set `source_playback_profile: 0`. Explicit false/zero
values are forwarded to modern Python SDKs; unsupported keywords are omitted for
older SDKs when compatibility mode is explicitly chosen. Booleans, strings and
unknown profiles are invalid.

The publisher cannot discover other viewers in the room. Updated viewers support
both profiles automatically; older viewers require the compatibility settings.
This changes the publisher default, not audio input, sample rates, sender/pacer
timing, End or interruption ordering. It does not submit an upstream TEN PR,
publish a package or deploy a runtime.

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
- Pending input is preserved in RAM within three hard limits: 120 s of PCM,
  16 MiB of PCM bytes, and 4096 audio/end items, including the item waiting
  for a connection or the previous request's gate. A 26 s TTS response can
  therefore survive a slow initial connection without a missing middle.
  These are retained-input limits, not a wall-clock reconnect deadline or
  a bound on total process RSS; resampling and transport also use buffers.
- Admission beyond any input limit cancels buffered speech and emits a
  nonfatal avatar error with vendor category `InputQueueOverflow`. It never
  discards old PCM and then reports a normally completed shortened answer.
  Identified failed turns stay retired; fresh turns can proceed after the
  interrupt completes. Older graphs without turn metadata discard that
  failed request until its ordered End or a stop/reset. If that End has a
  `request_id`, duplicates cannot close a following request; an unidentified
  End without any request ID cannot be distinguished from a new boundary.
  No input PCM is spilled to disk.
- A session is billed on connection time and holds one concurrency slot for as
  long as the extension runs, idle reconnects included.
