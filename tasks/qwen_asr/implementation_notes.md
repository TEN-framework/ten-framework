# Qwen ASR WebSocket implementation

## Scope

Implemented `qwen_asr_python` for `qwen-audio-3.1-asr-flash-streaming` with the
Qwen-Audio-ASR-Streaming duplex WebSocket protocol.

## Recommended defaults

- WebSocket endpoint: `wss://dashscope.aliyuncs.com/api-ws/v1/inference`
- Model: `qwen-audio-3.1-asr-flash-streaming`
- Audio: 16 kHz, mono PCM16 (`pcm`), sent as TEN audio-frame bytes
- VAD: `near_meeting_16k`
- Sentence silence: `240` ms
- `run-task` is sent after connection and audio waits for `task-started`
- `finish-task` is sent by `asr_finalize`; the socket remains reusable for the next turn

## Enhancement configuration

The implementation supports the report's recommended explicit choices:

- Precompiled hotwords: `params.vocabulary_id`
- Per-session hotwords: `params.vocabulary`
- Historical/domain context: `params.context`
- `vocabulary_id` and `vocabulary` are intentionally mutually exclusive in config
- Context is passed under `payload.input.context`

## Validation

- Qwen guarder runs use the shared reconnect-close case; Qwen must not be
  excluded from connection-status reconnection coverage.

### 2026-10-29 reconnection verification

- Removed the Qwen-specific exclusion from `test_connection_status_reconnection`.
- Changed `property_invalid.json` to valid connection/model/audio parameters with
  an explicitly invalid API key. The old fixture failed local config validation
  before opening a socket, so it did not exercise authentication failures.
- Targeted guarder: 1 passed, 13 deselected. The real endpoint rejects the invalid
  credential and the extension reports `connecting -> disconnected` plus a fatal
  error, with no retry, as required for permanent authentication failures.
- Default guarder suite: 12 passed, 1 skipped, 1 deselected in 153.86 seconds.
  The skipped item is the pre-existing, untracked opt-in `test_stream_soak`
  (requires `TEN_ASR_SOAK_SECONDS`); the default runner deselects
  `test_long_duration_stream`. Neither reconnection test is skipped.
- Logs: `/tmp/qwen_reconnect_guard_fixed_20261029.log` and
  `/tmp/qwen_guarder_fixed_20261029.log`.
- Added unit coverage for authentication rejection (401/403), transient failure
  and recovery with a single reconnect owner, bounded retries, stop cancellation,
  and validity of the guarder's invalid-credential fixture.

### 2026-10-30 supplied-credential verification

- Re-ran `task asr-guarder-test EXTENSION=qwen_asr_python CONFIG_DIR=tests/configs`
  with the newly supplied credential injected only through the process environment.
- Result: 12 passed, 1 skipped, 1 deselected in 155.23 seconds; exit code 0.
- Confirmed the effective masked credential fingerprint matches the supplied key,
  and the raw credential is absent from the log.
- Both reconnection guard cases ran. The invalid-credential fixture continues to
  use its deliberately invalid key and validates the fatal authentication path.
- Same-session finalize/next-turn coverage passed with two non-empty final results.
- The remaining skip is the opt-in `test_stream_soak`; the deselected item is
  `test_long_duration_stream`, excluded by the default guarder runner.
- Log: `/tmp/qwen_guarder_newkey_20261030.log` (permissions 0600).

### 2026-10-30 one-hour continuous-audio soak

- Completed 3600 seconds of real-time PCM input with finalize every 15 seconds.
- Received 7389 results and 1817 non-empty finals; all 60 minute windows had new
  finals, and the maximum consecutive-final log gap was 4.361930 seconds.
- All 241 finalize requests were acknowledged; no vendor errors or unexpected
  disconnections occurred. Production source fingerprints remained unchanged.
- The full soak **failed** its cumulative `actual_send` metric wait: Qwen resets
  the base audio timeline on finalize, so this metric never reached 3,600,000 ms.
  The test requirement was not weakened to hide the failure.
- Detailed setup, observations, diagnosis, and artifacts: `soak_20261030.md`.

### 2026-10-31 cumulative timeline fix and successful one-hour rerun

- Fixed Qwen's accounting without changing `ten_ai_base` or other ASR vendors.
  Finalize/reconnect no longer reset the base timeline or metric baseline. Each
  successful new task records its cumulative provider-audio origin for result
  and word timestamp mapping; no extra audio buffering is introduced.
- Eight new regression cases fail before the fix and pass afterward; all
  standalone unit tests now pass (26 total).
- Real short soak passes with exact 20,000 ms cumulative/delta accounting.
- Default guarder: 12 passed, 1 skipped, 1 deselected; reconnection is not skipped.
- Re-ran the original one-hour soak: **1 passed**. All 3600 seconds of input are
  accounted for by both `actual_send` and the sum of deltas (3,600,000 ms).
  Received 7385 results / 1816 non-empty finals, with new finals in all 60 minute
  windows, 241/241 finalize acknowledgements, no errors, no unexpected disconnects,
  and no metric or final-result timestamp regressions.
- Detailed changes, tests, and artifacts: `timeline_fix_20261031.md`.

The API key is injected through `${env:DASHSCOPE_API_KEY}` and is never stored
in this task directory or extension source.
