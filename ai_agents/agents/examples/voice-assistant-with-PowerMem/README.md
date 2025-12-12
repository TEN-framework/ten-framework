# Voice Assistant (with PowerMem)

A voice assistant enhanced with [PowerMem](https://github.com/oceanbase/powermem/) memory management capabilities for persistent conversation context.

## Configuration

### Required Environment Variables

1. **Agora Account**: Get credentials from [Agora Console](https://console.agora.io/)
   - `AGORA_APP_ID` - Your Agora App ID (required)

2. **Deepgram Account**: Get credentials from [Deepgram Console](https://console.deepgram.com/)
   - `DEEPGRAM_API_KEY` - Your Deepgram API key (required)

3. **OpenAI Account**: Get credentials from [OpenAI Platform](https://platform.openai.com/)
   - `OPENAI_API_KEY` - Your OpenAI API key (required)

4. **ElevenLabs Account**: Get credentials from [ElevenLabs](https://elevenlabs.io/)
   - `ELEVENLABS_TTS_KEY` - Your ElevenLabs API key (required)

### Optional Environment Variables

- `AGORA_APP_CERTIFICATE` - Agora App Certificate (optional)
- `OPENAI_MODEL` - OpenAI model name (optional, defaults to configured model)
- `OPENAI_PROXY_URL` - Proxy URL for OpenAI API (optional)
- `WEATHERAPI_API_KEY` - Weather API key for weather tool (optional)

### PowerMem Minimal Configuration:

```bash
# =============================================================================
# PowerMem Configuration Template
# =============================================================================
# Copy this file to .env and modify the values according to your needs
#
# Required Configuration: Database, LLM, Embedding
# Optional Configuration: Agent, Intelligent Memory, Performance, Security, etc.
# =============================================================================

# For a complete list of timezones, see: https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
TIMEZONE=Asia/Shanghai

# =============================================================================
# 1. Database Configuration (Required)
# =============================================================================
# Choose your database provider: sqlite, oceanbase, postgres
DATABASE_PROVIDER=oceanbase

# -----------------------------------------------------------------------------
# OceanBase Configuration
# -----------------------------------------------------------------------------
OCEANBASE_HOST=127.0.0.1
OCEANBASE_PORT=2881
OCEANBASE_USER=root@sys
OCEANBASE_PASSWORD=password
OCEANBASE_DATABASE=powermem
OCEANBASE_COLLECTION=memories

# =============================================================================
# 2. LLM Configuration (Required)
# =============================================================================
# Choose your LLM provider: qwen, openai, siliconflow
LLM_PROVIDER=qwen

# -----------------------------------------------------------------------------
# Qwen Configuration (Default)
# -----------------------------------------------------------------------------
LLM_API_KEY=your_api_key_here
LLM_MODEL=qwen-plus
LLM_BASE_URL=https://dashscope.aliyuncs.com/api/v1

# =============================================================================
# 3. Embedding Configuration (Required)
# =============================================================================
# Choose your embedding provider: qwen, openai, mock
EMBEDDING_PROVIDER=qwen

# -----------------------------------------------------------------------------
# Qwen Embedding Configuration (Default)
# -----------------------------------------------------------------------------
EMBEDDING_API_KEY=your_api_key_here
EMBEDDING_MODEL=text-embedding-v4
EMBEDDING_DIMS=1536
EMBEDDING_BASE_URL=https://dashscope.aliyuncs.com/api/v1
```

## Quick Start

1. **Install dependencies:**
   ```bash
   task install
   ```

2. **Run the voice assistant with MemU:**
   ```bash
   task run
   ```

3. **Access the application:**
   - Frontend: http://localhost:3000
   - API Server: http://localhost:8080
   - TMAN Designer: http://localhost:49483
