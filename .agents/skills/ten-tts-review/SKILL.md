---
name: ten-tts-review
description: Review implementation changes inside TEN Framework TTS extension directories for actionable correctness defects. Use when a pull request or diff changes Python/source code, extension-local configuration or manifests, dependencies, or tests under `ai_agents/agents/ten_packages/extension/` for a TTS extension. Focus on request lifecycle, exact `tts_audio_end` duration and event-interval accounting, streaming and flush races, PCM format, vendor errors, metrics, secrets, and regression coverage. Do not use for `ten_ai_base`, `tts_guarder`, graphs, examples, audio routing outside the extension, other extension types, or documentation-only changes.
---

# TEN TTS review

Find defects that can affect users or make the TTS contract unreliable. Prefer a
small number of high-confidence findings over a broad checklist report.

## Review workflow

1. Read the repository instructions before reviewing. At minimum, load
   `AGENTS.md`, `docs/ai/L0_repo_card.md`, and all files in `docs/ai/L1/`.
2. Establish the exact diff and its base. For a pull request, inspect the PR
   metadata and full diff. For a local review, use the merge base with the target
   branch. Do not infer a defect from a truncated patch.
3. Build the in-scope file list first. Review only changed files under
   `ai_agents/agents/ten_packages/extension/<tts-extension>/`, where the extension
   name or its imports/manifest identify it as TTS. Source code, `addon.py`,
   `config.py`, `manifest.json`, `property.json`, dependency files, and
   extension-local tests are in scope. README-only changes do not trigger this
   skill. Ignore every changed path outside that extension directory, including
   `ten_ai_base`, `integration_tests/tts_guarder`, examples, and graphs.
4. Determine whether each in-scope TTS extension is new by checking whether its
   extension directory exists at the pull request merge base. Do not infer this
   from the number of added files: a replacement, rename, or large update to an
   existing directory is not a new extension.
5. Classify each in-scope extension from its actual inheritance and imports, not
   its directory name:
   - `AsyncTTS2HttpExtension` -> HTTP implementation
   - direct `AsyncTTS2BaseExtension` -> WebSocket/custom implementation
6. Load [references/common.md](references/common.md) and
   [references/tests.md](references/tests.md) for every review. Then load exactly
   the applicable transport guide:
   - HTTP: [references/http.md](references/http.md)
   - WebSocket/custom: [references/websocket.md](references/websocket.md)
   If a pull request changes multiple TTS extensions, classify and review each
   independently; load both transport guides only when both shapes are present.
   For every new TTS extension, also load and apply
   [references/guarder.md](references/guarder.md).
7. Read the changed functions in full. You may inspect the installed base-class
   methods they call to understand the contract, but do not review `ten_ai_base`
   itself or ask the pull request to change it. Compare with one recent extension
   using the same transport shape; copied legacy patterns are not proof.
8. Follow each stateful path through success, empty input, vendor error,
   cancellation, and shutdown. For HTTP extensions, distinguish inherited base
   behavior from extension-owned behavior before raising a finding.
9. Audit every changed call to `send_tts_audio_end()`. Independently verify
   `request_total_audio_duration_ms` and `request_event_interval_ms`; these fields
   are mandatory correctness checks, including partial, interrupted, error, and
   empty-audio paths. When an HTTP extension inherits this logic unchanged, do
   not demand a duplicate implementation in the extension.
10. Treat extension-local tests as a merge requirement. Every in-scope TTS
   extension must have its own tests and satisfy the mandatory matrix in
   `references/tests.md`; shared guarder or integration tests do not substitute
   for them. Report a missing suite or mandatory category even when the
   production defect has not yet been demonstrated.
11. For each new TTS extension, inspect pull request comments and enforce the
   guarder evidence gate. The full guarder suite must pass against the current
   pull request HEAD, and a comment must include both a result screenshot and a
   downloadable non-empty PCM output file. Guarder evidence does not replace
   extension-local tests.
12. Report implementation issues only when introduced or made materially worse
   by in-scope diff hunks. The testing baselines are explicit exceptions: report
   extension-local test gaps for every touched TTS extension and guarder-evidence
   gaps for every new TTS extension.
   Verify each finding against surrounding code; do not flag style, preferences,
   or purely hypothetical risks.

## Evidence standard

A finding should form a complete causal chain:

`changed code -> reachable runtime condition -> violated invariant -> user-visible impact`

Before reporting, answer all of these:

- What input, event ordering, or vendor response triggers it?
- Why do guards, the base class, or cleanup code not prevent it?
- Which request, audio, error, metric, or secret is affected?
- Is the faulty behavior owned by the changed extension rather than an unchanged
  installed base class?
- Can the location be narrowed to changed lines?

If any answer is missing, investigate further or omit the finding.

## Severity

- **P0**: widespread outage, credential exposure, or destructive behavior.
- **P1**: common requests break, cross-request audio leaks, flush is violated, or
  the extension cannot recover without restart. A new or changed TTS extension
  with no extension-local test suite is also P1. A new TTS extension without
  current, passing guarder evidence and both required comment attachments is P1.
- **P2**: a realistic edge case produces wrong audio/events/metrics or resource
  leakage with meaningful impact, or the mandatory local-test matrix is
  materially incomplete.
- **P3**: limited but concrete correctness or maintainability defect worth fixing.

Do not inflate severity because code is concurrent or vendor-facing.

## Output

List findings first, ordered by severity. Use this form:

```text
[P1] Imperative, specific title
path/to/file.py:123
Explain the triggering sequence, the incorrect behavior, and its impact in one
compact paragraph. Mention the relevant contract or test when useful, and give a
bounded fix direction without writing a patch.
```

Keep line ranges tight and inside the diff whenever possible. Do not combine
independent bugs in one finding. Do not add praise, a walkthrough of the patch, or
generic testing advice. If there are no actionable findings, say:

```text
No actionable TTS-specific findings.
```
