# ASR / STT extension

Merge baseline: `docs/ai/L1/L2/asr_plugin_design_review.md` (every **MUST**
there blocks review). This file is the build recipe.

## Templates

| Vendor transport | Copy from | Why |
| ---------------- | --------- | --- |
| WebSocket, own protocol | `xai_asr_python` | Recent, small, full reconnect + finalize + tests |
| WebSocket, rich results/words | `deepgram_asr_python` | `recognition.py` + `tests/mock.py` patching pattern |
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
├── reconnect_manager.py   # copy from template; only change the docstring/limits
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

## extension.py lifecycle (fixed flow)

1. `on_init`: `await super().on_init(ten_env)`; create `ReconnectManager`;
   parse → `apply_defaults` → `validate_config` → log `config:`; start the
   `Dumper` if `dump`. On failure set `self._init_failed = True`, send **one**
   `FATAL_ERROR`, and keep a default config object.
2. `start_connection`: return immediately if `_init_failed`; record
   `connection_start_timestamp`; build `recognition`; `await start()`;
   on failure classify 401/403 fatal, else non-fatal + `_handle_reconnect()`.
3. `on_open` callback: `send_connect_delay_metrics`, mark reconnect success,
   carry `audio_timeline` totals across connections, flush buffered frames.
4. `send_audio(frame, session_id)`: return `False` when not connected; copy
   bytes with `lock_buf()` and unlock in all paths; push to dumper; send.
5. Result callback → `_emit_asr_result(...)`: deep-copy `self.metadata`, move
   `session_id` to `metadata.session_id`, put vendor fields under
   `metadata.asr_info` with `vendor` and `locked`; compute `start_ms` with
   `audio_timeline.get_audio_duration_before_time(...)` plus the pre-reset
   offset; skip empty text; `language=config.normalized_language`.
6. `finalize(session_id)`: send the vendor finalize primitive, wait with a
   bounded timeout, emit the final result if any, and always end with exactly
   one `send_asr_finalize_end()`.
7. `on_close` callback: ignore when stopping; planned single-utterance close →
   reconnect immediately; otherwise `_handle_reconnect()` (single owner).
8. `stop_connection`: idempotent; set stop latch; cancel reconnect tasks;
   close recognition. `on_deinit`: stop the dumper.

Also implement: `vendor()`, `is_connected()`, `input_audio_sample_rate()`,
`buffer_strategy()` (`ASRBufferConfigModeKeep(byte_limit=...)` when audio must
survive reconnects), and `vendor_metadata()` returning redacted model/region/url
or `{}` before config exists.

## Required standalone tests

Use `asyncio.run(...)` inside plain test functions and `AsyncMock` for
`send_asr_result` / `send_asr_error`, following `xai_asr_python/tests`:

| File | Covers |
| ---- | ------ |
| `test_asr_result.py` | id, text, final, start_ms, duration_ms, normalized language, `metadata.session_id`, `asr_info.vendor`, locked mapping |
| `test_finalize.py` | success, no result, disconnected, timeout — one `finalize_end` each |
| `test_reconnect.py` | transient retry, no retry after fatal or stop, counter reset |
| `test_vendor_error.py` | 401 → -1000, transient → 1000, `vendor_info.code` string |
| `test_error_check.py` / `test_invalid_params.py` | one fatal error, no later connect |
| `test_dump.py` | dump file bytes equal sent bytes |
| `test_metrics.py` | connect delay emitted |
| `test_vendor_metadata.py` | secrets redacted |

Then run `task asr-guarder-test EXTENSION=<name> CONFIG_DIR=tests/configs`.
Audio arrives as 16 kHz PCM16 mono, 320-byte frames; resample in the client if
the vendor needs another rate and return the true rate from
`input_audio_sample_rate()`.
