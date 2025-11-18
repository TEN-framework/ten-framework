# Flush Handling in TEN Framework Extensions

## Summary

**PR Review Comment Context:** "flush handling is by default handled in ten_ai_base, so you don't need to add your own cmd handling"

**This applies ONLY to extensions inheriting from specialized ten_ai_base classes.**

---

## Base Classes and Flush Handling

### 1. AsyncExtension (Generic Base)
**Location:** `ten_runtime_python/interface/ten/async_extension.py:222`

```python
async def on_cmd(self, async_ten_env: AsyncTenEnv, cmd: Cmd) -> None:
    pass  # No-op - does nothing
```

**Result:** ❌ **NO default flush handling**

### 2. AsyncTTSBaseExtension (Specialized TTS Base)
**Location:** `ten_ai_base/interface/ten_ai_base/tts.py:57-74`

```python
async def on_cmd(self, async_ten_env: AsyncTenEnv, cmd: Cmd) -> None:
    cmd_name = cmd.get_name()

    if cmd_name == CMD_IN_FLUSH:
        await self.on_cancel_tts(async_ten_env)
        await self.flush_input_items(async_ten_env)
        await async_ten_env.send_cmd(Cmd.create(CMD_OUT_FLUSH))
        # return success result
```

**Result:** ✅ **Has default flush handling**

### 3. Other Specialized Bases
- `AsyncASRBaseExtension` - **Not found** (may not exist yet)
- `LLMBaseExtension` - TBD
- **No video/avatar base classes exist**

---

## Our Extensions

| Extension | Inherits From | Has Custom Flush? | Correct? |
|-----------|---------------|-------------------|----------|
| **heygen_avatar_python** | `AsyncExtension` | ✅ Yes | ✅ **MUST have it** |
| **generic_video_python** | `AsyncExtension` | ✅ Yes | ✅ **MUST have it** |
| **deepgram_ws_asr_python** | `AsyncExtension` | ❌ No | ⚠️ **May need it** |
| **thymia_analyzer_python** | `AsyncExtension` | ❌ No | ⚠️ **May need it** |

---

## Why Video Extensions Need Custom Flush

**heygen_avatar_python & generic_video_python custom flush:**
```python
if cmd_name == "flush":
    await self._handle_interrupt()  # Cancel pending operations, clear buffers
    await ten_env.send_cmd(Cmd.create("flush"))  # Forward downstream
```

**What _handle_interrupt() does:**
1. Cancels in-flight HTTP requests to HeyGen/video APIs
2. Clears audio/video frame buffers
3. Resets connection state
4. Prevents stale data from being processed

**Without this:**
- Old audio would continue playing after user interrupts
- Video frames would queue up and cause delays
- System wouldn't properly reset between conversation turns

---

## Decision: Keep Custom Flush ✅

**Reason:** Extensions inheriting from `AsyncExtension` (not specialized bases) MUST implement flush handling if they:
1. Maintain internal buffers
2. Have async operations that need cancellation
3. Forward data downstream in the graph

**When PR comment applies:**
- Only when creating TTS extensions that inherit from `AsyncTTSBaseExtension`
- Then you get flush handling for free
- Don't add duplicate `on_cmd` for flush

**When you need custom flush:**
- Extensions inheriting directly from `AsyncExtension`
- Any extension with state that needs clearing on interrupt
- Video, avatar, or other non-standard extensions

---

## Recommendation

✅ **NO CHANGES NEEDED** - our custom flush handling is correct and necessary.
