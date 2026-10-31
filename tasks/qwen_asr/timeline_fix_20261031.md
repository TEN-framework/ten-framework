# Qwen ASR cumulative timeline fix (2026-10-31)

## Changes

Only Qwen production code changes. The ASR base class, other vendors, WebSocket
protocol, and base-managed audio buffering are unchanged.

- Remove finalize's reset of the base `AudioTimeline` and metric baseline.
- Keep cumulative accounting across connection replacement as well.
- Replace the old reset helper with `_start_task_timeline()`. After a successful
  task handshake, record the cumulative provider-audio origin:
  `total_user_audio_duration + total_silence_audio_duration`.
- Map task-relative vendor timestamps by adding this origin before calling
  `audio_timeline.get_audio_duration_before_time()`.
- Map both ends of sentence and word intervals, excluding generated silence
  from real-user-audio duration and retaining dropped-audio offsets.
- Finalize failure, cancellation, or a disconnected finalize does not alter the
  timeline. A failed task start does not commit a new timestamp origin.

No extension-owned audio queue or replay cache is introduced.

## Regression coverage

The eight new cases in `tests/test_timeline.py` all failed against the previous
implementation, then passed after the fix. They cover:

- Multiple finalize/new-task cycles with cumulative metrics and timestamp offsets.
- Audio sent after the last periodic report and before finalize, without loss.
- Finalize timeout, vendor exception, and cancellation.
- Finalize while disconnected.
- Connection replacement with unreported audio retained in the next metric delta.
- Failed task start followed by a successful retry.
- Sentence/word mapping through cumulative silence and dropped audio.

Standalone unit tests: **26 passed in 6.37 seconds**.

Logs: `/tmp/qwen_timeline_red_20261031.log` and
`/tmp/qwen_timeline_ut_20261031.log`.

## Real-endpoint validation

All real-endpoint runs use the latest user-supplied key through a temporary
`DASHSCOPE_API_KEY` environment variable; it is not stored in repository files.

- Short soak: 20 seconds of audio, finalize every 5 seconds; **1 passed**.
  All five finalize acknowledgements received, `actual_send=20,000 ms`, and
  `sum(actual_send_delta)=20,000 ms`, with no metric decreases or vendor errors.
- Default guarder: **12 passed, 1 skipped, 1 deselected in 153.52 seconds**.
  Both reconnection guard cases execute. The skip is the opt-in soak, and the
  deselection is the default long-duration test filter, as before.
- Explicit one-hour soak: **1 passed, 13 deselected in 3607.24 seconds**.
  Uses the same 3600-second real-time input, 10 ms frames, and 15-second finalize
  cadence as the previously failing run. No acceptance assertion was relaxed.

### One-hour results

| Measurement | Result |
| --- | --- |
| Harness completed / failure | `true` / `null` |
| Input audio | 3600 seconds / 115,200,000 bytes |
| Final cumulative `actual_send` | 3,600,000 ms |
| Sum of every `actual_send_delta` | 3,600,000 ms |
| Metric decreases / negative deltas | 0 / 0 |
| ASR results / non-empty finals | 7385 / 1816 |
| Minute windows with new finals | 60 / 60 |
| Maximum result / final log gap | 2.031138 / 6.190160 seconds |
| Finalize requests / acknowledgements | 241 / 241 |
| Vendor errors / unexpected disconnects | 0 / 0 |
| Final-result timestamp backwards steps | 0 |
| RSS across minute samples | 136,100–138,424 KiB |
| Task exit code | 0 |
| Launcher started / finished (UTC) | 12:51:50 / 13:52:05 on 2026-10-31 |

This closes the cumulative-metric failure recorded in `soak_20261030.md`.

## Artifacts

- Short soak: `/tmp/qwen-soak-fixed-preflight-20261031.log` and `.json`.
- Default guarder: `/tmp/qwen_guarder_timeline_fixed_20261031.log`.
- One-hour run: `/tmp/qwen-asr-soak-fixed-20261031-1252/`:
  - `guarder.log`: full log; no plaintext WebSocket key found.
  - `summary.json`: original successful harness report.
  - `analysis.json`: metric sums, timestamp continuity, result gaps, and memory.
  - `source.sha256`: all source fingerprints verified unchanged during the run.
  - `started_at.txt`, `finished_at.txt`, `exit_code.txt`: launcher outcome.

Black, Ruff, and `git diff --check` pass for this change.
