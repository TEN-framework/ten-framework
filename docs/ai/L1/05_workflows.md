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
cp -r agents/ten_packages/extension/<existing_tts> \
  agents/ten_packages/extension/my_vendor_tts
```

Then:
1. Rename addon decorator, class names, `manifest.json` `name` field
2. Keep vendor config in `params`; extract secrets for auth and forward the rest to the vendor request layer
3. Implement the abstract methods for your vendor API
4. Create `tests/configs/` with required config files (see below)
5. Run guarder tests: `task tts-guarder-test EXTENSION=my_vendor_tts`
6. Run formatter: `task format`

**Typical required test config files** for TTS: `property_basic_audio_setting1.json`,
`property_basic_audio_setting2.json`, `property_dump.json`, `property_miss_required.json`,
`property_invalid.json`

Some extensions also keep `property.json` as a default valid config, but the exact
set depends on the extension template and guarder harness in use.

Optional / vendor-dependent TTS configs:
- `property_subtitle_alignment.json` if the vendor emits word/segment timestamps

**Required test config files** for ASR: `property_en.json`, `property_zh.json`,
`property_invalid.json`, `property_dump.json`

For full walkthrough with code and guarder expectations, see
[Extension Development](deep_dives/extension_development.md) and [Testing](deep_dives/testing.md).

### New ASR/TTS Extension Checklist

Use one recent strong example as the main template, not just any extension of
the same type.

| Type | Strong Template | Why |
| ---- | --------------- | --- |
| TTS (WS) | `deepgram_tts` | Better lifecycle and standalone test coverage |
| ASR | `openai_asr_python` | Stronger result-shape testing than thinner templates |

Minimum end-to-end steps:
1. Verify the vendor wire contract first: endpoint, event names, payload encoding, finalize primitive.
2. Copy a recent extension with the same transport shape.
3. Implement config loading, secret redaction, and `config:` logging.
4. Implement error classification and `vendor_info`.
5. Add standalone tests before running guarders.
6. Run guarders sequentially, not in parallel, inside the same container.
7. Add README and example graph wiring only after the extension tests are green.

## Add Extension to a Graph

1. **Add node** to `predefined_graphs[].graph.nodes[]` in the example's `tenapp/property.json`:
   ```json
   {"type": "extension", "name": "my_tts", "addon": "my_tts_python",
    "extension_group": "tts",
    "property": {"params": {"api_key": "${env:MY_API_KEY}"}}}
   ```

2. **Add connections** — wire data flow between extensions:
   ```json
   {"extension": "my_tts",
    "data": [{"name": "tts_text_input", "source": [{"extension": "main"}]}],
    "audio_frame": [{"name": "pcm_frame", "dest": [{"extension": "agora_rtc"}]}]}
   ```

3. **Add dependency** to example `tenapp/manifest.json` (local extensions use path):
   ```json
   {"path": "../../../ten_packages/extension/my_tts_python"}
   ```

4. **Install** (use `task install`, not just `tman install` — the latter can wipe `bin/worker`):
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

See [Graph Configuration](deep_dives/graph_configuration.md) for connection types and
routing patterns. When adding a TTS vendor, copy a confirmed working voice graph and
preserve its routing model. Some shipped graphs rely on explicit message connections,
while others keep part of the orchestration inside `main_control`.

**For complex multi-graph setups** (A/B testing vendors, avatar variants), use
`rebuild_property.py` instead of hand-editing. See
[Generating property.json](deep_dives/graph_configuration.md#generating-propertyjson-with-rebuild_propertypy).

## Customize the Main Extension

The "main" extension orchestrates agent behavior (greetings, tool routing, interruption).
Three implementation variants exist:

| Variant              | File                  | Use Case                        |
| -------------------- | --------------------- | ------------------------------- |
| Python Cascade       | `main_cascade_python` | ASR → LLM → TTS pipeline       |
| Python Realtime V2V  | `main_realtime_python`| OpenAI Realtime API (voice-to-voice) |
| Node.js Cascade      | `main_nodejs`         | TypeScript implementation       |

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

# Install extension dependencies only (creates symlinks) — WARNING: can wipe bin/worker
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
