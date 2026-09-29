Please review this pull request and provide feedback on:
- Code quality and best practices
- Potential bugs or issues
- Performance considerations
- Security concerns
- Test coverage

Use the repository's CLAUDE.md (and AGENTS.md / docs/ai/ progressive
disclosure docs it points to) for guidance on style and conventions.
Be constructive and helpful in your feedback.

## ASR extension changes (required when applicable)

If this PR adds or modifies an ASR extension or ASR behavior (paths under
`ai_agents/agents/ten_packages/extension/` matching `*asr*`, or changes
that affect ASR lifecycle, connection, buffering, finalize, reconnect,
result protocol, metrics, or tests), you MUST treat the following as the
merge baseline (see docs/ai/L1/05_workflows.md):

1. Read the full guide at `docs/ai/L1/L2/asr_plugin_design_review.md`
   from the checked-out repository before reviewing.
2. Evaluate the diff against every applicable **MUST** rule; call out
   violations explicitly and mark them as merge blockers unless the PR
   documents a framework contract change with migration.
3. For **SHOULD** deviations, require the PR to explain vendor limits,
   alternatives, and test evidence.
4. Include a concise ASR checklist section in your PR comment: lifecycle /
   connection state / buffering / finalize / reconnect / result shape /
   metrics / tests — note pass, fail, or N/A per item.

If the PR is not ASR-related, skip the ASR section.

## TTS extension changes (required when applicable)

If the diff changes implementation, configuration, dependencies, or tests
inside `ai_agents/agents/ten_packages/extension/<tts-extension>/`, read
`.agents/skills/ten-tts-review/SKILL.md` and follow it for those extension
files. Load its referenced common, testing, transport, and new-extension
guarder guides as instructed. For a newly added TTS extension, inspect the
existing PR comments and verify current, passing guarder evidence, including
the required result screenshot and downloadable PCM output.

Do not apply that skill to `ten_ai_base`, `tts_guarder`, examples, graphs, or
files outside the TTS extension directory. Its evidence standard and finding
format take precedence over the generic checklist above.

Report only actionable issues introduced by this PR. Keep locations tight and
avoid summaries or praise when there are no findings.

Use `gh pr diff` (and `gh pr view` as needed) with the PR NUMBER above to
inspect this PR's changes. The checked-out worktree is the base branch, not the
PR head; do not run scripts or install dependencies from the PR branch.

Use `gh pr comment` with your Bash tool to leave your review as a comment on the PR.
