# AI Agent Instructions

This repository uses progressive disclosure documentation to help AI coding
agents work efficiently. Documentation is structured in three levels under
`docs/ai/`.

## How to Load

1. Read [docs/ai/L0_repo_card.md](docs/ai/L0_repo_card.md) to identify the repo.
2. Load ALL 8 files in `docs/ai/L1/`. They are small — load all of them upfront.
   This gives you setup, architecture, code map, conventions, workflows,
   interfaces, gotchas, and security.
3. If a task needs more detail than L1 provides, follow links to L2 deep dives
   in `docs/ai/L1/deep_dives/`. Load only the specific L2 file you need.

## Levels

- **L0 (Repo Card):** Identity and L1 index. Table of contents.
- **L1 (Summaries):** Eight structured summaries. Load all at session start.
- **L2 (Deep Dives):** Full specifications. Load only when L1 isn't detailed enough.

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
