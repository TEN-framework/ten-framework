# Configuration Guide

## Environment Variables Reference

### Voice Assistant Services

| Variable | Required | Description | Example |
|----------|----------|-------------|---------|
| `AGORA_APP_ID` | ✅ Yes | Agora App ID for RTC | `abc123def456` |
| `AGORA_APP_CERTIFICATE` | ⚠️ Optional | Agora certificate (recommended for production) | `cert123...` |
| `DEEPGRAM_API_KEY` | ✅ Yes | Deepgram API key for ASR | `key_abc123...` |
| `OPENAI_API_KEY` | ✅ Yes | OpenAI API key for LLM | `sk-...` |
| `OPENAI_MODEL` | ⚠️ Optional | OpenAI model name | `gpt-4` |
| `ELEVENLABS_TTS_KEY` | ✅ Yes | ElevenLabs API key for TTS | `abc123...` |

### PowerMem Configuration

**Database:**
- `DATABASE_PROVIDER`: `oceanbase` (recommended), `sqlite`, or `postgres`
- `OCEANBASE_HOST`: `seekdb` (Docker) or `127.0.0.1` (local)
- `OCEANBASE_PORT`: `2881` (default)
- `OCEANBASE_USER`: `root` (default)
- `OCEANBASE_PASSWORD`: Leave empty for SeekDB default
- `OCEANBASE_DATABASE`: `powermem` (default)
- `OCEANBASE_COLLECTION`: `memories` (default)

**LLM Provider:**
- `LLM_PROVIDER`: `qwen` (recommended), `openai`, or `siliconflow`
- `LLM_API_KEY`: Your provider API key
- `LLM_MODEL`: Model name (e.g., `qwen-plus`, `gpt-4`)
- `LLM_BASE_URL`: API base URL

**Embedding Provider:**
- `EMBEDDING_PROVIDER`: `qwen` (recommended), `openai`, or `mock`
- `EMBEDDING_API_KEY`: Your provider API key
- `EMBEDDING_MODEL`: Model name (e.g., `text-embedding-v4`)
- `EMBEDDING_DIMS`: Embedding dimensions (e.g., `1536`)

## Alternative LLM/Embedding Providers

**Using OpenAI for PowerMem:**
```bash
LLM_PROVIDER=openai
LLM_API_KEY=${OPENAI_API_KEY}  # Reuse OpenAI key
LLM_MODEL=gpt-4
LLM_BASE_URL=https://api.openai.com/v1

EMBEDDING_PROVIDER=openai
EMBEDDING_API_KEY=${OPENAI_API_KEY}
EMBEDDING_MODEL=text-embedding-3-large
EMBEDDING_DIMS=3072
```
