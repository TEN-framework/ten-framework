# Phase 1 Findings: Logging Infrastructure

## Environment Variables Discovered

### Current .env Settings
```bash
LOG_PATH=/tmp/ten_agent
```

### TEN Framework Environment Variables (found in tman binary)
```
TEN_ENABLE_BACKTRACE_DUMP - Enable/disable backtrace dumps
TEN_LOG_FORMATTER - Log format control
TEN_ENABLE_PYTHON_DEBUG - Python debugging (commented in start.sh)
TEN_PYTHON_DEBUG_PORT - Python debug port (commented in start.sh)
```

## tman Command

**Location**: `/usr/local/bin/tman`

**Flags Available**:
- `--verbose` - Enable verbose output
- `-c, --config-file <CONFIG_FILE>` - Config location

**Scripts**: Runs scripts from manifest.json
- `start` script → `scripts/start.sh` → `bin/main`

## Start Script Analysis

**File**: `/app/agents/examples/voice-assistant-advanced/tenapp/scripts/start.sh`

Sets up environment and runs `bin/main`:
```bash
export PYTHONPATH=$(pwd)/ten_packages/system/ten_ai_base/interface:$PYTHONPATH
export LD_LIBRARY_PATH=...
export NODE_PATH=...
exec bin/main "$@"
```

**Note**: Has commented debug flags:
```bash
#export TEN_ENABLE_PYTHON_DEBUG=true
#export TEN_PYTHON_DEBUG_PORT=5678
```

## Key Discovery

**tman has `--verbose` flag** which might enable logging! Need to test if worker.go can pass flags to tman.
