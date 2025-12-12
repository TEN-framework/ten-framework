# Voice Assistant (with PowerMem)

A voice assistant enhanced with [PowerMem](https://github.com/oceanbase/powermem/) memory management capabilities for persistent conversation context.

## 📋 Table of Contents

- [Prerequisites](#prerequisites)
- [Quick Start (Docker)](#quick-start-docker---recommended)
- [Local Development](#local-development)
- [Configuration Guide](#configuration-guide)
- [Verification](#verification)
- [Troubleshooting](#troubleshooting)

## Prerequisites

Before you begin, ensure you have the following:

### System Requirements

- **Docker** (version 20.10+) and **Docker Compose** (version 2.0+)
  - Check: `docker --version` and `docker-compose --version`
- **Git** for cloning the repository
- At least **4GB RAM** available for containers

### API Accounts & Keys

You'll need to create accounts and obtain API keys from the following services:

| Service | Purpose | Where to Get |
|---------|---------|--------------|
| **Agora** | Real-time communication | [Agora Console](https://console.agora.io/) |
| **Deepgram** | Speech-to-text (ASR) | [Deepgram Console](https://console.deepgram.com/) |
| **OpenAI** | Language model (LLM) | [OpenAI Platform](https://platform.openai.com/) |
| **ElevenLabs** | Text-to-speech (TTS) | [ElevenLabs](https://elevenlabs.io/) |
| **Qwen** (or alternative) | PowerMem LLM & Embedding | [DashScope](https://dashscope.aliyun.com/) |

> 💡 **Tip**: Keep your API keys secure. Never commit them to version control.

## Quick Start (Docker - Recommended)

This is the easiest way to get started. Docker will handle all dependencies automatically.

### Step 1: Configure Environment Variables

1. Navigate to the project root:
   ```bash
   cd /path/to/ten-framework/ai_agents
   ```

2. Edit the `.env` file (create it if it doesn't exist):
   ```bash
   # The .env file is located at: ai_agents/.env
   # You can copy from .env.example if available
   ```

3. Configure the following **required** variables:

   **Voice Assistant Services:**
   ```bash
   # Agora - Real-time Communication
   AGORA_APP_ID=your_agora_app_id
   AGORA_APP_CERTIFICATE=your_agora_certificate  # Optional but recommended

   # Deepgram - Speech Recognition
   DEEPGRAM_API_KEY=your_deepgram_api_key

   # OpenAI - Language Model
   OPENAI_API_KEY=your_openai_api_key
   OPENAI_MODEL=gpt-4  # Optional, defaults to configured model

   # ElevenLabs - Text-to-Speech
   ELEVENLABS_TTS_KEY=your_elevenlabs_api_key
   ```

   **PowerMem Configuration:**
   ```bash
   # Timezone
   TIMEZONE=Asia/Shanghai  # Adjust to your timezone

   # Database (OceanBase is automatically started by docker-compose)
   DATABASE_PROVIDER=oceanbase
   OCEANBASE_HOST=seekdb  # Use 'seekdb' for Docker, '127.0.0.1' for local
   OCEANBASE_PORT=2881
   OCEANBASE_USER=root
   OCEANBASE_PASSWORD=  # Leave empty for SeekDB default
   OCEANBASE_DATABASE=powermem
   OCEANBASE_COLLECTION=memories

   # LLM Provider (for PowerMem)
   LLM_PROVIDER=qwen  # Options: qwen, openai, siliconflow
   LLM_API_KEY=your_qwen_api_key
   LLM_MODEL=qwen-plus
   LLM_BASE_URL=https://dashscope.aliyuncs.com/api/v1

   # Embedding Provider (for PowerMem)
   EMBEDDING_PROVIDER=qwen  # Options: qwen, openai, mock
   EMBEDDING_API_KEY=your_qwen_api_key  # Can be same as LLM_API_KEY
   EMBEDDING_MODEL=text-embedding-v4
   EMBEDDING_DIMS=1536
   EMBEDDING_BASE_URL=https://dashscope.aliyuncs.com/api/v1
   ```

   **Optional Variables:**
   ```bash
   OPENAI_PROXY_URL=  # Optional: Proxy for OpenAI API
   WEATHERAPI_API_KEY=  # Optional: For weather tool functionality
   ```

### Step 2: Start the Services

1. Navigate to the example directory:
   ```bash
   cd ai_agents/agents/examples/voice-assistant-with-PowerMem
   ```

2. Start all services with Docker Compose:
   ```bash
   docker-compose up -d
   ```

   This will:
   - Build the voice assistant container
   - Start OceanBase (SeekDB) database container
   - Initialize PowerMem database schema
   - Start the API server and frontend

3. Check container status:
   ```bash
   docker-compose ps
   ```

   You should see two containers running:
   - `voice-assistant-with-powermem` (main application)
   - `seekdb` (OceanBase database)

### Step 3: Verify Deployment

1. **Check logs** to ensure everything started correctly:
   ```bash
   docker-compose logs -f voice-assistant
   ```

   Look for:
   - ✅ "Server started" or similar success messages
   - ✅ No critical errors

2. **Access the application:**
   - **Frontend UI**: http://localhost:3000
   - **API Server**: http://localhost:8080

3. **Test the API** (optional):
   ```bash
   curl http://localhost:8080/ping
   ```

### Step 4: Stop Services (when needed)

```bash
docker-compose down
```

To also remove volumes (database data):
```bash
docker-compose down -v
```

## Local Development

If you prefer to run without Docker, follow these steps:

### Step 1: Install Dependencies

1. Ensure you have:
   - Python 3.8+
   - Node.js 20+
   - Go 1.21+
   - [Task](https://taskfile.dev/) build tool

2. Install project dependencies:
   ```bash
   cd ai_agents/agents/examples/voice-assistant-with-PowerMem
   task install
   ```

### Step 2: Configure Environment

1. Edit `.env` file at `ai_agents/.env` (same as Docker setup)

2. **Important**: For local development, update OceanBase connection:
   ```bash
   OCEANBASE_HOST=127.0.0.1  # Use localhost instead of 'seekdb'
   ```

3. **Start OceanBase locally** (if not using Docker):
   ```bash
   # Option 1: Use Docker just for database
   docker run -d --name seekdb \
     -p 2881:2881 -p 2886:2886 \
     -v $(pwd)/data:/var/lib/oceanbase \
     oceanbase/seekdb:latest

   # Option 2: Install OceanBase locally (see OceanBase docs)
   ```

### Step 3: Run the Application

```bash
task run
```

This starts:
- **API Server**: http://localhost:8080
- **Frontend**: http://localhost:3000
- **TMAN Designer**: http://localhost:49483 (for graph visualization)

## Configuration Guide

### Environment Variables Reference

#### Voice Assistant Services

| Variable | Required | Description | Example |
|----------|----------|-------------|---------|
| `AGORA_APP_ID` | ✅ Yes | Agora App ID for RTC | `abc123def456` |
| `AGORA_APP_CERTIFICATE` | ⚠️ Optional | Agora certificate (recommended for production) | `cert123...` |
| `DEEPGRAM_API_KEY` | ✅ Yes | Deepgram API key for ASR | `key_abc123...` |
| `OPENAI_API_KEY` | ✅ Yes | OpenAI API key for LLM | `sk-...` |
| `OPENAI_MODEL` | ⚠️ Optional | OpenAI model name | `gpt-4` |
| `ELEVENLABS_TTS_KEY` | ✅ Yes | ElevenLabs API key for TTS | `abc123...` |

#### PowerMem Configuration

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

### Alternative LLM/Embedding Providers

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

## Verification

### 1. Check Container Health

```bash
# Check if containers are running
docker-compose ps

# Check logs for errors
docker-compose logs voice-assistant | tail -50
docker-compose logs seekdb | tail -50
```

### 2. Test API Endpoints

```bash
# Health check
curl http://localhost:8080/ping

# Should return a success response
```

### 3. Access Frontend

1. Open http://localhost:3000 in your browser
2. You should see the voice assistant interface
3. Try starting a conversation to verify end-to-end functionality

### 4. Verify Database Connection

```bash
# Check if OceanBase is accessible
docker exec -it seekdb bash
# Inside container:
mysql -h127.0.0.1 -P2881 -uroot -e "SHOW DATABASES;"
```

## Troubleshooting

### Common Issues

#### 1. Containers won't start

**Problem**: `docker-compose up` fails or containers exit immediately

**Solutions**:
- Check Docker is running: `docker ps`
- Check port conflicts: `netstat -tuln | grep -E '3000|8080|2881'`
- View detailed logs: `docker-compose logs`
- Ensure `.env` file exists and has all required variables

#### 2. "Connection refused" to database

**Problem**: Voice assistant can't connect to OceanBase

**Solutions**:
- Verify `OCEANBASE_HOST` is correct:
  - Docker: use `seekdb`
  - Local: use `127.0.0.1`
- Check SeekDB container is running: `docker-compose ps seekdb`
- Wait a few seconds after starting - database needs time to initialize
- Check database logs: `docker-compose logs seekdb`

#### 3. API keys not working

**Problem**: Authentication errors from API services

**Solutions**:
- Verify API keys are correct in `.env` file
- Check for extra spaces or quotes around values
- Ensure API keys have sufficient credits/quota
- Test API keys directly with service providers' documentation

#### 4. Frontend not loading

**Problem**: http://localhost:3000 shows error or blank page

**Solutions**:
- Check frontend container logs: `docker-compose logs voice-assistant | grep -i frontend`
- Verify port 3000 is not in use: `lsof -i :3000`
- Try accessing API directly: `curl http://localhost:8080/ping`
- Rebuild containers: `docker-compose up -d --build`

#### 5. PowerMem initialization errors

**Problem**: Memory features not working

**Solutions**:
- Verify all PowerMem environment variables are set
- Check database connection (see issue #2)
- Ensure LLM and Embedding providers are correctly configured
- Check PowerMem logs in container: `docker-compose logs voice-assistant | grep -i powermem`

#### 6. Port already in use

**Problem**: Error about ports 3000, 8080, or 2881 being in use

**Solutions**:
```bash
# Find process using the port
lsof -i :3000
lsof -i :8080
lsof -i :2881

# Kill the process or change ports in docker-compose.yml
```

### Getting Help

- Check logs: `docker-compose logs -f`
- Review [PowerMem documentation](https://github.com/oceanbase/powermem/)
- Check [TEN Framework documentation](../../../AGENTS.md)
- Verify your `.env` configuration matches the examples above

### Reset Everything

If you need to start fresh:

```bash
# Stop and remove containers
docker-compose down -v

# Remove any local data
rm -rf ./data

# Restart
docker-compose up -d
```
