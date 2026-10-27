# HTTP TTFB adapter mocks

As of: 2026-10-27 (Asia/Shanghai, UTC+08:00)
Codebase commit: `1493ed239`

This directory is independent of the first-stage tts_guarder PR. It checks
OpenAI, Mistral, Smallest, EZAI and Sarvam adapters through TEN text inputs,
audio outputs, completion and TTFB metrics. An injected HTTPX provider delays
connection admission, supplies first audio and delayed response completion,
and timestamps those provider boundaries. Each test sends two request IDs,
with one final-text second request. The outer tester bound is strict; the
independent local mock reference allows 5 ms for scheduling and rounding.

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

Repeat with `mistral_tts_python`, `smallest_tts_python`, `ezai_tw_tts_python`,
and `sarvam_http_tts`. The extension must exist in the prepared app; collection
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
