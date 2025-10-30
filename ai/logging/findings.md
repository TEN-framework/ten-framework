# Logging Investigation Findings

**Investigation Date**: 2025-10-30

---

## Known Facts (Starting Point)

### What Works
1. ✅ Thymia extension produces wellness results:
   ```
   Distress: 0.5, Stress: 0.6, Burnout: 0.2, Fatigue: 0.0, Self-Esteem: 0.0
   ```
2. ✅ Voice assistant responds and speaks
3. ✅ Extensions load successfully (confirmed by functionality)
4. ✅ API server responds to health checks

### What Doesn't Work
1. ❌ Worker log files are 0 bytes: `app-{channel}-{timestamp}.log`
2. ❌ Extension logs don't appear in `/tmp/task_run.log`
3. ❌ `Log2Stdout:true` in worker.go has no effect
4. ❌ Python `print()` statements don't appear
5. ❌ `ten_env.log_info()` calls don't appear

### Code Analysis
1. ✅ **worker.go lines 145-174**: Correctly sets up stdout/stderr capture
2. ✅ **worker.go lines 173-174**: Connects cmd.Stdout/Stderr to PrefixWriter
3. ✅ **worker.go line 201-202**: Sets prefix to channel name
4. ❌ **Problem**: `tman run start` or `bin/main` produces NO output

---

## Investigation Results

*Fill in as you test each phase of the plan*

### Phase 1: Logging Infrastructure

#### Test 1.1: Search for log configuration
```bash
# Command:
docker exec ten_agent_dev find /app/agents -name "*.json" -o -name "*.yaml" | xargs grep -l "log"

# Result:
[paste results here]

# Conclusion:
[what this tells us]
```

#### Test 1.2: Check tman help
```bash
# Command:
docker exec ten_agent_dev tman --help

# Result:
[paste results here]

# Conclusion:
[what flags are available?]
```

#### Test 1.3: Check environment variables
```bash
# Command:
docker exec ten_agent_dev env | grep -i log

# Result:
[paste results here]

# Conclusion:
[any log-related env vars?]
```

---

### Phase 2: Direct Testing

#### Test 2.1: Run bin/main directly
```bash
# Command:
[paste command]

# Result:
[paste output]

# Conclusion:
[does it produce ANY output?]
```

#### Test 2.2: Strace analysis
```bash
# Command:
[paste command]

# Result:
[paste output]

# Conclusion:
[does it call write() syscalls?]
```

---

### Phase 3: property.json Configuration

#### Test 3.1: Search property.json for log settings
```bash
# Command:
[paste command]

# Result:
[paste output]

# Conclusion:
[any log_level or log configuration?]
```

---

### Phase 4: Environment Variables

#### Test 4.1: Try TEN_LOG_LEVEL=DEBUG
```bash
# Command:
[paste command]

# Result:
[paste output]

# Conclusion:
[does it enable logging?]
```

---

### Phase 5: Host Access

#### Test 5.1: Volume mount
```bash
# Command:
[paste command]

# Result:
[paste output]

# Conclusion:
[can we see logs on host now?]
```

---

### Phase 6: Deep Dive

#### Test 6.1: Examine TEN runtime
```bash
# Command:
[paste command]

# Result:
[paste output]

# Conclusion:
[what did we learn about TEN runtime logging?]
```

---

## Key Discoveries

*Update this section as you find important things*

### Discovery 1: [Title]
**What**: [description]
**How**: [how you found it]
**Impact**: [what this means for logging]

### Discovery 2: [Title]
**What**: [description]
**How**: [how you found it]
**Impact**: [what this means for logging]

---

## Hypothesis Updates

### Initial Hypothesis
TEN framework `bin/main` doesn't output to stdout by default.

### Updated Hypothesis (after testing)
[Update based on what you learn]

---

## Next Actions

Based on findings:
1. [Action item 1]
2. [Action item 2]
3. [Action item 3]
