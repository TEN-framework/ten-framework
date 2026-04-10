# generic_video_python Compliance And Hardening Plan

## Scope

This plan covers bringing
`ai_agents/agents/ten_packages/extension/generic_video_python`
up to date with the latest checked-out
`/home/ubuntu/convoai_to_video`
contract and up to the engineering quality bar recently applied to
`deepgram_tts`.

The goals are:

- restore full interoperability with the latest documented ConvoAI start, stop,
  and WebSocket message contracts
- make all extension parameters flow correctly from TEN graph config into HTTP
  requests and WebSocket messages
- remove silent config drift and runtime hazards
- add real tests and integration validation so future protocol drift is caught

This plan is intentionally detailed and does not change code by itself.

## Branch

Implementation branch:

- `fix/generic-video-python`

This work should be executed on a fresh branch created from `main`, not from
`dev/ben-graphs`.

## Current State Summary

The extension is functional in outline, but it is not fully compliant with the
latest `convoai_to_video` contract and it is not at the same maturity level as
recently hardened extensions such as `deepgram_tts`.

The main problems fall into four groups:

1. Contract mismatches
2. Config/schema drift
3. Runtime robustness gaps
4. Missing tests and weak documentation

## Priority Findings

### High Priority

#### 1. Audio sample rate is not propagated correctly end to end

The extension currently sends `self.config.input_audio_sample_rate` in outbound
`voice` messages instead of the actual sample rate from
`AudioFrame.get_sample_rate()`.

Why this matters:

- the latest ConvoAI WebSocket contract expects `sampleRate` to describe the
  actual audio chunk being sent
- if upstream TEN audio arrives at `44100` or `48000` but config says `16000`,
  the extension mislabels the chunk
- providers may resample incorrectly, distort playback, or reject audio

Impact:

- real interoperability risk
- hard to diagnose because the payload shape still looks valid

Required outcome:

- queue audio bytes together with the actual sample rate
- make outbound `voice.sampleRate` reflect the true frame rate, not a static
  config default
- document what `input_audio_sample_rate` means after the fix:
  expected upstream rate, fallback, or optional validation constraint

#### 2. Config naming and manifest drift causes silent misconfiguration

The implementation expects `agora_video_uid`, while the extension metadata and
defaults still expose `agora_avatar_uid`.

The extension also has many runtime config fields that are not declared in the
manifest schema at all.

Why this matters:

- TEN `BaseConfig` loads fields by exact name and silently ignores misses
- mismatched names do not fail fast; they fall back to defaults
- graph-level settings can appear configured while being ignored at runtime

Examples of drift:

- code expects `agora_video_uid`
- manifest/property defaults publish `agora_avatar_uid`
- manifest exposes only a subset of actual config fields

Impact:

- silent defaulting to wrong values, including UID `0`
- poor editor/tooling support because manifest does not reflect runtime config
- high maintenance risk because schema and code are not coupled

Required outcome:

- choose one canonical field name and use it everywhere
- make manifest, defaults, code, README, and example graph snippets consistent
- remove silent drift paths

### Medium Priority

#### 3. Stop-session request shape does not match latest ConvoAI contract

The latest `convoai_to_video` contract expects `session_token` in the DELETE
request body for `/session/stop`. The current extension only sends
`session_id` in the body and moves the token into an auth header.

Why this matters:

- the mock contract in the checked-out repo validates stop payload structure
- even if some providers accept the current extension behavior, it no longer
  matches the checked-in reference contract

Required outcome:

- align the stop request with the current contract used in
  `convoai_to_video`
- make token placement explicit and tested
- support the latest expected request body shape

#### 4. Missing `area` parameter

The latest ConvoAI repo includes optional `area` in both session start and
WebSocket `init`.

Why this matters:

- the provider can use `area` for geographic routing and latency optimization
- the extension is not fully compliant with the latest contract without it

Required outcome:

- add `area` to config
- validate against the allowed enum:
  `GLOBAL`, `NORTH_AMERICA`, `EUROPE`, `ASIA`, `INDIA`, `JAPAN`
- forward it in both start and `init` payloads

#### 5. Secrets are logged in plaintext

The extension logs the request headers for session start, including the raw
API key.

Why this matters:

- violates repo security conventions
- leaks credentials into logs
- increases operational risk in shared environments

