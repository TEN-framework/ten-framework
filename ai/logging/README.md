# Logging Investigation

**Goal**: Make TEN Framework extension logs visible and accessible from host machine.

## Files

- **plan.md** - Comprehensive investigation plan with 6 phases
- **findings.md** - Will be created as we discover things
- **solution.md** - Final working solution once found

## Quick Status

**Current State**:
- ✅ Thymia extension works (produces wellness results)
- ❌ Extension logs not visible
- ❌ Worker log files are 0 bytes
- ❌ `Log2Stdout:true` has no effect

**Root Cause**: Unknown - investigating

## Quick Start

Follow the plan in `plan.md` step by step:
1. Phase 1: Understand logging infrastructure
2. Phase 2: Test log output directly
3. Phase 3: Check property.json configuration
4. Phase 4: Test environment variables
5. Phase 5: Make logs visible on host
6. Phase 6: Deep dive into TEN runtime (if needed)

## Hypothesis

The TEN framework `bin/main` binary is not outputting logs to stdout/stderr by default. Likely needs:
- A log level environment variable (TEN_LOG_LEVEL=DEBUG?)
- A configuration file setting
- Or uses its own internal logging system

## Document Findings

As you discover things, update `findings.md` with:
```markdown
## Finding: [Title]
**Date**: [date]
**Test**: [what you did]
**Result**: [what happened]
**Conclusion**: [what this means]
```
