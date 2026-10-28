# Second-stage provider TTFB hooks

As of: 2026-10-28 (Asia/Shanghai, UTC+08:00)
Codebase commit: `56f790d47`

## Scope

Every changed second-stage provider has a module in `provider_ttfb_hooks/` and
an entry in its `HOOKS` registry. Both stages use the same case,
`test_metrics.py::test_sequential_requests_ttfb`, and the same
`task tts-guarder-test` entry point. The reference installer selects only
the requested extension's hook; other vendor extensions need not be installed.
The tester sends two sequential requests (the second has one final text),
checks actual audio/completion/metrics, rejects duplicate TTFB, and requires
`0 <= reported TTFB <= tester send-to-first-audio elapsed time`. It also requires
an independent hook reference and an absolute difference no greater than 1 ms.
Providers without a hook retain the first-stage tester bound and are marked
`ttfb_provider_reference=unsupported`. Registered hooks must produce a reference.

| Provider module | Observed start | Observed end |
|---|---|---|
| generic_http | HTTPX body send trace | first raw PCM response chunk |
| gemini | HTTPX body send trace | valid audio MIME inlineData in JSON/SSE |
| azure | compile-time mock SDK first synthesis call | compile-time mock SDK first audio; live native reference unavailable |
| openai | HTTPX body send trace, after connection admission | first raw audio response chunk |
| mistral | HTTPX body send trace | SSE audio_data payload |
| smallest | HTTPX body send trace | SSE audio payload |
| ezai | HTTPX body send trace | JSON audio payload |
| sarvam | HTTPX body send trace | JSON audios payload |
| hume | HTTPX client injected through the SDK's supported parameter | JSON/NDJSON audio payload |
| typecast | aiohttp request chunk sent trace | first nonempty public SDK stream chunk |
| google | SDK consumes the first nonempty text input | first nonempty audio_content response |
| polly | public synthesize_speech call | first nonempty AudioStream chunk |
| oracle | public synthesize_speech call | first body chunk with audio beyond a WAV header |
| cosy | public streaming_call | first nonempty SDK on_data callback |
| murf | nonempty text wire frame for matching context_id | matching context's nonempty base64 audio |
| stepfun | tts.text.delta frame | tts.response.audio.delta containing audio beyond WAV header |

SDK hooks do not claim visibility of opaque SDK-internal DNS/TLS/connection work.
They inspect public vendor dependencies, not production adapter timing fields or
record_text_sent/record_audio_received methods. HTTPX and WebSocket observers
queue raw data and monotonic arrival times; protocol/audio decoding occurs when
the tester reads the reference, outside the capture lock. Capture associates
events with the tester request ID at that moment, not at deferred decode time.
Murf also filters provider context IDs. SDK and protocols without application
request identity assume sequential requests; overlapping or late unidentifiable
callbacks are not proven distinguishable. Installations are scoped and restored.

## Run

Run inside the development container from `ai_agents/`, after loading the
provider credentials. Use the same command as the first stage:

```bash
task tts-guarder-test EXTENSION=openai_tts2_python -- \
  -k test_sequential_requests_ttfb -m api --strict-markers -v --tb=short
```

Change `EXTENSION` to the provider under test. The Task installs the selected
extension and uses its `tests/configs/property_basic_audio_setting1.json`.
Override `CONFIG_DIR` with another directory relative to that extension when
needed. The HTTP changes require `ten_ai_base` 0.7.63 or later; until it is
published, copy the updated base into the guarder app locally after installation.
Do not commit installed package copies.

For a prepared Linux TEN app, the equivalent direct command is:

```bash
uv run --no-project --python /path/to/prepared/python python -m pytest \
  /path/to/agents/integration_tests/tts_guarder/tests/test_metrics.py::test_sequential_requests_ttfb \
  --extension_name openai_tts2_python \
  --config_dir /path/to/private/openai-configs \
  -m api --strict-markers -v --tb=short
```

Set `PYTHONPATH` to the prepared app and its runtime/base library and interface
paths. This entry point no longer requires a sibling test package on PYTHONPATH.

Each config directory contains `property_basic_audio_setting1.json`. Preserve
provider-specific configuration schemas and load private credentials separately.
Do not attach raw debug logs containing credentials to public PRs.

Local hook fixtures require no provider credentials:

```bash
uv run --no-project --python /path/to/prepared/python python -m pytest \
  /path/to/agents/integration_tests/tts_guarder/tests/test_provider_ttfb_hooks.py \
  --extension_name openai_tts2_python --config_dir /tmp -q --tb=short
```

## Verification

16 local hook tests passed in the development container: six HTTP protocol
fixtures, Google/Polly/Oracle/Cosy/Typecast SDK fixtures, two real local WebSocket
transport fixtures, and request/context association/fragmented WAV header cases. Five HTTP adapter
TEN mock runs also passed through these hooks with the 1 ms reference bound;
the mock provider independently audits hook timing with a separate 5 ms
scheduling allowance. Existing adapter tests remain in place. No claim is made
that all thirteen live vendor services have been run with these new hooks.

## Additional HTTP and native adapters

Gemini and Generic HTTP are registered even when their extension packages are
provided by an external prepared app. Generic HTTP requires its raw PCM contract.
Gemini recognizes only `inlineData`/`inline_data` with an audio MIME type and
nonempty decoded data; image/control payloads cannot stop TTFB. Both are covered
by local wire fixtures and two-request TEN adapter mocks with the 1 ms bound.

Azure is native C++. A Python network/SDK observer cannot intercept its real
C++ Speech SDK calls. Without the prepared mock library, hook installation
fails explicitly; it cannot be reported as a passing independent-hook run.
The native mock hook uses the compile-time SDK fixture's independent exported
functions `azure_mock_synthesis_count`, `azure_mock_text_sent_ns` and
`azure_mock_first_audio_ns`. It never reads production timing state. Configure
`ttfb_mock_sdk_library` with the absolute path to the prepared mock extension's
shared library, and use that same library in the TEN app. Each request records
the next synthesis index before submitting text; subsequent text chunks do not
reset its first SDK call. Production libraries do not expose these mock symbols.

The Azure mock build must already be prepared before running the case; the
hook neither recompiles an extension nor adds instrumentation to a production
binary. Use `azure_tts` with its valid mock configuration plus the library path.
The common JUnit property is `ttfb_provider_reference=enabled`; Azure results
must additionally be labeled as native mock results when sharing evidence.

Verification after these additions: 20 local hook cases passed; Gemini and
Generic HTTP two-request adapter mocks passed with independent hooks at 1 ms;
an actual native Azure extension built against the SDK fixture passed a
request containing two text chunks followed by a single-final-text request,
using the new NativeReference with a 1 ms comparison. Azure production SDK
online timing is not independently verified by this result.
