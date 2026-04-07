# TEN Framework — Repo Card

## Identity

| Field         | Value                                                                |
| ------------- | -------------------------------------------------------------------- |
| Repo          | `TEN-framework/TEN-Agent`                                           |
| Description   | Open-source platform for building real-time multimodal AI agents     |
| Repo Type     | `distributed-system`                                                 |
| Language      | Python (extensions), Go (API server), TypeScript/React (playground)  |
| Deploy Target | Docker container (`ten_agent_dev`), Taskfile-based build             |
| Owner         | TEN Framework team                                                   |
| Last Reviewed | 2026-04-07                                                           |

## L1 Index

| File                                     | Purpose                                                  | Audience |
| ---------------------------------------- | -------------------------------------------------------- | -------- |
| [01_setup](L1/01_setup.md)               | Docker, .env, ports, health checks, restart procedures   | both     |
| [02_architecture](L1/02_architecture.md) | Extensions, graphs, connections, RTC-first design        | both     |
| [03_code_map](L1/03_code_map.md)         | Directory tree, key files, base classes, 93+ extensions  | both     |
| [04_conventions](L1/04_conventions.md)   | Naming, Pydantic configs, params pattern, formatting     | both     |
| [05_workflows](L1/05_workflows.md)       | Create extension, modify graph, test, restart, deploy    | both     |
| [06_interfaces](L1/06_interfaces.md)     | REST API, connection schemas, base class abstract methods| both     |
| [07_gotchas](L1/07_gotchas.md)           | Property tuples, signal handlers, zombies, .env timing   | both     |
| [08_security](L1/08_security.md)         | API keys, .env, sensitive logging, git hooks             | both     |
