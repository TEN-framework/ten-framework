# Guarder timing hooks

As of: 2026-10-27 (Asia/Shanghai, UTC+08:00)
Codebase commit: `11562f89409198d942a1f4eb05bec6c7bd01cb53`

## Required TTFB hook

Every new or completed TTS implementation must have an independent guarder TTFB
hook and an installer/registry entry that selects it for the actual provider,
transport and model family. An existing hook may be reused only after verifying
its protocol matches. A named file alone, or an `unsupported` result, does not
satisfy this requirement. HTTP and opaque SDK implementations are not exempt:
observe their public transport/send/first-audio boundary and document the
measurement limit when the SDK hides its socket. Do not claim socket timing
when only an SDK boundary is observable.

The current guarder layout is `ai_agents/agents/integration_tests/tts_guarder/`:

- `tests/ttfb_hooks/`: provider wire observers;
- `tests/ttfb_reference.py`: hook selection, reference timing and request mapping;
- `tests/timing_hooks/transport.py`: shared connection observation;
- `tests/test_metrics.py::test_sequential_requests_ttfb`: reference comparison;
- `tests/TTFB_TESTING.md`: maintained run instructions and hook contract.

Read the checked-out interfaces before implementing. If this checkout predates
the hook infrastructure, report that prerequisite and arrange the supporting
change; do not silently skip the mandatory hook or introduce a competing
framework. These hook paths are introduced in PR #2370.

Observe independently of production timing fields and protocol decoders:

- Start at the first nonempty text send after connection/session preparation.
- End at arrival of the first message containing valid, nonempty audio. Control
  messages, acknowledgements, headers and empty audio must not end the interval.
- Capture monotonic timestamps and request identity immediately at the public
  send/receive boundary. Queue raw messages and parse them when the tester reads
  the reference, so JSON, gzip, Base64 or other decoding does not delay the
  production send/receive path. Use capture time, never later parsing time.
- Keep observers independent per connection. Map provider context/session IDs
  to the captured TEN request ID. If the protocol lacks wire request identity,
  state and test its serialization constraint; do not claim arbitrary interleave
  coverage. Old connection data must not complete a new request's interval.
- Preserve payloads, return values and cancellation behavior, and restore all
  patched bindings when the test exits, including failure paths.

Require both existing guarder checks: reported TTFB must be nonnegative and no
larger than tester first-text-send to first-audio-receive elapsed time, and must
match the independent hook reference within the guarder's current tolerance
(currently 1 ms). Do not widen tolerance or disable the observer to obtain a pass.
Missing reference timing for a supported implementation is a test failure.

Add deterministic protocol and real local-transport tests for hook installation
and restoration, control/empty messages, multiple chunks, two request IDs with a
single-final-text second request, delayed connection preparation, and stale
connection/context data. Verify deferred decoding retains original arrival
times and does not mix requests. Then run the real-provider TTFB guarder with
reference observation enabled; record the command, source revision and result.
Mocks cannot replace the real-provider acceptance gate in common.md.

## Conditional subtitle alignment hook

First determine whether the adapter exposes trustworthy provider word/segment
timestamps through `tts_text_result`. Supporting audio alone does not imply
subtitle support. If the implementation supplies subtitles, register a matching
subtitle timing hook under `tests/timing_hooks/` and its registry, using
`tests/SUBTITLE_TIMING.md` and the existing interfaces. Compare observed TEN
subtitles against independently captured provider alignment and emitted PCM
clock, including sequential requests and flush/reconnect boundaries.

If the adapter does not support subtitle timestamps, mark this check not
applicable and explain the capability limitation. Do not fabricate provider
alignment or require a subtitle hook merely to satisfy a checklist. A provider
that lacks usable timing does not remove the mandatory TTFB hook requirement.

TTFB and subtitle checks may reuse transport observation but must keep their
reference data and assertions separate. Neither may read the production state
or calculated values that it is meant to validate.
