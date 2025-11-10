# Hellos API Migration: curl → aiohttp + In-Memory

**Date**: 2025-11-10
**Issue**: Hellos API uses curl subprocess + disk files, should use aiohttp + in-memory like Apollo

---

## Problems Identified

1. **ThymiaAPIClient.upload_audio** uses curl subprocess with tempfiles (extension.py:403-459)
2. **_run_hellos_phase** saves WAV to disk, re-reads it, then uploads (extension.py:1327-1358)
3. **Buffer never cleared** after both APIs complete - old audio accumulates
4. **Old /tmp files persist** - could be reused across sessions
5. **User names accumulate history** on Thymia's servers (e.g., "Mark" has too much history)

## Working Reference: Apollo API

Apollo API does it correctly (apollo_api.py:124-152):

```python
async def upload_audio(self, upload_url: str, pcm_data: bytes, sample_rate: int = 16000):
    """Upload audio to presigned URL - IN MEMORY"""
    await self._ensure_session()

    # Convert PCM to WAV format IN MEMORY
    wav_data = self._pcm_to_wav_bytes(pcm_data, sample_rate)

    headers = {"Content-Type": "audio/wav"}

    # Upload directly from memory using aiohttp
    async with self.session.put(upload_url, data=wav_data, headers=headers) as response:
        if response.status not in (200, 201, 204):
            error_text = await response.text()
            raise RuntimeError(f"Apollo audio upload failed: {response.status} - {error_text}")
```

---

## Changes Required

### Change 1: Migrate ThymiaAPIClient.upload_audio to aiohttp

**Current** (extension.py:403-459):
```python
async def upload_audio(self, upload_url: str, wav_data: bytes) -> bool:
    """Upload WAV audio file to presigned S3 URL using curl"""
    # Creates tempfile
    with tempfile.NamedTemporaryFile(mode="wb", suffix=".wav", delete=False) as tmp_file:
        tmp_file.write(wav_data)
        tmp_filename = tmp_file.name

    try:
        curl_cmd = ["curl", "-X", "PUT", upload_url, "--data-binary", f"@{tmp_filename}"]
        process = await asyncio.create_subprocess_exec(*curl_cmd, ...)
        ...
    finally:
        os.unlink(tmp_filename)
```

**New** (match Apollo API pattern):
```python
async def upload_audio(self, upload_url: str, wav_data: bytes) -> bool:
    """Upload WAV audio to presigned S3 URL using aiohttp"""
    await self._ensure_session()

    headers = {"Content-Type": "audio/wav"}

    try:
        async with self.session.put(upload_url, data=wav_data, headers=headers) as response:
            if response.status not in (200, 201, 204):
                error_text = await response.text()
                print(
                    f"[THYMIA_HELLOS_UPLOAD_ERROR] Upload failed: status={response.status}, error={error_text}",
                    flush=True,
                )
                return False

            print(
                f"[THYMIA_HELLOS_UPLOAD] Upload successful: status={response.status}",
                flush=True,
            )
            return True
    except Exception as e:
        print(
            f"[THYMIA_HELLOS_UPLOAD_ERROR] Exception during upload: {e}",
            flush=True,
        )
        return False
```

### Change 2: Remove Disk I/O from _run_hellos_phase

**Current** (extension.py:1327-1358):
```python
async def _run_hellos_phase(self, ten_env: AsyncTenEnv):
    wav_data = self.audio_buffer.get_wav_data()

    # REMOVE THIS: Save to disk
    mood_filename = f"/tmp/thymia_audio_{timestamp}_{self.user_name or 'unknown'}_mood.wav"
    with open(mood_filename, "wb") as f:
        f.write(wav_data)
    self.saved_mood_wav_path = mood_filename

    # REMOVE THIS: Re-read from disk
    with open(mood_filename, "rb") as f:
        file_bytes = f.read()
        upload_success = await self.api_client.upload_audio(upload_url, file_bytes)
```

**New** (upload directly from memory):
```python
async def _run_hellos_phase(self, ten_env: AsyncTenEnv):
    wav_data = self.audio_buffer.get_wav_data()

    if not wav_data:
        ten_env.log_warn("[THYMIA_HELLOS_PHASE_1] No audio data available")
        return

    # Create session
    session_response = await self.api_client.create_session(...)
    session_id = session_response["id"]
    upload_url = session_response["recordingUploadUrl"]

    # Upload directly from memory (no disk I/O)
    upload_success = await self.api_client.upload_audio(upload_url, wav_data)

    if not upload_success:
        ten_env.log_error("[THYMIA_HELLOS_PHASE_1] Failed to upload audio")
        return
```

### Change 3: Remove Disk I/O from _run_apollo_phase

