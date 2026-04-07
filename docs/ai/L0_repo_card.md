# TEN Framework — Repo Card

> Open-source platform for building real-time multimodal AI agents with voice, video, and tool capabilities.

## Identity

| Field         | Value                                                                |
| ------------- | -------------------------------------------------------------------- |
| Repo          | `TEN-framework/TEN-Agent`                                           |
| Type          | `framework` (SDK-library + API-service + frontend)                   |
| Language      | Python (extensions), Go (API server), TypeScript/React (playground)  |
| Deploy Target | Docker container (`ten_agent_dev`), Taskfile-based build             |
| Owner         | TEN Framework team                                                   |
| Last Reviewed | 2026-04-02                                                           |

## L1 — Summaries

| File                                     | Purpose                                                  |
| ---------------------------------------- | -------------------------------------------------------- |
| [01_setup](L1/01_setup.md)               | Docker, .env, ports, health checks, restart procedures   |
| [02_architecture](L1/02_architecture.md) | Extensions, graphs, connections, RTC-first design        |
| [03_code_map](L1/03_code_map.md)         | Directory tree, key files, base classes, 93+ extensions  |
| [04_conventions](L1/04_conventions.md)   | Naming, Pydantic configs, params pattern, formatting     |
| [05_workflows](L1/05_workflows.md)       | Create extension, modify graph, test, restart, deploy    |
| [06_interfaces](L1/06_interfaces.md)     | REST API, connection schemas, base class abstract methods|
| [07_gotchas](L1/07_gotchas.md)           | Property tuples, signal handlers, zombies, .env timing   |
| [08_security](L1/08_security.md)         | API keys, .env, sensitive logging, git hooks             |

## L2 — Deep Dives

See [L1/deep_dives/_index.md](L1/deep_dives/_index.md) for extended guides referenced by L1 files.
