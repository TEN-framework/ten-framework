# Sequential request TTFB verification

As of: 2026-10-27 (Asia/Shanghai, UTC+08:00)
Codebase commit: `95debb242dd9dcab6728cf0fed122b33c750acba`

## Scope and assertions

Run only `test_metrics.py::test_sequential_requests_ttfb`, with seven configurations:
ElevenLabs TTS/TTD, ByteDance standard/duplex, MiniMax standard/duplex, and Soul.
The first request streams text and then ends; after completion, a second request
contains one final text. Each must emit audio, finish, and report one TTFB.
Reported TTFB must be nonnegative and no larger than tester send-to-first-audio
elapsed time. All seven configurations now install independent provider hooks and require
agreement within 1 ms. Unknown extensions remain explicitly unsupported; they
only receive the tester bound.

These are live provider tests. They require valid credentials and may consume
provider quota. They are different from the deterministic delayed-connection
mock tests. A live pass alone cannot prove exclusion of connection delays.
HTTP and other provider timing improvements are deferred to a later batch.

## Preparation

Use a prepared Linux TEN application with `ten_packages/system/{ten_runtime,
ten_runtime_python,ten_ai_base}` and the six Python extension packages at the
revisions intended for verification. Prepare the Python interpreter with the
extension requirements, pytest, pytest-asyncio, python-dotenv and the guarder
requirements before running. The runner does not download or publish packages.
If an extension imports another local Python package, add that package's parent
directory to `PYTHONPATH`.

Create seven configuration folders under a private configuration root, named:

- `elevenlabs-tts` (model `eleven_flash_v2_5`)
- `elevenlabs-ttd` (a dialogue model, for example `eleven_v4`)
- `bytedance_tts`
- `bytedance_tts_duplex`
- `minimax_tts_websocket`
- `minimax_tts_websocket_duplex`
- `soul_tts_python`

Each folder contains `property_basic_audio_setting1.json` using that extension's
configuration schema. Use the deployed extension's own valid property/config
as the starting point. Set model and voice explicitly; do not copy standard
MiniMax configuration into duplex without checking its schema. Credentials
can be `${env:VARIABLE}` placeholders. Keep configuration and output outside
the repository, and never commit credentials or raw provider logs.

## Run and capture evidence

Run inside the container, substituting the prepared paths:

```bash
uv run --no-project --python /path/to/prepared/python python \
  /path/to/tts_guarder/scripts/run_ttfb.py \
  --app-dir /path/to/prepared-app \
  --extension-dir /path/to/extensions \
  --config-root /path/to/private-configs \
  --python /path/to/prepared/python \
  --env-file /path/to/private.env \
  --output /tmp/ttfb-review-$(date +%Y%m%d-%H%M%S)
```

Repeat `--env-file` to load multiple dotenv files; later nonempty values
override earlier ones. Empty template entries do not erase existing values. Use `--case elevenlabs-tts` (or another case folder name listed above) to run
one configuration and capture its own screenshot. Omit it to run all seven.
Use `--collect-only` first to validate the seven test invocations without
calling providers. Collection is labeled COLLECTED, never PASS.

The runner creates an isolated temporary app using the specified packages and
current guarder tests. It runs each case in a separate process, continues after
failures and returns a nonzero exit status if any case fails. An existing output
folder is rejected to preserve prior results. Capture the terminal's TTFB SUMMARY
and source/configuration paths for review screenshots. Keep the accompanying
`summary.txt`, `results.json`, per-case pytest logs and JUnit XML for diagnosis.
Logs may include credentials from provider configuration; inspect and redact
before sharing. Do not screenshot raw debug logs.

## TTFB hook contract

Protocol observers are under `tests/ttfb_hooks/`: ElevenLabs (TTS/TTD),
ByteDance, ByteDance duplex, MiniMax (standard/duplex), and Soul. The installer
in `ttfb_reference.py` chooses the implementation and endpoint, resolves URL
environment placeholders, and restores the client dependency after the test.
It reuses `timing_hooks/transport.py`, which remains compatible with the subtitle
observers. TTFB does not read production timing fields or call production
protocol decoders.

