# Tavus Digital Human - Debug Guide

## Quick Testing Steps

### 1. Reinstall and Rebuild in Container

```bash
# Enter container
docker exec -it ten_agent_dev bash

# Navigate to project directory
cd /app/agents/examples/tavus-digital-human

# Clean old build
rm -rf tenapp/bin tenapp/manifest-lock.json

# Reinstall (now includes explicit build step)
task install
```

**Expected Output**:
- See "Build GO app" or go build success message
- `tenapp/bin/main` file exists with size ~2.4M

**Verification**:
```bash
ls -lh tenapp/bin/main
# Should see: -rwxr-xr-x 1 root root 2.4M ... tenapp/bin/main
```

### 2. Start Services

```bash
cd /app/agents/examples/tavus-digital-human
task run
```

**Expected Output**:
- API server starts on port 8080
- Frontend starts on port 3000
- Graph Designer starts on port 49483

**Should NOT see these errors**:
- ❌ `Error: Script 'start' exited with non-zero code: Some(127)`
- ❌ `bash: bin/main: No such file or directory`

### 3. Test Frontend

Open browser and visit: http://localhost:3000

**Checklist**:
- [ ] Page loads successfully
- [ ] See "Tavus Digital Human" title
- [ ] See "Start Conversation" button
- [ ] Open developer tools, no errors in Console

### 4. Test Creating Conversation

Click "Start Conversation" button

**Check in Browser Network Panel**:
- [ ] See POST request to `/api/tavus/conversation/create`
- [ ] Response status 200
- [ ] Response contains `conversation_url` field

**Check in Browser Console**:
- [ ] See "Conversation created:" log
- [ ] Daily iframe starts loading
- [ ] No WebRTC or Daily.co errors

### 5. Test Video Display

**Checklist**:
- [ ] See Daily.co video interface
- [ ] Can see Tavus digital human video
- [ ] Can hear audio
- [ ] Can speak and interact with digital human

## Common Issues Troubleshooting

### Issue 1: libten_runtime_go.so not found OR rwlock thread lock assertion failed

**Symptom 1 - Library not found**:
```
bin/main: error while loading shared libraries: libten_runtime_go.so: cannot open shared object file: No such file or directory
Error: Script 'start' exited with non-zero code: Some(127)
```

**Symptom 2 - Thread lock assertion (QEMU specific)**:
```
17088(17089) ten_rwlock_lock@rwlock.c:218 Invalid argument.
qemu: uncaught target signal 6 (Aborted) - core dumped
```

**Root Cause**:
- **Symptom 1**: Go binary needs to find libten_runtime_go.so library at runtime
- **Symptom 2**: TEN Framework uses custom pflock (phase-fair lock) implementation based on spinlocks and atomic operations. In QEMU emulation environment, these low-level operations may cause memory access errors or race conditions

**Solution**:

#### For Symptom 1 (library not found):
1. Ensure LD_LIBRARY_PATH includes library path:
```bash
export LD_LIBRARY_PATH=$(pwd)/ten_packages/system/ten_runtime_go/lib:$LD_LIBRARY_PATH
```

2. Or it's already configured in start.sh (latest version)

#### For Symptom 2 (rwlock assertion - QEMU environment):

**Use native Linux instead of QEMU**:
Docker Desktop on macOS uses QEMU for x86-64 Linux emulation. TEN Framework's pflock implementation is incompatible with QEMU.

**Recommended**:
1. Use **Colima** or **OrbStack** instead of Docker Desktop (uses native virtualization instead of QEMU)
2. Or run on physical Linux machine
3. Or run on WSL2 (Windows), which uses Hyper-V instead of QEMU

**Colima Installation** (macOS):
```bash
brew install colima
colima start --arch x86_64 --memory 8 --cpu 4
# Configure Docker to use Colima
export DOCKER_HOST=unix://$HOME/.colima/docker.sock
```

**If you must use QEMU**:
You can try using `TEN_RW_NATIVE` instead of `TEN_RW_PHASE_FAIR` to switch to using pthread_rwlock, but this requires recompiling the TEN Framework core library

### Issue 2: bin/main does not exist

**Symptom**:
```
Error: Script 'start' exited with non-zero code: Some(127)
bash: bin/main: No such file or directory
```

**Solution**:
```bash
cd /app/agents/examples/tavus-digital-human/tenapp
export CGO_ENABLED=1
export CGO_LDFLAGS='-L./ten_packages/system/ten_runtime_go/lib -lten_runtime_go'
export CGO_CFLAGS='-I./ten_packages/system/ten_runtime_go/interface/ten_runtime'
mkdir -p bin
go build -o bin/main -v .
```

