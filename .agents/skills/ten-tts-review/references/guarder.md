# New-extension guarder evidence

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

Do not review or request implementation changes inside `tts_guarder`; it remains
out of scope. Report infrastructure problems separately as the reason the
required evidence has not yet been satisfied.
