# 07 Gotchas

> Critical pitfalls, tribal knowledge, and troubleshooting.

## CRITICAL: Property Getters Return Tuples

All `get_property_*()` methods return `(value, error_or_none)`, not the raw value.

```python
# WRONG — causes TypeError
threshold = await ten_env.get_property_float("threshold")
if threshold > 0.5:  # TypeError: '>' not supported between 'float' and 'tuple'

# CORRECT — extract from tuple
threshold_result = await ten_env.get_property_float("threshold")
threshold = threshold_result[0] if isinstance(threshold_result, tuple) else threshold_result
```

This applies to `get_property_string()`, `get_property_int()`, `get_property_float()`,
`get_property_bool()`. Always extract `[0]`.

## CRITICAL: Signal Handlers Forbidden

Extensions run in worker threads. Signal handlers only work in the main thread.

```python
# WRONG — raises ValueError: signal only works in main thread
signal.signal(signal.SIGTERM, handler)
atexit.register(cleanup)

# CORRECT — use extension lifecycle
async def on_stop(self, ten_env):
    await self.cleanup()
```

## CRITICAL: Always Use `task run`

Never start the server with `./bin/api` or `./bin/worker` directly.
`task run` sets the correct PYTHONPATH and starts all services together
(API server + playground + TMAN Designer).

## Zombie Worker Processes

Worker processes (`bin/worker`) can survive container and server restarts.
Always check for and kill zombies before restarting.

## .env Loaded at Container Startup Only

Editing `.env` while the container is running has **no effect**. You must
`docker compose down && docker compose up -d`, then reinstall Python deps.

## Next.js Lock File

After crashes, `.next/dev/lock` becomes stale, preventing restart. Delete it
and do a full restart. See [Operations and Restarts](deep_dives/operations_restarts.md).

## Python Deps Not Persisted

Python dependencies are lost on container restart. Always reinstall after
`docker compose down && up`.

## tman install Can Wipe bin/worker

Running `tman install` when system dependencies have newer versions replaces
the runtime packages and **deletes `bin/worker`**. Use `task install` (full
rebuild) instead of bare `tman install`. Signs: Worker fails with
`bin/worker: No such file or directory` in logs.

## tman Install Creates Symlinks

Never manually `ln -s` for extensions. Use `tman install` which resolves
dependencies and creates correct links. If a symlink is missing after
`tman install`, create it manually as a fallback.

## docker cp Creates Nested Directories

When using `docker cp` to update extension code, trailing slashes create
nested directories. Use `docker cp ./ext/. container:/path/ext/` syntax.
Signs: `ModuleNotFoundError: No module named 'ten_packages.extension.X'`.

## Audio Routing: Split at Source Only

When routing audio to multiple destinations, the split must happen at the
source node (e.g., `agora_rtc`), not at intermediate nodes. Splitting from
intermediate nodes can cause crashes.

## Frontend Caches Graph List

The playground caches the `/graphs` API response. When adding or removing
graphs from `property.json`, a full restart is required — simple server
restart is not enough.

## Guarder Symlink Drift

Guarder apps create their own `ten_packages/extension/*` symlinks. These can
point at dead host paths and break imports after worktree changes or manual
copies.

Typical symptom:
`ModuleNotFoundError: No module named 'ten_packages.extension.<name>'`

Reliable fix inside the container:

```bash
ln -sf /app/agents/ten_packages/extension/<name> \
  /app/agents/integration_tests/<guarder>/ten_packages/extension/<name>
```

## Run Guarders Sequentially

Do not run ASR and TTS guarders in parallel in the same container. Their build
scripts may collide on shared temp paths and fail with errors like:
`/tmp/test: Text file busy`

Run one guarder to completion, then start the other.

## Production Graph Picker 403

If the playground requests `/api/dev/v1/graphs` in production, the graph list
may be empty even though `/api/agents/graphs` is healthy. Check
`NEXT_PUBLIC_EDIT_GRAPH_MODE`; production deployments should typically set it to
`false`.

## Manifest Module Name Must Match

The `name` field in extension `manifest.json` must exactly match the `addon`
field used in graph nodes in `property.json`. Mismatches cause silent failures.

## next-server Holds Port 3000

Killing `node` and `bun` is not enough — `next-server` is a separate process
that holds port 3000. If port 3000 is occupied, Next.js silently starts on
3001+ which isn't Docker-exposed, making the frontend appear down.

## Apple Silicon Docker

Docker containers may need Rosetta for x86 images on Apple Silicon Macs.
Enable in Docker Desktop: Settings > General > Use Rosetta.

## Windows Line Endings

Before cloning on Windows: `git config --global core.autocrlf false`

## Related Deep Dives

- [Operations and Restarts](deep_dives/operations_restarts.md) — Full restart procedures, port debugging, recovery
- [Deployment](deep_dives/deployment.md) — Production setup, persistent startup
- [Server Architecture](deep_dives/server_architecture.md) — Worker lifecycle, session management