Required outcome:

- mask API keys and tokens in logs
- adopt the same sensitive logging pattern used in stronger extensions

#### 6. Background task lifecycle is incomplete

The extension creates the audio sender task but does not store it in
`self._audio_task`, so `on_stop()` cannot cancel it.

Why this matters:

- leaves a background task blocked on the queue
- can cause shutdown leaks and stale work after stop

Required outcome:

- store all long-lived tasks explicitly
- cancel and await them during stop
- ensure shutdown is deterministic

#### 7. Blocking HTTP inside async methods

The extension uses `requests.post()` and `requests.delete()` inside async
methods.

Why this matters:

- blocks the event loop
- can stall unrelated extension work
- makes timeout and cancellation behavior worse

Required outcome:

- replace blocking HTTP calls with an async client
- preserve explicit timeout handling and verbose error parsing

#### 8. No test suite and broken test metadata

The manifest declares a test script, but the extension has no `tests/`
directory at all.

Why this matters:

- no way to verify contract compliance automatically
- packaging metadata is incorrect
- drift against `convoai_to_video` will continue unless tested

Required outcome:

- add a real `tests/` harness
- make `manifest.json` test script point to something that exists
- add enough coverage to catch protocol and config regressions

### Low Priority

#### 9. Config model uses the older dataclass pattern

The extension uses `BaseConfig` plus a dataclass, while newer stronger
extensions use explicit Pydantic models.

Why this matters:

- weaker validation
- weaker schema expressiveness
- easier for silent drift to persist

Required outcome:

- migrate to a dedicated `config.py` with explicit model validation
- add masked string rendering for safe logging

#### 10. `activity_idle_timeout` default is inconsistent

The extension default is `60`, while the latest `convoai_to_video` examples use
`120`.

Why this matters:

- not a hard compatibility break
- but it creates unnecessary surprise relative to the current reference

Required outcome:

- default to `120` unless a TEN-specific reason justifies a different default

#### 11. Session cache file is singleton and not instance-safe

The extension uses a fixed file path in `/tmp` for session caching.

Why this matters:

- concurrent instances can overwrite each other
- one channel/session can interfere with another

Required outcome:

- scope cache entries by extension instance or channel
- or remove file-based singleton caching if it is not essential

#### 12. Extension is not obviously exercised by an active example graph

The extension is present in example manifests but does not appear to be part of
the main active avatar graphs that are regularly used.

Why this matters:

- drift can accumulate without being noticed
- changes are harder to validate without a canonical graph

Required outcome:

- add or maintain a minimal example graph that actually uses this extension

#### 13. Stale comment around WebSocket library version

There is a comment referring to an older `websockets` version while the
requirements are newer.

Why this matters:

- small maintenance issue
- increases confusion during debugging

Required outcome:

- update or remove stale version commentary

## Desired End State

When this work is complete, `generic_video_python` should:

- accept a complete and well-defined config schema
- validate config before any network calls
- generate start, stop, and WebSocket messages that match the latest
  `convoai_to_video` contract
- propagate audio sample rates correctly
- avoid blocking the event loop
- avoid leaking secrets in logs
- shut down cleanly
- include a real automated test suite
- have a usable README and at least one example graph

## Implementation Plan

## Phase 1: Config Normalization And Schema Repair

Goal:

- eliminate config drift before touching runtime behavior

Tasks:

1. Define a canonical config field set.
   Include:
   - Agora fields:
     `agora_appid`, `agora_appcert`, channel field, canonical UID field,
     `enable_string_uid`
   - provider fields:
     `generic_video_api_key`, `avatar_id`, `quality`, `version`,
     `video_encoding`, `activity_idle_timeout`, `area`,
     `start_endpoint`, `stop_endpoint`
   - audio-related fields:
     `input_audio_sample_rate`

2. Pick one canonical UID field name.
   Recommendation:
   - use `agora_avatar_uid` as the canonical field name for this extension
   - reason: it is already what this extension's manifest and property defaults
     expose, and it is the lower-risk choice for preserving compatibility with
     any existing graph config that targets `generic_video_python`
   - update code, manifest, defaults, tests, and docs together

3. Add all runtime config fields to `manifest.json`.
   This is needed for:
   - schema clarity
   - better tooling
   - easier config review
   - consistency between runtime and metadata

