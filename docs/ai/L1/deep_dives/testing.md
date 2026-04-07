# Testing

> **When to Read This:** Load this document when you need to run tests for an extension,
> understand what the guarder tests validate, or debug test failures.

## Overview

Three levels of testing:
1. **Extension standalone tests** — per-extension unit/integration tests in `tests/`
2. **Guarder integration tests** — framework-level ASR/TTS validation suites
3. **Root-level tasks** — orchestrated via `Taskfile.yml`

## Running Tests

```bash
# All tests
docker exec ten_agent_dev bash -c "cd /app && task test"

# Single extension with dependency install
docker exec ten_agent_dev bash -c \
  "cd /app && task test-extension EXTENSION=agents/ten_packages/extension/deepgram_tts"

# Single extension, skip install (faster iteration)
docker exec ten_agent_dev bash -c \
  "cd /app && task test-extension-no-install EXTENSION=agents/ten_packages/extension/deepgram_tts"

# TTS guarder (16 tests)
docker exec ten_agent_dev bash -c "cd /app && task tts-guarder-test EXTENSION=deepgram_tts"

# ASR guarder (10 tests)
docker exec ten_agent_dev bash -c "cd /app && task asr-guarder-test EXTENSION=azure_asr_python"

# Specific test only (faster iteration on failures)
docker exec ten_agent_dev bash -c "cd /app && task tts-guarder-test EXTENSION=deepgram_tts -- -k test_flush"
```

**Before running tests**, sync your local code into the container. Use tar
to exclude cache artifacts that cause import errors:

```bash
tar --exclude='__pycache__' --exclude='.pytest_cache' \
  -C ai_agents/agents/ten_packages/extension/my_ext -cf - . | \
  sudo docker exec -i ten_agent_dev tar \
  -C /app/agents/ten_packages/extension/my_ext -xf -
```

## Extension Standalone Tests

Each extension can have `tests/` with a `bin/start` entry point:

```
my_extension/tests/
├── bin/start            # Sets PYTHONPATH, runs pytest
├── configs/             # Test config JSON files
│   ├── property.json
│   ├── property_basic_audio_setting1.json
│   ├── property_basic_audio_setting2.json
│   ├── property_dump.json
│   ├── property_miss_required.json
│   └── property_invalid.json
├── conftest.py          # Fixtures
└── test_*.py            # Test files
```

### PYTHONPATH

Tests need this to import TEN runtime:

```bash
export PYTHONPATH=".:ten_packages/system/ten_runtime_python/lib:\
ten_packages/system/ten_runtime_python/interface:\
ten_packages/system/ten_ai_base/interface:\
ten_packages/extension/${EXT_NAME}:$PYTHONPATH"
```

---

## TTS Guarder Tests (15 Tests)

**Location**: `agents/integration_tests/tts_guarder/`

These tests run against any TTS extension. The manifest template (`manifest-tmpl.json`)
substitutes `{{extension_name}}` with your extension name at runtime.

### Test Inventory

| # | Test | What It Validates | Pass Criteria |
|---|------|-------------------|---------------|
| 1 | `test_append_input` | Multiple texts appended with same request_id | audio_start -> frames -> audio_end per group, correct request_id |
| 2 | `test_append_input_stress` | High volume append operations | All appends processed without errors |
| 3 | `test_append_input_without_text_input_end` | Missing text_input_end flag | Processes correctly despite missing flags |
| 4 | `test_append_interrupt` | New requests interrupting in-progress ones | Interrupts handled without crash or malformed audio |
| 5 | `test_basic_audio_setting` | Different sample rates produce different audio | Two configs with different sample_rate yield different output rates |
| 6 | `test_corner_input` | Special chars, emojis, punctuation-only, very short/long | All processed without errors |
| 7 | `test_dump` | Audio dump file creation | Dump file exists, contains valid PCM, size matches duration |
| 8 | `test_dump_each_request_id` | Separate dump files per request_id | Each request_id has own dump file |
| 9 | `test_empty_text_request` | Empty/whitespace text | audio_end within 500ms, no audio data, no crash |
| 10 | `test_flush` | Flush signal handling | Receives flush_end with matching flush_id, no data for 5s after |
| 11 | `test_interleaved_requests` | 8 concurrent requests with different request_ids | Each maintains separate audio stream, correct ordering per request |
| 12 | `test_invalid_required_params` | Invalid API key | Returns FATAL ERROR with message, no crash |
| 13 | `test_invalid_text_handling` | Malformed text, null chars, very long strings | Handled gracefully without crash |
| 14 | `test_metrics` | TTFB metric generation | Metrics data present with valid timestamps |
| 15 | `test_miss_required_params` | Missing API key | Appropriate error returned |

