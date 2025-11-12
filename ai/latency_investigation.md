# Latency Investigation & Groq Integration Plan

**Date:** 2025-11-12
**Branch:** feat/deepgram-v2

---

## Current Setup

**LLM:** OpenAI gpt-4o (500-2000ms)
**STT:** Deepgram flux-general-en (2000ms EOT timeout)
**TTS:** ElevenLabs eleven_multilingual_v2
**Tools:** None configured ✓

---

## Latency Sources

1. **LLM Response:** 500-2000ms (gpt-4o)
2. **STT EOT Timeout:** 2000ms after speech stops
3. **Network Roundtrips:** STT → LLM → TTS
4. **TTS First Chunk:** 200-800ms

**Total Typical Latency:** 3-5 seconds

---

## Groq Integration (Recommended)

### Why Groq?
- **10-50x faster** than OpenAI (100-500ms response time)
- **OpenAI-compatible API** - works with existing `openai_llm2_python`
- **No code changes needed** - just config update

### Recommended Models

| Model | Latency | Quality | Use Case |
|-------|---------|---------|----------|
| `llama-3.1-8b-instant` | 100-300ms | Good | Maximum speed |
| `llama-3.3-70b-versatile` | 200-500ms | Excellent | **Recommended** |
| `llama-3.1-70b-versatile` | 300-800ms | Excellent | Quality priority |

### Implementation Steps

1. **Add to .env:**
   ```bash
   GROQ_API_KEY=your_groq_key_here
   ```

2. **Update one graph in property.json:**
   ```json
   {
     "name": "llm",
     "addon": "openai_llm2_python",
     "property": {
       "base_url": "https://api.groq.com/openai/v1",
       "api_key": "${env:GROQ_API_KEY}",
       "model": "llama-3.3-70b-versatile",
       "max_tokens": 512,
       "temperature": 0.7,
       "prompt": "You are a voice assistant. Keep responses under 20 words."
     }
   }
   ```

3. **Test graph:** `flux_apollo_groq` or similar

4. **Expected improvement:** 1.5-3 seconds total latency (vs 3-5s)

---

## Security Issue - RESOLVED ✓

**Issue:** `PERSISTENT_KEYS_CONFIG.md` contained real API keys in git history

**Resolution:**
1. ✓ File removed from working directory
2. ✓ File removed from git index
3. ✓ Added to `.gitignore`
4. ✓ Pre-commit hook installed to prevent recurrence
5. ⚠️ **Action Required:** Remove from git history (see below)

**Git History Cleanup:**
```bash
# WARNING: Rewrites history - coordinate with team first
git filter-branch --force --index-filter \
  'git rm --cached --ignore-unmatch ai_agents/PERSISTENT_KEYS_CONFIG.md' \
  --prune-empty --tag-name-filter cat -- --all

# Force push (dangerous - confirm with team)
# git push origin --force --all
# git push origin --force --tags
```

**Affected Keys (ROTATE IMMEDIATELY):**
- HEYGEN_API_KEY
- THYMIA_API_KEY

---

## PR #1691 Review Notes

### Flush Handling
**Finding:** Extensions shouldn't implement custom flush handlers

**Why:** `ten_ai_base` AsyncTTS2BaseExtension handles flush automatically

**Action:** Verify no custom `on_cmd("flush")` in TTS extensions

---

## Next Steps

1. [ ] Add Groq API key to `.env`
2. [ ] Create test graph with Groq LLM
3. [ ] Test latency improvement
4. [ ] Coordinate git history cleanup with team
5. [ ] Rotate exposed API keys