Each matched connection gets its own observer. Its send/receive callbacks only
queue raw frames, monotonic timestamps and the tester request ID at capture.
When the tester reads the reference, queued events are decoded in capture order.
This keeps JSON, gzip and audio decoding off the production send/receive path.
The observer excludes whitespace-only input, control messages and empty audio,
and retains the first timestamp across subsequent chunks. Delayed parsing uses
the original capture time, never the later parsing time. ElevenLabs context IDs and ByteDance duplex session IDs associate audio
with the request that sent text. Standard ByteDance, MiniMax and Soul serialize
requests on a connection; their audio references follow the active text request
on that connection because the wire protocol does not provide an application
request ID for every audio frame. Frames from an old connection cannot complete
a new request's interval. This does not prove arbitrary interleaved requests
without wire identity are distinguishable.

When adding a vendor, implement `text_context(raw)` and `audio_context(raw)` in
its observer, register the module/class/endpoint in `HOOKS`, and add wire-fixture
and local WebSocket tests. The existing tests cover all seven modes, two request
IDs, empty payloads, repeated chunks, context mismatch, real transport wrapping
and restoration. Soul also covers JSON and gzip gateway envelopes. Run the local
mock targets in a prepared guarder app (no provider requests):

```bash
uv run --no-project --python /path/to/prepared/python python -m pytest \
  tests/test_ttfb_hooks.py tests/test_soul_ttfb_reference.py tests/test_timing_hooks.py \
  --extension_name soul_tts_python --config_dir /path/to/private-configs/soul_tts_python \
  -q --tb=short
```

After this hook change, repeat the seven live runs using the runner above; old
screenshots obtained without the ElevenLabs reference hook are not evidence for
the strengthened provider-boundary assertions.

## Live verification on 2026-10-27

All seven configurations passed `test_sequential_requests_ttfb` with independent
provider reference hooks enabled, retaining the tester upper bound and the
2 ms reference tolerance. The run used the same prepared container runtime and
extension sources as the review setup. No assertion was relaxed.

The first attempt exposed two setup defects: empty entries in a second dotenv
file erased valid credentials, and ByteDance observers lacked the extensions'
default endpoints. The runner now ignores empty dotenv entries and the registry
supplies both ByteDance default URLs. All seven passed after these corrections.
This is a focused live regression run, not a guarantee of future provider
availability or a full guarder suite result.

## 1 ms verification

As of: 2026-10-27 (Asia/Shanghai, UTC+08:00)

With synchronous parsing, ElevenLabs TTS exceeded 1 ms partly because the hook
itself decoded audio before returning it to production code. Soul also produced
a 1.001394 ms mismatch. Protocol recognition is now deferred for all seven modes,
including outgoing messages; captured request IDs preserve association while
processing events later. The threshold is 1 ms, without changing the tester
upper bound or production adapters.

All seven live sequential-request TTFB cases passed with this implementation.
All 43 local protocol/transport/subtitle hook tests passed, including deferred
parsing, original arrival times, context filtering and reading an earlier
request while events from the next request are pending. These results supersede
the earlier 2 ms run for the current implementation; one passing live run does
not guarantee arbitrary scheduling delays will stay within 1 ms.

## Single-extension setup compatibility

As of: 2026-10-28 (Asia/Shanghai, UTC+08:00)
Codebase commit: `974517a11608f26b5a29aa29175833c31683f230`

Transport utility tests install minimal module/config doubles rather than
importing vendor implementations. They run their coroutines with asyncio.run,
so pytest-asyncio is not required; if websockets itself is absent, only those
local WebSocket fixtures skip. Protocol-only fixtures need no installed vendor
SDK. Endpoint contract tests run only for the selected extension_name, and
skip other vendors without importing them.

The live observer resolves environment placeholders, instantiates the selected
installed extension's configuration, and calls its normal default/parameter
normalization (without credential validation). It reads the resulting uri,
api_url or url. ElevenLabs also applies its initialization defaults and uses
its normalized base_url when no direct URL is supplied. This shares endpoint
configuration rules only: production timing state and response decoding are
still independent. Tests cover uri/params.uri, top-level versus nested endpoint
precedence, environment substitution and defaults.

Container verification prepared six apps, each exposing only one vendor
extension, and ran the protocol, transport and selected endpoint tests with
`-p no:asyncio --strict-markers`. All six passed. This verifies the new offline
targets under the standard single-extension installation shape; it is not a
claim that the full live guarder suite was run for every vendor.

After these changes, all seven live sequential-request TTFB configurations also
passed with the independent reference enabled and the existing 1 ms tolerance.
The endpoint checks and single-extension installation checks are separate from
that live result: the live verification app supplies all selected packages.
