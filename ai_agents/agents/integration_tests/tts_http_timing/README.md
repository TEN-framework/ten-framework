# HTTP TTFB adapter mocks

As of: 2026-11-03 (Asia/Shanghai, UTC+08:00)
Codebase commit: `10c89b121`

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

Repeat with `mistral_tts_python`, `smallest_tts_python`,
`sarvam_http_tts`, `generic_tts_http_python` and `gemini_tts_python`.
The last two require their packages to be supplied by the prepared app. The extension must exist in the prepared app; collection
from an unprepared checkout is not a valid installation test.

Other second-stage adapters have focused extension-local `test_ttfb_boundary.py`
cases: Google, Oracle, Murf, Cosy, StepFun, Hume, Polly, Typecast and OpenAI.
Hume and Typecast additionally have `test_ttfb_transport.py`. Those SDK transport
fixtures use local providers and public SDK boundaries. SDK-internal timing
that is not exposed by a supported hook remains out of scope.

All five HTTP mock invocations and the listed adapter/SDK tests passed in the
prepared development container. The shared base also passed raw audio and
partial-JSON-envelope native TEN tests. All touched package versions were
bumped for subsequent publication. These PRs depend on publication of the
shared base 0.7.63; no package is published by running these tests.

All thirteen provider hooks and the reusable live guarder case are documented
in [PROVIDER_TTFB.md](../tts_guarder/tests/PROVIDER_TTFB.md).
