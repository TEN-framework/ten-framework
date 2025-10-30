# Plan: Discover Minimal Logging Configuration

## Goal
Find the minimal changes needed to make TEN Framework logging work, without modifying shared platform code (worker.go).

## Current State Analysis

### .env.example (Official Template)
```bash
LOG_PATH=/tmp/ten_agent
LOG_STDOUT=true                    # ← Missing in our .env
WORKERS_MAX=100                     # ← Missing in our .env
WORKER_QUIT_TIMEOUT_SECONDS=60     # ← Missing in our .env
```

### Our Current .env
```bash
LOG_PATH=/tmp/ten_agent
TEN_LOG_FORMATTER=json             # ← We added this
PYTHONUNBUFFERED=1                 # ← We added this
# Missing: LOG_STDOUT, WORKERS_MAX, WORKER_QUIT_TIMEOUT_SECONDS
```

### Changes We Made
1. **worker.go** - Added `cmd.Env`, rewrote PrefixWriter (LIKELY UNNECESSARY)
2. **.env** - Added TEN_LOG_FORMATTER, PYTHONUNBUFFERED (may be redundant with LOG_STDOUT)
3. **property.json** - Added `ten.log` configuration (LIKELY NECESSARY)

## Hypothesis

The **official** way to enable logging is probably:
- `LOG_STDOUT=true` in .env (from .env.example)
- `ten.log` configuration in property.json (required for ten_env.log_*() to work)

The worker.go changes and TEN_LOG_FORMATTER may be unnecessary if LOG_STDOUT is the intended mechanism.

## Test Plan

### Phase 1: Revert to Original worker.go
1. Check git history to find original worker.go
2. Revert worker.go to feat/thymia branch version (before our changes)
3. Keep .env and property.json changes
4. Test if logging still works

**Expected Result:** If logging still works, worker.go changes were unnecessary.

### Phase 2: Test Official .env Configuration
1. Update .env to match .env.example pattern:
   ```bash
   LOG_PATH=/tmp/ten_agent
   LOG_STDOUT=true
   WORKERS_MAX=100
   WORKER_QUIT_TIMEOUT_SECONDS=60
   ```
2. Remove our custom variables (TEN_LOG_FORMATTER, PYTHONUNBUFFERED) OR test both ways
3. Keep property.json ten.log configuration
4. Restart container and test

**Expected Result:** Logging should work with LOG_STDOUT=true alone.

### Phase 3: Test property.json Necessity
1. Try removing ten.log configuration from property.json
2. Test if ten_env.log_*() calls still work
3. Test if print() statements still work

**Expected Result:**
- print() should work regardless
- ten_env.log_*() likely needs ten.log configuration

### Phase 4: Document Minimal Configuration

Based on test results, document the **minimal required changes**:
- Which .env variables are needed?
- Is property.json ten.log required?
- Are worker.go changes needed?

## Success Criteria

✅ Logging works with:
- No modifications to worker.go
- Only standard .env variables from .env.example
- Minimal property.json configuration (if any)

✅ Both types of logs visible:
- Python print() statements
- TEN framework ten_env.log_*() calls

✅ Logs appear with channel prefixes in /tmp/task_run.log

## Rollback Strategy

If tests fail, we can revert to current working configuration and document that as the solution.
