# Phase 2 Findings: Direct Testing

## MAJOR BREAKTHROUGH ✅

### Test 2.1: tman with --verbose flag
**Result**: FAILED
```
error: unexpected argument '--verbose' found
tip: to pass '--verbose' as a value, use '-- --verbose'
```
**Conclusion**: --verbose flag needs to be passed differently (after --), not useful for this purpose

---

### Test 2.2: TEN_LOG_FORMATTER Environment Variable
**Result**: ✅ **SUCCESS - LOGS APPEAR!**

**Command:**
```bash
export TEN_LOG_FORMATTER=json
tman run start
```

**Output:**
```
Successfully registered addon 'weatherapi_tool_python'
Successfully registered addon 'thymia_analyzer_python'
[THYMIA_INIT] Extension initializing at 1761813547.478739
[THYMIA_INIT_STDERR] Extension initializing at 1761813547.4787538
Successfully registered addon 'deepgram_ws_asr_python'
Successfully registered addon 'openai_llm2_python'
Successfully registered addon 'rime_tts'
Successfully registered addon 'main_python'
Successfully registered addon 'message_collector2'
Successfully registered addon 'streamid_adapter'
KEYPOINT _process_websocket: wss://users.rime.ai/ws2?...
[THYMIA_ON_START] on_start called at 1761813547.888729
```

**Conclusion**:
- **Python stdout logs appear!** ([THYMIA_INIT])
- **Python stderr logs appear!** ([THYMIA_INIT_STDERR])
- **Lifecycle logs appear!** ([THYMIA_ON_START])
- **All extensions register successfully**
- TEN_LOG_FORMATTER can be set to `json` or `text` (both work)

---

### Test 2.3: TEN_ENABLE_BACKTRACE_DUMP
**Result**: ✅ Logs also appear with this variable

**Conclusion**: Setting TEN_ENABLE_BACKTRACE_DUMP=1 also enables logging output

---

### Test 2.4: bin/main direct execution
**Result**: ❌ FAILS
```
Traceback (most recent call last):
  File ".../weatherapi_tool_python/addon.py", line 19, in on_create_instance
    from .extension import WeatherToolExtension
  File ".../weatherapi_tool_python/extension.py", line 18, in <module>
    from ten_ai_base.config import BaseConfig
...
timeout: the monitored command dumped core
```

**Conclusion**: bin/main requires proper environment setup via start.sh or tman. Always use `tman run start`.

---

## ROOT CAUSE IDENTIFIED

**The TEN Framework suppresses stdout/stderr output by default.**

**Solution**: Set environment variable before running worker:
```bash
export TEN_LOG_FORMATTER=json  # or "text"
# OR
export TEN_ENABLE_BACKTRACE_DUMP=1
```

---

## Next Steps

1. ✅ Add TEN_LOG_FORMATTER to .env file
2. ✅ Update docker-compose.yml to pass this variable
3. ✅ Restart server and verify logs appear
4. ✅ Update documentation with proper logging instructions