### Issue 3: TAVUS_API_KEY not set

**Symptom**:
```
call tavus api failed: missing API key
```

**Solution**:
```bash
# Check .env file
cat /app/.env | grep TAVUS_API_KEY

# If not found, add:
echo "TAVUS_API_KEY=your_key_here" >> /app/.env
```

### Issue 4: Python dependencies missing

**Symptom**:
```
ModuleNotFoundError: No module named 'httpx'
```

**Solution**:
```bash
cd /app/agents/examples/tavus-digital-human/tenapp
pip install httpx>=0.27.0
```

### Issue 5: Frontend page 404

**Symptom**:
Browser accessing http://localhost:3000 shows 404

**Solution**:
Check if playground is running:
```bash
# In container
cd /app/playground
bun run dev
```

### Issue 6: Tavus API call failed

**Symptom**:
```
call tavus api failed: status 401
```

**Possible Causes**:
1. TAVUS_API_KEY is invalid
2. TAVUS_API_KEY has expired
3. Tavus account has insufficient balance

**Debug**:
```bash
# Test Tavus API directly
curl -X POST https://tavusapi.com/v2/conversations \
  -H "x-api-key: $TAVUS_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "persona_id": "your_persona_id",
    "conversational_context": "Test"
  }'
```

## View Logs

### TEN App Logs
```bash
# In container
tail -f /tmp/ten_agent/app-*.log
```

### API Server Logs
API server logs output directly to terminal (in the window running `task run`)

### Frontend Logs
- Browser developer tools Console panel
- Terminal output from bun run dev

## Manual API Endpoint Testing

### Test Create Conversation
```bash
curl -X POST http://localhost:8080/api/tavus/conversation/create \
  -H "Content-Type: application/json" \
  -d '{
    "persona_id": "your_persona_id"
  }'
```

**Expected Response**:
```json
{
  "code": "0",
  "data": {
    "conversation_id": "...",
    "conversation_url": "https://tavus.daily.co/...",
    "status": "active"
  }
}
```

## Verify Extension Loading

```bash
# In container
cd /app/agents/examples/tavus-digital-human/tenapp

# Test Python import
python3 -c "
import sys
sys.path.insert(0, './ten_packages/extension/tavus_conversation_manager_python')
from extension import TavusConversationManagerExtension
print('✅ Extension import successful')
"
```

## Check File Permissions

```bash
# In container
cd /app/agents/examples/tavus-digital-human/tenapp

# Check start.sh executable permission
ls -l scripts/start.sh
# Should show: -rwxr-xr-x

# If no execute permission, add:
chmod +x scripts/start.sh

# Check bin/main executable permission
ls -l bin/main
# Should show: -rwxr-xr-x

# If no execute permission, add:
chmod +x bin/main
```

## Complete Clean and Rebuild

If you encounter various strange issues, try complete cleanup and rebuild:

```bash
# In container
cd /app/agents/examples/tavus-digital-human

# Clean all build artifacts
rm -rf tenapp/bin
rm -rf tenapp/manifest-lock.json
rm -rf tenapp/ten_packages
rm -rf tenapp/.release

# Clean API server
rm -rf ../../../server/bin

# Clean frontend node_modules (optional, slow)
# rm -rf ../../../playground/node_modules

# Reinstall
task install

# Rerun
task run
```

## Compare with voice-assistant

If tavus doesn't work, first confirm if voice-assistant works:

```bash
# In container
cd /app/agents/examples/voice-assistant

task install
task run
```

Visit http://localhost:3000

If voice-assistant works but tavus doesn't, it's a tavus-specific issue.
If neither works, it's an environment configuration issue.

## Next Debugging Steps

If following the above steps still doesn't work, please provide:

1. **Complete error logs**:
   ```bash
   # Save logs to file
   cd /app/agents/examples/tavus-digital-human
   task run > /tmp/tavus-debug.log 2>&1
   ```

2. **TEN app logs**:
   ```bash
   cat /tmp/ten_agent/app-*.log
   ```

3. **File existence check**:
   ```bash
   ls -laR /app/agents/examples/tavus-digital-human/tenapp/ | grep -E '(main.go|bin/main|start.sh)'
   ```

4. **Environment variables check**:
   ```bash
   env | grep -E '(TAVUS|TEN|GO)'
   ```

5. **Browser Console errors**:
   - Press F12 to open developer tools
   - Screenshot or copy all red error messages