**Current** (extension.py:1418-1448):
```python
async def _run_apollo_phase(self, ten_env: AsyncTenEnv):
    # Split PCM data
    mood_pcm, read_pcm = self._split_pcm_by_duration(full_pcm_data, 30.0)

    # REMOVE THIS: Reuse saved mood.wav file
    if self.saved_mood_wav_path:
        mood_filename = self.saved_mood_wav_path
    else:
        mood_wav = AudioBuffer.pcm_to_wav(mood_pcm, 16000, 1)
        mood_filename = f"/tmp/thymia_audio_{timestamp}_mood.wav"
        with open(mood_filename, "wb") as f:
            f.write(mood_wav)

    # REMOVE THIS: Save reading.wav
    read_wav = AudioBuffer.pcm_to_wav(read_pcm, 16000, 1)
    read_filename = f"/tmp/thymia_audio_{timestamp}_reading.wav"
    with open(read_filename, "wb") as f:
        f.write(read_wav)
```

**New** (Apollo already uses in-memory - no change needed):
```python
async def _run_apollo_phase(self, ten_env: AsyncTenEnv):
    # Split PCM data
    mood_pcm, read_pcm = self._split_pcm_by_duration(full_pcm_data, 30.0)

    # Call Apollo API directly with PCM data (already in-memory)
    apollo_result = await self.apollo_client.analyze(
        mood_audio_pcm=mood_pcm,
        read_aloud_audio_pcm=read_pcm,
        user_label=self.user_name or "anonymous",
        date_of_birth=self.user_dob or "1990-01-01",
        birth_sex=self.user_sex or "OTHER",
        sample_rate=16000,
        language="en-GB",
    )
```

### Change 4: Clear Buffer After Both APIs Sent

**Add to AudioBuffer class** (extension.py:~245):
```python
def clear_buffer(self):
    """Clear speech buffer after audio has been sent to APIs"""
    self.speech_buffer.clear()
    self.speech_duration = 0.0
    print("[THYMIA_BUFFER_CLEAR] Speech buffer cleared", flush=True)
```

**Call after both APIs uploaded** (extension.py:~1380 and ~1465):
```python
async def _run_hellos_phase(self, ten_env: AsyncTenEnv):
    ...
    upload_success = await self.api_client.upload_audio(upload_url, wav_data)

    if upload_success:
        ten_env.log_info("[THYMIA_HELLOS_PHASE_1] Uploaded audio - checking if Apollo also sent")

        # If Apollo already sent, clear buffer
        if self.apollo_complete or self.apollo_analysis_running:
            ten_env.log_info("[THYMIA_BUFFER] Both APIs sent/sending - clearing buffer")
            self.audio_buffer.clear_buffer()

async def _run_apollo_phase(self, ten_env: AsyncTenEnv):
    ...
    # After Apollo API completes
    self.apollo_complete = True

    # If Hellos already sent, clear buffer
    if self.hellos_complete or self.hellos_session_id:
        ten_env.log_info("[THYMIA_BUFFER] Both APIs sent - clearing buffer")
        self.audio_buffer.clear_buffer()
```

### Change 5: Remove saved_mood_wav_path Variable

**Remove from __init__** (extension.py:~580):
```python
# DELETE THIS LINE:
self.saved_mood_wav_path: Optional[str] = None
```

**Remove references** throughout the file - no longer needed since we don't save files

---

## Testing Plan

### Test 1: Verify No Disk Files Created
```bash
# Clear /tmp before test
sudo docker exec ten_agent_dev bash -c "rm -f /tmp/thymia_audio_*.wav"

# Run test session
# ...

# Check no files created
sudo docker exec ten_agent_dev bash -c "ls -lh /tmp/thymia_audio_*.wav 2>&1"
# Should output: "No such file or directory"
```

### Test 2: Verify Both APIs Upload Successfully
```bash
# Check logs for upload success
sudo docker exec ten_agent_dev bash -c "grep 'THYMIA.*UPLOAD' /tmp/task_run.log"

# Expected:
# [THYMIA_HELLOS_UPLOAD] Upload successful: status=200
# [THYMIA_APOLLO_UPLOAD] (already working via aiohttp)
```

### Test 3: Verify Buffer Cleared
```bash
# Check logs for buffer clear
sudo docker exec ten_agent_dev bash -c "grep 'THYMIA_BUFFER_CLEAR' /tmp/task_run.log"

# Expected:
# [THYMIA_BUFFER_CLEAR] Speech buffer cleared
```

### Test 4: Verify No Name History Issues
```bash
# Use unique name per test
# property.json should use: "TestUser_${timestamp}" instead of fixed names
# Or use random UUID suffix: "TestUser_abc123"
```

---

## Benefits

1. ✅ **No disk I/O** - faster, no file cleanup needed
2. ✅ **Consistent with Apollo** - both APIs use same pattern
3. ✅ **No tempfile accumulation** - prevents /tmp filling up
4. ✅ **No name history issues** - buffer cleared after each session
5. ✅ **Simpler code** - fewer file operations, less error handling
6. ✅ **Better debugging** - aiohttp errors more informative than curl

---

## Implementation Order

1. ✅ Write this plan
2. Migrate ThymiaAPIClient.upload_audio to aiohttp
3. Remove disk I/O from _run_hellos_phase
4. Remove saved_mood_wav_path references
5. Add AudioBuffer.clear_buffer() method
6. Call clear_buffer() after both APIs sent
7. Test session with no disk files created
8. Verify both APIs work correctly
9. Update status.md with changes

---

## Status

- ⏳ Plan written
- ⏳ Implementation pending
- ⏳ Testing pending
