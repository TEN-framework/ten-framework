# 05 Workflows

> Step-by-step guides for common development tasks.

## Create a New TTS / ASR / LLM Extension

**Fastest path**: Copy a similar extension and adapt it.

| Type        | Copy From                  | Base Class                  |
| ----------- | -------------------------- | --------------------------- |
| TTS (HTTP)  | `rime_http_tts`            | `AsyncTTS2HttpExtension`    |
| TTS (WS)    | `deepgram_tts`             | `AsyncTTS2BaseExtension`    |
| ASR         | `deepgram_asr_python`      | `AsyncASRBaseExtension`     |
| LLM         | `openai_llm2_python`       | `AsyncLLMBaseExtension`     |

```bash
cp -r agents/ten_packages/extension/deepgram_tts agents/ten_packages/extension/my_vendor_tts
```

Then:
1. Rename addon decorator, class names, `manifest.json` `name` field
2. Implement the abstract methods for your vendor API
3. Create `tests/configs/` with required config files (see below)
4. Run guarder tests: `task tts-guarder-test EXTENSION=my_vendor_tts`
5. Run formatter: `task format`

**Required test config files** for TTS: `property.json`, `property_basic_audio_setting1.json`,
`property_basic_audio_setting2.json`, `property_dump.json`, `property_miss_required.json`,
`property_invalid.json`

**Required test config files** for ASR: `property_en.json`, `property_zh.json`,
`property_invalid.json`, `property_dump.json`

For full walkthrough with code and all 15/10 test details, see
[Extension Development](deep_dives/extension_development.md) and [Testing](deep_dives/testing.md).

## Add Extension to a Graph

1. **Add node** to `predefined_graphs[].graph.nodes[]` in the example's `tenapp/property.json`:
   ```json
   {"type": "extension", "name": "my_tts", "addon": "my_tts_python",
    "extension_group": "tts_group",
    "property": {"api_key": "${env:MY_API_KEY}"}}
   ```

2. **Add connections** — wire data flow between extensions:
   ```json
   {"extension": "my_tts",
    "data": [{"name": "tts_text_input", "source": [{"extension": "main"}]}],
    "audio_frame": [{"name": "pcm_frame", "dest": [{"extension": "agora_rtc"}]}]}
   ```

3. **Add dependency** to example `tenapp/manifest.json`:
   ```json
   {"type": "extension", "name": "my_tts_python", "version": "0.1.0"}
   ```

4. **Install** (use `task install`, not just `tman install` — the latter can wipe `bin/main`):
   ```bash
   docker exec ten_agent_dev bash -c "cd /app/agents/examples/<example> && task install"
   ```

5. **Nuclear restart** (required when graphs are added/removed):
   ```bash
   sudo docker exec ten_agent_dev bash -c \
     "pkill -9 -f 'bin/api'; pkill -9 -f bun; pkill -9 -f node; pkill -9 -f next-server; pkill -9 -f tman"
   sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"
   sleep 30  # wait for port 3000 TIME_WAIT to clear
   sudo docker exec -d ten_agent_dev bash -c \
     "cd /app/agents/examples/<example> && task run > /tmp/task_run.log 2>&1"
   ```

See [Graph Configuration](deep_dives/graph_configuration.md) for connection types and routing patterns.

**For complex multi-graph setups** (A/B testing vendors, avatar variants), use
`rebuild_property.py` instead of hand-editing. See
[Generating property.json](deep_dives/graph_configuration.md#generating-propertyjson-with-rebuild_propertypy).

## Customize the Main Extension

The "main" extension orchestrates agent behavior (greetings, tool routing, interruption).
Three implementation variants exist:

| Variant              | File                  | Use Case                        |
| -------------------- | --------------------- | ------------------------------- |
| Python Cascade       | `main_python_cascade` | ASR → LLM → TTS pipeline       |
| Python Realtime V2V  | `main_python_realtime`| OpenAI Realtime API (voice-to-voice) |
| Node.js Cascade      | `main_nodejs_cascade` | TypeScript implementation       |

Modify `on_data()` to change event routing, `on_cmd()` for tool handling.

## Run Tests

```bash
# All tests
docker exec ten_agent_dev bash -c "cd /app && task test"

# Single extension (with dependency install)
docker exec ten_agent_dev bash -c \
  "cd /app && task test-extension EXTENSION=agents/ten_packages/extension/deepgram_tts"

# Single extension (skip install — faster)
docker exec ten_agent_dev bash -c \
  "cd /app && task test-extension-no-install EXTENSION=agents/ten_packages/extension/deepgram_tts"

# ASR guarder integration tests
docker exec ten_agent_dev bash -c \
  "cd /app && task asr-guarder-test EXTENSION=azure_asr_python"

# TTS guarder integration tests
docker exec ten_agent_dev bash -c \
  "cd /app && task tts-guarder-test EXTENSION=deepgram_tts"
```

See [Testing](deep_dives/testing.md) for test structure and debugging.

## Restart After Changes

| What Changed                    | Action                                               |
| ------------------------------- | ---------------------------------------------------- |
| `property.json` (graphs added)  | Nuclear restart (kill all, remove lock, task run)    |
| `property.json` (config only)   | No restart needed (loaded per session)               |
| `.env`                          | `docker compose down && docker compose up -d` + deps |
| Python code                     | Restart server only                                  |
| Go code                         | `task install` then restart server                   |
| Container restart               | Reinstall Python deps, then `task run`               |

## Build and Install

```bash
# Full install (first time or after adding extensions) — ALWAYS prefer this
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/<example> && task install"

# Install Python deps only
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/<example>/tenapp && bash scripts/install_python_deps.sh"

# Install extension dependencies only (creates symlinks) — WARNING: can wipe bin/main
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/<example>/tenapp && tman install"
```

## Update Extension Code in Running Container

See [Operations and Restarts](deep_dives/operations_restarts.md) for the full procedure
including `docker cp` syntax, symlink verification, and restart steps.

## Pre-Commit Checks

```bash
# Format Python code (Black, line-length 80)
docker exec ten_agent_dev bash -c "cd /app && task format"

# Check formatting without modifying
docker exec ten_agent_dev bash -c "cd /app && task check"
```

Pre-commit hooks validate: API key patterns, Black formatting, conventional commit messages.

## Related Deep Dives

- [Extension Development](deep_dives/extension_development.md) — Full extension creation with code examples
- [Graph Configuration](deep_dives/graph_configuration.md) — Connection wiring and routing patterns
- [Testing](deep_dives/testing.md) — Test infrastructure, guarder tests, debugging
- [Operations and Restarts](deep_dives/operations_restarts.md) — Full restart procedures, recovery