4. Update `property.json` defaults.
   Include sane defaults for:
   - `quality`
   - `version`
   - `video_encoding`
   - `activity_idle_timeout`
   - `area`
   - endpoint placeholders

5. Add explicit enum validation.
   At minimum:
   - `quality`: `low`, `medium`, `high`
   - `video_encoding`: `H264`, `VP8`, `AV1`
   - `area`: `GLOBAL`, `NORTH_AMERICA`, `EUROPE`, `ASIA`, `INDIA`, `JAPAN`

6. Migrate config implementation to a dedicated `config.py`.
   Recommendation:
   - use a stronger validated model pattern instead of the current minimal
     dataclass loader
   - implement safe string rendering for logs

Deliverables:

- aligned `config.py`
- aligned `manifest.json`
- aligned `property.json`
- updated runtime references to canonical field names

## Phase 2: Protocol Compliance Fixes

Goal:

- make outbound HTTP and WebSocket messages match the latest ConvoAI contract

Tasks:

1. Fix audio sample-rate propagation.
   - queue tuples of `(audio_bytes, actual_sample_rate)`
   - use the real sample rate in outbound `voice.sampleRate`
   - do not resample in this extension
   - keep `input_audio_sample_rate` only as documentation and optional
     validation of expected upstream behavior
   - if actual frame rate differs from configured expectation, log a warning
     rather than rewriting the outgoing sample rate

2. Add `area` to session start payload.
   Current contract includes it as an optional field with a default.

3. Add `area` to WebSocket `init` payload.

4. Align stop-session request payload.
   - include `session_id`
   - include `session_token` in the DELETE request body
   - this is required by the latest checked-in contract and mock validation in
     `convoai_to_video`
   - keep any additional auth header behavior explicit and tested, but do not
     rely on headers alone

5. Reconfirm all outbound messages match the documented protocol:
   - `init`
   - `voice`
   - `voice_end`
   - `voice_interrupt`
   - `heartbeat`

6. Review whether any optional protocol support should be added now.
   For example:
   - `special` command support is not required immediately, but should be
     explicitly treated as out of scope unless there is a real integration need

Deliverables:

- compliant start payload
- compliant stop payload
- compliant init payload
- correct audio message formatting

## Phase 3: Runtime Robustness And Safety

Goal:

- make the extension safe to run under load, restart, and failure conditions

Tasks:

1. Replace blocking HTTP with an async client.
   Requirements:
   - request timeout support
   - response error parsing
   - clear exception types
   - cancellation friendliness
   Recommendation:
   - use `httpx` for the async HTTP client because it is a straightforward
     replacement for `requests`-style call patterns and keeps the migration
     focused

2. Fix task ownership.
   - store the audio sender task in `self._audio_task`
   - review all other long-lived tasks
   - ensure all are cancelled in `on_stop()`

3. Harden shutdown behavior.
   - clear queue if needed
   - disconnect WebSocket and stop session deterministically
   - ensure no background tasks survive stop

4. Remove plaintext secret logging.
   - mask `x-api-key`
   - mask bearer tokens
   - ensure error logging does not reintroduce secrets indirectly

5. Review session cache design.
   Options:
   - scope file path by channel/UID
   - scope by instance name
   - or remove global cache behavior if reconnect/session reuse does not justify
     the complexity

6. Clean stale comments and version assumptions.
   - update `websockets` note
   - ensure comments match current requirements and behavior

Deliverables:

- async network layer
- clean shutdown
- masked logging
- safer session caching

## Phase 4: Test Suite Buildout

Goal:

- reach a testability bar comparable to recently hardened extensions

Principle:

- do not force this extension into TTS guarders
- build protocol-appropriate tests for an avatar/video transport extension

Tasks:

1. Create the test harness.
   Add:
   - `tests/bin/start`
   - `tests/conftest.py`
   - any shared fixtures or mock utilities

2. Add config tests.
   Cover:
   - required fields
   - enum validation
   - canonical UID field loading
   - default values
   - secret masking in string output

3. Add payload construction tests.
   Cover:
   - session start payload
   - session stop payload
   - WebSocket `init` payload
   - `voice` payload with actual sample rate
   - `voice_end`, `voice_interrupt`, `heartbeat`

