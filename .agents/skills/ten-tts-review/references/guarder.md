# TTS guarder coverage and evidence

As of: 2026-10-27 (Asia/Shanghai, UTC+08:00)
Codebase commit: `11562f89409198d942a1f4eb05bec6c7bd01cb53`

## Hook coverage for every touched TTS extension

This section applies to existing and new extensions in scope. Read the guarder's
hook installer and registry solely to verify the extension's timing coverage.
Do not review unrelated guarder internals or demand unrelated framework changes.

Require an independent TTFB hook selected for the extension's actual protocol,
transport and model family. Check the registry entry and implementation, not
just a file name. The current integration uses `tests/ttfb_reference.py` and
`tests/ttfb_hooks/` under `ai_agents/agents/integration_tests/tts_guarder/`,
introduced in PR #2370; read the current interfaces rather than assuming every
checkout has these paths. Missing infrastructure is an explicit validation
prerequisite, not passing hook coverage. HTTP/SDK providers still need an
observable public send/first-audio boundary with stated limits.

The hook must independently observe first nonempty text send and first valid
nonempty audio arrival, excluding connection/session setup, controls and empty
payloads. Capture monotonic timestamps and request identity at the boundary,
then defer protocol decoding so the hook does not inflate production TTFB.
Verify per-connection isolation and provider context/session mapping, including
limitations for protocols that only support serialized requests. Never accept
production timer fields or production decoder outputs as the independent
reference. Require the tester upper bound and hook-reference comparison; an
unsupported observer does not satisfy mandatory TTFB coverage. Check focused
protocol/transport regression tests and reference-enabled live run evidence.

For subtitle alignment, first inspect whether the adapter supplies timestamped
`tts_text_result`. If supported, verify a matching hook and registration in
`tests/timing_hooks/registry.py` and independent provider/PCM reference checks.
If unsupported, accept an explicit capability explanation and mark not
applicable; do not demand fabricated word timing. Audio output alone does not
make subtitle alignment mandatory.

Report a missing/unregistered/unusable TTFB hook, or a missing alignment hook
for supported subtitles, as a P2 coverage finding on the touched extension's
changed entry point or manifest. Group equivalent hook gaps per extension.
Do not require guarder screenshots and PCM attachments for an existing extension
solely because this hook coverage check applies; those remain the new-extension
gate below.

## New-extension full-suite evidence


Apply this gate only when the TTS extension directory does not exist at the pull
request merge base. Apply it independently to every new TTS extension in the
pull request.

## Required run

Require the pull request owner to run the full default guarder suite from
`ai_agents/` with the extension's real service credentials:

```bash
task tts-guarder-test EXTENSION=<extension-directory-name>
```

Use `CONFIG_DIR=<relative-config-directory>` only when the extension does not use
the default `tests/configs`. Passing one selected test is not a full guarder run.
Capability-based skips performed by the guarder are acceptable only when the
comment identifies them and the extension genuinely lacks that capability.

Never ask the review workflow to expose or print credentials. A failed run,
credential error, or unavailable service is not passing evidence.

## Required pull request comment

Inspect all current pull request comments. Require a comment from the pull
request owner or an authorized test workflow that contains, for each new
extension:

- the extension directory name and the tested pull request HEAD commit SHA;
- the exact guarder command and its final pass/fail/skip counts;
- an inline screenshot showing the command context and final successful test
  summary, with credentials and sensitive text redacted;
- a downloadable, non-empty `.pcm` file produced by that guarder run, or a
  downloadable archive whose named contents include that `.pcm` file;
- the PCM sample rate, sample width/bit depth, and channel count needed to play
  and independently check the raw audio.

The screenshot and PCM link must be accessible from the pull request comment;
text claiming that they exist locally is insufficient. A CI log link alone is
not a screenshot, and a waveform image or encoded audio file is not the required
PCM output.

Evidence is stale when its stated SHA differs from the current pull request HEAD.
If the pull request adds multiple TTS extensions, one comment may aggregate the
evidence, but it must contain a distinct result and PCM file for each extension.

## Review outcome

Report one P1 merge-blocking finding per new extension when the run is missing or
failed, the evidence is stale, the screenshot is missing, or the PCM attachment
is missing, empty, inaccessible, or not identified as guarder output. Group
multiple missing fields for the same extension into one finding. Anchor the
finding to that extension's newly added `manifest.json` or main extension entry
point and state exactly which evidence is absent.

Do not perform a general implementation review of `tts_guarder`. The narrow
hook coverage check above may identify a required hook/registration change as
the fix for an extension coverage gap. Report other infrastructure problems
separately as the reason required evidence has not yet been satisfied.
