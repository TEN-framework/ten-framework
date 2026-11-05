# ASR / STT extension

Merge baseline: `docs/ai/L1/L2/asr_plugin_design_review.md` (every **MUST**
there blocks review). This file is the build recipe.

## Templates

Pick by transport and session shape, as the design guide's scenario table
does. Copying a template with the wrong shape is the most common reason a new
plugin fails review.

| Vendor transport | Copy from | Why |
| ---------------- | --------- | --- |
| Long-lived WebSocket (one socket across turns) | `fish_asr_python`, `smallest_asr_python` | Connection generation, independent reconnect task, connection-state reporting, finalize context, buffer drain on recovery |
| Server closes after each finalize | `xai_asr_python` | Planned close then fresh socket for the next turn |
| WebSocket, rich results/words | `deepgram_asr_python` | Result and word mapping |
| Vendor SDK with thread callbacks | `azure_asr_python` | Event-loop transfer, reconnect lifecycle tests |

## Package layout

```text
<vendor>_asr_python/
├── __init__.py
├── addon.py
├── config.py              # <Vendor>ASRConfig, defaults dict, validate_config, to_json
├── const.py               # DUMP_FILE_NAME, MODULE_NAME_ASR = "asr", buffer/retry limits
├── extension.py           # <Vendor>ASRExtension(AsyncASRBaseExtension, <Vendor>ASRRecognitionCallback)
├── recognition.py         # <Vendor>ASRRecognition + <Vendor>ASRRecognitionCallback (ABC)
├── reconnect_manager.py   # optional; see "Reconnect policy" below
├── manifest.json          # imports asr-interface.json
├── property.json
├── pyproject.toml
├── requirements.txt
├── README.md
└── tests/
    ├── __init__.py
    ├── bin/start
    ├── conftest.py
    ├── mock.py            # patches extension.<Vendor>ASRRecognition
    ├── configs/
    │   ├── property_en.json       # valid key, language en
    │   ├── property_zh.json       # valid key, language zh (or 2nd supported language)
    │   ├── property_invalid.json  # api_key "invalid"
    │   └── property_dump.json     # dump true, dump_path ./tests/dump_output/
    └── test_*.py
```

The guarder's `test_multi_language` loads `property_zh.json` and asserts a
`zh-CN` result (extensions listed in its `_EXTENSIONS_USE_SPANISH` use
`property_es.json` instead). If the vendor has no Chinese, do not fake the
file with another language: omit it, state in the README that the Chinese case
is expected to fail, and report it. `test_dump` uses `property_en.json`.

## config.py shape

```python
<VENDOR>_DEFAULT_PARAMS = {"base_url": "...", "sample_rate": 16000,
                           "encoding": "pcm", "language": "en"}

class <Vendor>ASRConfig(BaseModel):
    dump: bool = False
    dump_path: str = "/tmp"
    finalize_timeout_ms: int = 2000
    params: dict[str, Any] = Field(default_factory=dict)

    def apply_defaults(self) -> None: ...      # setdefault from DEFAULT_PARAMS
    def validate_config(self) -> None: ...     # key, url, sample_rate, encoding, language
    def to_json(self, sensitive_handling: bool = False) -> str: ...
    @property
    def normalized_language(self) -> str: ...  # "en" -> "en-US", "zh" -> "zh-CN"
```

## recognition.py shape (vendor client)

- `<Vendor>ASRRecognitionCallback(ABC)` with async `on_open`,
  `on_partial_result` / `on_result`, `on_error(msg, code)`, `on_close`, plus
  `on_done` if the vendor has an utterance-done event.
- `<Vendor>ASRRecognition.__init__(api_key, audio_timeline, ten_env, config,
  callback)`; `_build_url()`; `async start(timeout)`; `send_audio_frame(bytes)`;
  finalize primitive (`send_audio_done()` / `finish()` / vendor control msg);
  `wait_for_done(timeout_ms)` if the vendor confirms finalize; `close()`;
  `is_connected()` based on the real socket state.
- A single reader task parses vendor messages and calls the callback. Map
  `ConnectionClosedOK` to `on_close` only; `ConnectionClosedError` and
  handshake failures to `on_error` with the HTTP status when known.
- Bound the whole handshake (connect, auth, ready) with one deadline, not a
  per-message timeout. Map an HTTP 401/403 on the upgrade
  (`websockets.exceptions.InvalidStatus`) to a fatal auth error that keeps
  the real status.
- The client never reconnects itself, and `close()` never cancels the task
  it runs in. Reconnecting from inside the reader task cancels the recovery
  it starts.

## extension.py lifecycle (fixed flow)

1. `on_init`: `await super().on_init(ten_env)`; parse → `apply_defaults` →
   `validate_config` → log `config:`; start the `Dumper` if `dump`. On
   failure latch it (`_init_failed` or `_session_fatal`, and
   `auto_connect = False`), send **one** `FATAL_ERROR`, and never connect
   with a fallback config.