4. Add protocol mock tests modeled on the checked-out ConvoAI reference.
   Use the checked-out `convoai_to_video` repo as the contract source, but do
   not depend directly on its standalone scripts as imported test
   infrastructure.

   Build local test fixtures and mock servers in this extension's own test
   suite that validate the same request and message shapes enforced by:
   - `connection-setup/session_test_receiver.py`
   - `websocket-receive-audio/websocket_test_receiver.py`

   Validate:
   - start request acceptance
   - stop request acceptance
   - init message acceptance
   - audio chunk formatting
   - interrupt behavior
   - voice end behavior

5. Add runtime/error tests.
   Cover:
   - missing API key
   - invalid enums
   - HTTP error parsing
   - WebSocket reconnect behavior
   - task cancellation on stop
   - session cache behavior

6. Add flush and `tts_audio_end` flow tests.
   Because this extension integrates with TEN control flow, tests should verify:
   - `flush` clears queued audio and sends `voice_interrupt`
   - `tts_audio_end` with completion reason triggers `voice_end`

7. Fix manifest test metadata.
   Ensure `manifest.json` points to the real test entrypoint.

Deliverables:

- real `tests/` tree
- unit tests
- contract mock tests
- shutdown/reconnect tests

## Phase 5: Example Graph And Integration Validation

Goal:

- ensure the extension is exercised in a realistic TEN setup

Tasks:

1. Add or repair a minimal example graph that uses `generic_video_python`.
   Requirements:
   - audio from TTS routed into the avatar extension
   - flush routed to the extension
   - `tts_audio_end` routed to the extension
   - video/audio published back through Agora where appropriate

2. Add a reproducible integration scenario using the local
   `convoai_to_video` checkout.
   The intent is not to depend on external provider infrastructure for every
   test, but to validate the extension against the local mock contract.

3. Decide whether to create an avatar/video guarder.
   Recommendation:
   - do not build this first
   - first add extension-local tests and one integration path
   - only create a reusable guarder if multiple avatar/video extensions need the
     same contract validation

Deliverables:

- working example graph
- documented integration procedure
- optional future guarder decision

## Phase 6: Documentation

Goal:

- make the extension understandable and maintainable without reading source first

Tasks:

1. Replace the placeholder README.
   Include:
   - purpose of the extension
   - required environment variables
   - all supported config fields
   - allowed enum values
   - expected payload flow
   - sample graph snippet
   - known limitations
   - test instructions

2. Document the audio contract clearly.
   Explain:
   - what audio format is expected from TEN
   - how `sampleRate` is derived
   - whether any resampling happens

3. Document latest `convoai_to_video` alignment assumptions.
   Include:
   - start payload fields
   - stop payload fields
   - WebSocket commands supported

Deliverables:

- complete README
- no placeholder sections
- alignment notes for future maintainers

## Recommended Execution Order

Recommended order of work:

1. Phase 1 config/schema normalization
2. Phase 2 protocol compliance
3. Phase 3 runtime robustness
4. Stop and verify the extension behavior after Phases 1-3 before expanding
   scope further
5. Phase 4 tests
6. Phase 5 integration graph
7. Phase 6 documentation

Reason:

- config drift and protocol mismatches should be removed before writing tests
  against the wrong behavior
- tests should lock in the corrected contract
- docs should reflect the corrected implementation, not the current one
- Phases 1-3 are the critical path for interoperability and safety; Phases 4-6
  should follow once those core behaviors are stable

## Acceptance Criteria

The extension should be considered complete when all of the following are true:

- all supported config fields are present and consistent across code, manifest,
  defaults, and docs
- `area` is supported and validated
- actual frame sample rates are preserved end to end
- stop-session calls match the latest checked-in contract
- no secrets are logged in plaintext
- no blocking HTTP remains in async code paths
- shutdown cancels background tasks cleanly
- manifest test script points to a real harness
- unit and mock contract tests pass
- at least one example graph exercises the extension
- README is complete and no longer placeholder text

## Nice-To-Have Follow-Ups

These are not required for first compliance:

- add richer metrics around session create/connect/interrupt timing
- add structured status reporting from provider messages instead of only logging
- add provider capability negotiation if future ConvoAI revisions expand the
  protocol
- build a reusable avatar/video protocol guarder if multiple extensions adopt
  the same transport contract
