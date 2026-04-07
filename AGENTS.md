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

## Working Areas

- **AI Agents development**: `ai_agents/` — see `ai_agents/AGENTS.md` for workspace-specific context
- **Core framework**: `core/`, `packages/`, `build/`
- **Operational reference**: `ai/AI_working_with_ten.md` (full), `ai/AI_working_with_ten_compact.md` (quick)
