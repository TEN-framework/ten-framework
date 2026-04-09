# AI Agent Instructions

This repository uses progressive disclosure documentation. Docs live under
`docs/ai/` in three levels.

## How to Load

1. Read [docs/ai/L0_repo_card.md](docs/ai/L0_repo_card.md) to identify the repo.
2. Load ALL 8 files in `docs/ai/L1/`. They are small — load all upfront.
3. Follow L2 deep-dive links only when L1 is not detailed enough.

## Git Conventions

- **Lowercase start** — commit messages begin with a lowercase letter
- **No AI tool names** — never mention claude, cursor, copilot, cody, aider,
  gemini, codex, chatgpt, or gpt-3/4
- **Present tense** — "add feature", not "added feature"
- **No Co-Authored-By trailers** — omit AI attribution lines
- **No --no-verify** — let git hooks run normally
- **No git config changes** — do not modify `user.name` or `user.email`

## Doc Commands

| Command       | When to Use                                  |
| ------------- | -------------------------------------------- |
| `generate docs` | No `docs/ai/` directory exists yet         |
| `update docs` | Code changed since the `last_reviewed` date  |
| `test docs`   | Verify docs give agents the right context    |

## Working Areas

- `ai_agents/` — primary area for agents, examples, server, integrations
- `core/`, `packages/`, `build/` — framework internals
- `ai_agents/AGENTS.md` — workspace-specific guidance for `ai_agents/`