2. `start_connection` (public; the base reports `connecting` first): return
   if latched; bump a connection generation and bind callbacks to it; build
   the client; `await connect()`. On failure call `on_disconnected(...)`,
   classify 401/403 fatal (no retry), else non-fatal and schedule a reconnect.
3. After the vendor's ready signal (not when `connect()` returns): call
   `on_connected()`, `send_connect_delay_metrics`, carry `audio_timeline`
   totals across connections, then drain buffered frames while
   `is_connected()` is still False so new frames queue behind them. Drain
   each frame with its own metadata, and on a failed send keep the failed
   frame first so order is preserved.
4. `send_audio(frame, session_id)`: return `False` when not connected; copy
   bytes with `lock_buf()` and unlock in all paths; send; only after the send
   succeeds add user audio to the timeline and the dumper.
5. Result callback → `_emit_asr_result(...)`: deep-copy `self.metadata`, move
   `session_id` to `metadata.session_id`, put vendor fields under
   `metadata.asr_info` with `vendor` and `locked`; compute `start_ms` with
   `audio_timeline.get_audio_duration_before_time(...)` plus the pre-reset
   offset; skip empty text; `language=config.normalized_language`.
6. Finalize: override `on_data` to snapshot each `asr_finalize` request's
   `finalize_id` and metadata and serialize requests with a lock. Send the
   vendor finalize primitive (or trailing silence via `add_silence_audio`),
   wait with a bounded timeout, and end with exactly one
   `asr_finalize_end` echoing that request's id and metadata (the base
   `send_asr_finalize_end()` uses mutable latest-frame values). Decide
   whether speech is pending from a watermark on the vendor's stream clock,
   not from "audio was sent" or "a partial arrived": a late final for an
   earlier utterance must not acknowledge newer speech. Do not hold the
   connection lock while waiting, so a handshake cannot delay the end.
7. Close callback: ignore stale generations and stops; report
   `on_disconnected(...)` (no `error` event for `ConnectionClosedOK`); then
   schedule the single reconnect task. Never await the reconnect inline.
8. `stop_connection`: idempotent; set the stop latch; abort a handshake in
   progress; cancel and await the reconnect task; close the client.
   `on_deinit`: stop the dumper.

### Reconnect policy

The guide's default is unlimited non-fatal retries with capped exponential
backoff, reset after a successful handshake, stopped only by stop/deinit or a
fatal error. `xai_asr_python`'s `ReconnectManager` always stops at
`max_attempts`, so do not copy it unchanged: adapt it to retry without a
ceiling by default, or use a reconnect loop like `fish_asr_python`. A
bounded limit is allowed only as an explicit, documented option that
escalates to `FATAL_ERROR` when reached and has its own test.

Also implement: `vendor()`, `is_connected()`, `input_audio_sample_rate()`,
`buffer_strategy()` (`ASRBufferConfigModeKeep(byte_limit=...)` when audio must
survive reconnects), and `vendor_metadata()` returning redacted model/region/url
or `{}` before config exists.

## Required standalone tests

Use `asyncio.run(...)` inside plain test functions and `AsyncMock` for
`send_asr_result` / `send_asr_error`, and assert on the emitted Data JSON
where possible. The design guide's section 13.1 is the required list:

| File | Covers |
| ---- | ------ |
| `test_asr_result.py` | id, text, final, start_ms, duration_ms, normalized language, `metadata.session_id`, `asr_info.vendor`, locked mapping, emitted JSON |
| `test_finalize.py` | success, no result, disconnected, timeout, send failure, concurrent requests, late final for an earlier utterance — one `finalize_end` each with its own id and session |
| `test_connection_status.py` | connecting → connected → disconnected, payload, redaction |
| `test_reconnect.py` | transient retry without going fatal, no retry after fatal (including HTTP 401/403), counter reset |
| `test_reconnect_lifecycle.py` | expected close, no reconnect during stop, stale callback ignored, buffer drain on recovery, a real server-side close recovering |
| `test_invalid_params.py` | one fatal error, no later connect; URLs with credentials rejected without echoing them |
| `test_vendor_error.py` | 401 → -1000, transient → 1000, `vendor_info.code` string |
| `test_dump.py` | dump bytes equal sent bytes, no injected silence |
| `test_metrics.py` | connect delay emitted |
| `test_vendor_metadata.py` | model/url reported, secrets redacted |
| `test_audio_timeline.py` | user audio after successful sends only, silence, continuity across connections |

Then run `task asr-guarder-test EXTENSION=<name> CONFIG_DIR=tests/configs`.

`input_audio_sample_rate()` reports the rate of the frames the graph sends
into `send_audio`, not the vendor's rate. The guarder sends 16 kHz PCM16 mono
in 320-byte frames, but graphs may differ, so read the configured input rate.
If the vendor needs another rate, keep the input rate and the vendor rate as
separate settings, resample in the client from one to the other, and compute
user-audio durations from the input rate.
