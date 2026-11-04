# HTTP TTFB adapter mocks

As of: 2026-11-04 (Asia/Shanghai, UTC+08:00)
Codebase commit: `8a9b75683`

These TEN mocks check two sequential request IDs through text inputs, audio,
completion and TTFB metrics. HTTP timing starts at client.get() and ends at its
first nonempty audio RESPONSE, including connection delay and conversion.
The guarder adapter reference allows 1 ms and the tester outer bound is strict.
The wire-level fixture interval is a lower bound, not an equal interval, because
it excludes adapter parsing and conversion.

## Run

Use a prepared TEN app with original runtime packages, the second-stage
local ten_ai_base interface (0.7.63), and the adapter under test installed in
`ten_packages/extension`. Install the adapter's Python requirements and pytest,
pytest-asyncio, HTTPX and typing-extensions in a Python 3.10 environment. Copy
this directory's tests to a test package in that prepared app; tests import
production adapters by their installed package name. No live credentials are
used and the HTTPX transport is intercepted before provider I/O.

From that app, with its system interface/lib paths on PYTHONPATH:

```bash
uv run --no-project --python /path/to/python python -m pytest \
  /path/to/tts_http_timing/tests/test_adapters.py \
  --extension_name openai_tts2_python --config_dir /tmp -q --tb=short
```

Repeat with `mistral_tts_python`, `generic_tts_http_python` and
`gemini_tts_python`. The latter two require externally prepared extension
packages. The app must contain the updated base implementation (0.7.64 or
later) and the matching runtime.

Google, Hume and OpenAI also have extension-local timing cases. Cosy uses
main's existing tests, including `tests/test_metrics.py` for vendor TTFB and
`tests/test_connection_reuse.py` for request isolation and connection reuse.
The obsolete pre-pool Cosy fixture and unused timing helper have been removed.

The guarder references and current timing contract are documented in
[PROVIDER_TTFB.md](../tts_guarder/tests/PROVIDER_TTFB.md).
