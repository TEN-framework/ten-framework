# Second-stage provider TTFB hooks

As of: 2026-10-27 (Asia/Shanghai, UTC+08:00)
Codebase commit: `a7eaf18847b0b720a3f1b7d14e9f895b5f2261ac`

## Scope

Every changed second-stage provider has a module in `provider_ttfb_hooks/` and
an entry in its `HOOKS` registry. The separate case is
`test_provider_ttfb.py::test_sequential_requests_ttfb_with_provider_hook`.
This addition does not overwrite the first-stage hooks/case under review.
The tester sends two sequential requests (the second has one final text),
checks actual audio/completion/metrics, rejects duplicate TTFB, and requires
`0 <= reported TTFB <= tester send-to-first-audio elapsed time`. It also requires
an independent hook reference and an absolute difference no greater than 1 ms.
Unknown providers are skipped explicitly by this second-stage case.

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

Use a prepared Linux TEN app containing the current second-stage extensions,
base 0.7.63 and runtime packages, plus all required vendor Python SDKs. Set
PYTHONPATH to that app, its system lib/interface paths, and the parent
`ai_agents/agents` directory so both guarder and shared tester packages import.
The caller must load valid environment variables referenced by the config.

From that app, run one provider (repeat with any registry extension name):

```bash
uv run --no-project --python /path/to/prepared/python python -m pytest \
  /path/to/agents/integration_tests/tts_guarder/tests/test_provider_ttfb.py::test_sequential_requests_ttfb_with_provider_hook \
  --extension_name openai_tts2_python \
  --config_dir /path/to/private/openai-configs \
  -m api --strict-markers -v --tb=short
```

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
C++ Speech SDK calls, so live mode without an independent reference is explicitly
skipped by this case. This is not reported as a passing independent-hook run.
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
The JUnit property is `native-mock-sdk` rather than a live provider reference.

Verification after these additions: 20 local hook cases passed; Gemini and
Generic HTTP two-request adapter mocks passed with independent hooks at 1 ms;
an actual native Azure extension built against the SDK fixture passed a
request containing two text chunks followed by a single-final-text request,
using the new NativeReference with a 1 ms comparison. Azure production SDK
online timing is not independently verified by this result.
