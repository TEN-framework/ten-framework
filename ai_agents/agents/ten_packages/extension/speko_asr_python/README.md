# Speko Router ASR extension

Streams TEN PCM audio to the Speko Router and emits the standard TEN
`asr_result`, `asr_finalize_end`, `metrics`, `error`, and connection-status
messages.

## Configuration

Set `SPEKO_API_KEY`, then use the extension defaults or override `params` in
your graph.

| Property | Default | Description |
| --- | --- | --- |
| `params.base_url` | `https://router.speko.dev` | Speko Router origin |
| `params.sample_rate` | `16000` | PCM input sample rate |
| `params.channels` | `1` | PCM input channel count |
| `params.language` | `en-US` | Language tag for routing/transcription |
| `params.routing` | auto, balanced | Speko routing selection |
| `params.options` | `{}` | Diarization, keywords, noise reduction, or namespaced provider options |
| `params.buffer_duration_ms` | `5000` | Audio retained while disconnected, `0`–`30000`; `0` discards |
| `params.max_reconnect_attempts` | `0` | Reconnect attempts before a fatal `retry_exhausted`; `0` retries until stop |

The disconnected buffer is held in memory and is also capped at 10 MiB, which
binds before 30 seconds at high sample rates or channel counts. When a frame
would overflow it, the oldest buffered frames are dropped (a frame larger than
the whole buffer is dropped itself). Dropped frames are never sent; they are
recorded as dropped audio in the TEN audio timeline, so later transcript
timestamps still line up with the caller's audio.

The buffer holds one session at a time. A frame whose metadata `session_id`
differs from the held frames' drops them, and a finalize while disconnected
drops the finished turn's held frames, both recorded as dropped audio. Frames
still queued when the session becomes ready follow the same rule: the replay
after reconnecting never merges one session's speech into another's.

The transport is raw signed 16-bit little-endian PCM. A TEN `asr_finalize`
message sends Speko `input.commit` when any audio was sent since the last
commit. Each finalize is serialized and acknowledged with its own `finalize_id`
and metadata, including disconnected and timeout paths. Each commit carries a
fresh `commit_id`. When `session.ready` has `"commit_ids": true`, the Router
answers each commit with exactly one `transcript.final` stamped with that
`commit_id` (empty when the provider had nothing left to finalize). Only that
final completes the finalize; a final without a `commit_id` is the provider's
own endpoint and is emitted as a normal result. If no answer arrives within
`finalize_timeout_sec`, the extension logs a warning, keeps the session, and
still completes the finalize, and the next turn's results take a new result
id. A late answer is reported as the timed-out turn's final, with that turn's
metadata, result id and audio timestamps. It never completes another finalize
or takes its TTLW. At most three timed-out commits wait for a late answer: a
timeout beyond that means the Router has stopped answering, so the session is
reset with one non-fatal error, and answers to those commits are never
reported.
Without that flag the Router answers commits in order: finals are paired first
in, first out, and a commit timeout resets the session so later answers cannot
be paired one behind.
Empty vendor transcripts are not emitted as user turns. TTLW comes from the
first non-empty final after `asr_finalize`; when the commit is acknowledged
without one (already-finalized input or an empty final), TTLW is the latency to
that acknowledgement. A commit timeout or disconnect reports no TTLW.
Each `session.ready` logs the Router `request_id` and the route, including its
`attempt_id`, for support reports.

```json
{
  "type": "extension",
  "name": "asr",
  "addon": "speko_asr_python",
  "extension_group": "asr",
  "property": {
    "params": {
      "api_key": "${env:SPEKO_API_KEY}",
      "routing": {"mode": "auto", "objective": "latency"},
      "language": "en-US"
    }
  }
}
```

For an explicit route, set `routing` to
`{"mode":"explicit","provider":"deepgram","model":"nova-3"}`.


## Testing

From the extension directory, with TEN runtime/base packages installed and
available on `PYTHONPATH`, run the client, extension lifecycle, and package
contract regressions:

```bash
./tests/bin/start
```

From `ai_agents/`, set `SPEKO_API_KEY` and run the live guarder:

```bash
task asr-guarder-test EXTENSION=speko_asr_python
```

Run ASR and TTS guarders sequentially. The guarder uses `tests/configs` and
contacts the hosted Router; it requires a funded key and an available route.
For reproducible provider checks, copy the configs to a temporary directory,
set `params.routing` to an explicit supported provider/model, then invoke the
installed guarder’s `tests/bin/start` with `--extension_name speko_asr_python`
and `--config_dir /absolute/path/to/configs`. Keep invalid-key configs invalid.
Client/lifecycle unit tests use mocked transport and do not require credentials.

Transient disconnects have one background recovery loop with delays of 0.5, 1,
2, and then 4 seconds per attempt, retrying until stop unless
`max_reconnect_attempts` sets a ceiling. A successful handshake resets the
attempt count. A normal close (WebSocket 1000/1001 or `session.closed`) reports
`disconnected` and recovers without an `error`; an abnormal close or a Router
error reports one. An exhausted ceiling, invalid configuration,
authentication/credit failures, unavailable explicit routes, and any handshake
rejection the Router marks non-retryable (for example HTTP 400 on upgrade, or
an `error` event before `session.ready` with `retryable: false`) report one
fatal error and stop further attempts until the cause is corrected and the
extension is restarted. Failed sends are not replayed; disconnected input uses the bounded
buffer. Stop cancels pending recovery and finalization.

Finalization waits for an in-progress handshake and previously accepted audio
to drain through the base consumer before committing. Later ingress waits until
that finalize completes, preserving audio order and the request's metadata.

Segment and word timestamps are mapped onto TEN's user-audio timeline, retaining
position across connection replacement and excluding protocol silence. The raw
input dump records each ingress frame once, including buffered input.

Run standalone tests before guarders using the same image as CI:

```bash
task test-extension EXTENSION=agents/ten_packages/extension/speko_asr_python
task check
task lint
```

The standalone suite inspects emitted TEN JSON for results, finalize context,
metrics, status transitions, and redaction. Endpoint logs omit URL userinfo,
query parameters, and fragments. The single `key` vendor metadata field is
masked by TEN's reporting layer.