### Critical TTS Invariants

1. **Event ordering must be**: `tts_audio_start` -> `pcm_frame`(s) -> `tts_audio_end` per request
2. **Request isolation**: Interleaved requests must never mix audio streams
3. **Error handling**: Invalid/missing configs produce errors, never crashes
4. **Empty text**: Must complete fast (audio_end within 500ms), generate no audio
5. **Flush**: After flush_end, zero data output for 5 seconds

### Required TTS Config Files

Your `tests/configs/` must provide:

```
property.json                      # Valid API key + default settings
property_basic_audio_setting1.json # sample_rate: 16000 + valid key + dump:true
property_basic_audio_setting2.json # sample_rate: 24000 + valid key + dump:true
property_dump.json                 # dump:true + dump_path + valid key
property_miss_required.json        # Empty/missing API key
property_invalid.json              # Empty/invalid API key
```

**Template** (`property_basic_audio_setting1.json`):
```json
{
  "dump": true,
  "dump_path": "./tests/keep_dump_output/",
  "params": {
    "sample_rate": 16000,
    "key": "${env:MY_VENDOR_API_KEY}"
  }
}
```

### Sample Rate Test Notes

Some extensions don't support multiple sample rates. To skip the sample rate
comparison (test still runs, just doesn't assert rates differ), the test runner
checks `ENABLE_SAMPLE_RATE` env var. Extensions like `openai_tts_python` and
`humeai_tts_python` set this to `False`.

---

## ASR Guarder Tests (10 Tests, 1 Skipped)

**Location**: `agents/integration_tests/asr_guarder/`

### Test Audio Format

- 16-bit PCM, 16kHz sample rate, mono
- Test files: `test_data/16k_en_us.pcm` (English), `test_data/16k_zh_cn.pcm` (Chinese)
- Chunk size: 320 bytes per frame
- Send interval: 10ms between frames

### Test Inventory

| # | Test | What It Validates | Pass Criteria |
|---|------|-------------------|---------------|
| 1 | `test_connection_timing` | Connect + transcribe English audio | Results received, language="en-US" |
| 2 | `test_asr_result` | Result structure and data integrity | Fields: id, text, language, session_id all present |
| 3 | `test_asr_finalize` | Finalize signal → final result + finalize_end | final=True in result, finalize_end received |
| 4 | `test_reconnection` | Recovery after connection failure | Error detected, no crash, can reconnect |
| 5 | `test_vendor_error` | Invalid creds → proper error format | Error has id, module, code, message + vendor info |
| 6 | `test_multi_language` | English + Chinese transcription | en-US and zh-CN both detected correctly |
| 7 | `test_dump` | Audio dump functionality | Dump files created with correct data |
| 8 | `test_metrics` | TTFW and TTLW metrics | TTFW > 0, TTLW > TTFW, both in milliseconds |
| 9 | `test_audio_timestamp` | start_ms and duration_ms accuracy | Timestamps accurate within tolerance |
| 10 | `test_long_duration_stream` | **SKIPPED** — 5+ min stream | No timeout or connection drop |

### Critical ASR Invariants

1. **Result fields**: Every result must have `id`, `text`, `language`, `session_id`
2. **Finalize flow**: `asr_finalize` cmd -> `final=True` result -> `asr_finalize_end` response
3. **Error format**: `{id, module, code, message, vendor_info: {vendor, code, message}}`
4. **Metrics**: TTFW (Time To First Word) > 0, TTLW (Time To Last Word) > TTFW

### Required ASR Config Files

```
property_en.json       # Valid key + language: "en-US"
property_zh.json       # Valid key + language: "zh-CN"
property_invalid.json  # key: "invalid" (triggers vendor error test)
property_dump.json     # Valid key + dump: true
```

**Template** (`property_en.json` for Deepgram):
```json
{
  "params": {
    "key": "${env:DEEPGRAM_API_KEY}",
    "model": "nova-2",
    "sample_rate": 16000,
    "encoding": "linear16",
    "language": "en-US"
  }
}
```

---

## Guarder Test Framework Internals

### Manifest Template System

Both guarders use template manifests with `{{extension_name}}` placeholders:

```json
{
  "type": "app",
  "name": "tts_guarder",
  "version": "0.1.0",
  "dependencies": [
    {"path": "../../ten_packages/extension/{{extension_name}}"}
  ]
}
```

The Taskfile substitutes this at runtime with `sed`.

### conftest.py Pattern

Both guarders use a session-scoped FakeApp:

```python
@pytest.fixture(scope="session", autouse=True)
def global_setup_and_teardown():
    event = threading.Event()
    fake_app_ctx = FakeAppCtx(event)
    fake_app_thread = threading.Thread(target=run_fake_app, args=(fake_app_ctx,))
    fake_app_thread.start()
    event.wait()
    yield
    fake_app_ctx.fake_app.close()
    fake_app_thread.join()
```

Each test creates its own `ExtensionTester` within this shared app context.
Tests share the session-scoped app but get fresh extension instances.

### Pytest Options

- `--extension_name` — extension to test (required)
- `--config_dir` — path to configs directory (required)
- `--enable_sample_rate` — "True"/"False" for sample rate comparison (TTS only)

---

## Common Test Failures and Fixes

### "Timeout waiting for audio"
- **Cause**: External API not responding within timeout
- **Fix**: Check API key is valid, check network, increase timeout if needed
- **Note**: Some flakiness is expected with external APIs — run individually to confirm

### "Received error data" / FATAL ERROR
- **Cause**: Extension detected invalid config and raised error (this is correct behavior for error tests)
- **Fix**: If this happens on non-error tests, check your config files have valid API keys

### "Found N dump files, expected M"
- **Cause**: Some requests timed out and didn't produce dump files
- **Fix**: Usually API timeout flakiness — rerun the test

### "Received additional data after flush_end"
- **Cause**: Extension sent audio data after it should have stopped
- **Fix**: Ensure your cancel_tts/flush handling stops all pending output immediately

### "Test failed: sample rates are the same"
- **Cause**: Your extension ignores the sample_rate config
- **Fix**: Implement sample_rate support, or set ENABLE_SAMPLE_RATE=False if your API doesn't support it

### Import errors
- **Cause**: PYTHONPATH doesn't include ten_runtime_python and ten_ai_base
- **Fix**: Check `tests/bin/start` script sets PYTHONPATH correctly

### "ModuleNotFoundError: No module named 'ten_packages.extension.xxx'"
- **Cause**: Extension not installed in test environment
- **Fix**: Run `tman install --standalone` in extension directory, or use `task test-extension` (does it automatically)

---

## CI/CD Pipeline

### Manual Guarder Tests (GitHub Actions)

ASR and TTS guarder tests can be triggered manually:

- Workflow: `.github/workflows/manual_test_asr_guarder.yml`
- Inputs: `extension` name, `config_dir`, `branch`, `env_vars` (semicolon-separated secret names)
- API keys loaded from GitHub Secrets at runtime

### Extension Publishing

- Workflow: `.github/workflows/manual_publish_extension.yml`
- Steps: `tman install --standalone` -> `tman run build` -> `tman publish`
- Requires `TEN_CLOUD_STORE` secret for publishing

---

## See Also

- [Extension Development](extension_development.md) — Config files and pre-submission checklist
- [Back to Workflows](../05_workflows.md)
