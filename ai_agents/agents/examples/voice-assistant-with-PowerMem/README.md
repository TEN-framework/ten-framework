# Voice Assistant (with PowerMem)

A voice assistant enhanced with PowerMem memory management capabilities for persistent conversation context.

## PowerMem Configuration

Set the environment variable:

```bash
# =============================================================================
# PowerMem Configuration Template
# =============================================================================
# Copy this file to .env and modify the values according to your needs
# 
# Required Configuration: Database, LLM, Embedding
# Optional Configuration: Agent, Intelligent Memory, Performance, Security, etc.
# =============================================================================


# =============================================================================
# 1. Database Configuration (Required)
# =============================================================================
# Choose your database provider: sqlite, oceanbase, postgres
DATABASE_PROVIDER=sqlite

# -----------------------------------------------------------------------------
# SQLite Configuration (Default - Recommended for development)
# -----------------------------------------------------------------------------
DATABASE_PATH=./data/powermem_dev.db
DATABASE_ENABLE_WAL=true
DATABASE_TIMEOUT=30

# -----------------------------------------------------------------------------
# OceanBase Configuration (Uncomment if using OceanBase)
# -----------------------------------------------------------------------------
# DATABASE_PROVIDER=oceanbase
# DATABASE_HOST=localhost
# DATABASE_PORT=2881
# DATABASE_USER=root
# DATABASE_PASSWORD=your_password
# DATABASE_NAME=powermem
# DATABASE_COLLECTION_NAME=memories

## Keep the default settings, as modifications are generally not needed.
# DATABASE_INDEX_TYPE=IVF_FLAT
# DATABASE_VECTOR_METRIC_TYPE=cosine
# DATABASE_TEXT_FIELD=document
# DATABASE_VECTOR_FIELD=embedding
# DATABASE_EMBEDDING_MODEL_DIMS=1536
# DATABASE_PRIMARY_FIELD=id
# DATABASE_METADATA_FIELD=metadata
# DATABASE_VIDX_NAME=memories_vidx

# -----------------------------------------------------------------------------
# PostgreSQL Configuration (Uncomment if using PostgreSQL)
# -----------------------------------------------------------------------------
# DATABASE_PROVIDER=postgres
# DATABASE_HOST=localhost
# DATABASE_PORT=5432
# DATABASE_USER=postgres
# DATABASE_PASSWORD=your_password
# DATABASE_NAME=powermem

## Keep the default settings, as modifications are generally not needed.
# DATABASE_SSLMODE=prefer
# DATABASE_POOL_SIZE=10
# DATABASE_MAX_OVERFLOW=20


# =============================================================================
# 2. LLM Configuration (Required)
# =============================================================================
# Choose your LLM provider: qwen, openai, mock
LLM_PROVIDER=qwen

# -----------------------------------------------------------------------------
# Qwen Configuration (Default)
# -----------------------------------------------------------------------------
LLM_API_KEY=your_api_key_here
LLM_MODEL=qwen-plus

## Keep the default settings, as modifications are generally not needed.
LLM_BASE_URL=https://dashscope.aliyuncs.com/api/v1
LLM_TEMPERATURE=0.7
LLM_MAX_TOKENS=1000
LLM_TOP_P=0.8
LLM_TOP_K=50
LLM_ENABLE_SEARCH=false

# -----------------------------------------------------------------------------
# OpenAI Configuration (Uncomment if using OpenAI)
# -----------------------------------------------------------------------------
# LLM_PROVIDER=openai
# LLM_API_KEY=your-openai-api-key

## Keep the default settings, as modifications are generally not needed.
# LLM_MODEL=gpt-4
# LLM_BASE_URL=https://api.openai.com/v1
# LLM_TEMPERATURE=0.7
# LLM_MAX_TOKENS=1000
# LLM_TOP_P=1.0


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

# -----------------------------------------------------------------------------
# OpenAI Embedding Configuration (Uncomment if using OpenAI)
# -----------------------------------------------------------------------------
# EMBEDDING_PROVIDER=openai
# EMBEDDING_API_KEY=your-openai-api-key
# EMBEDDING_MODEL=text-embedding-ada-002
# EMBEDDING_DIMS=1536
# EMBEDDING_BASE_URL=https://api.openai.com/v1



## 4-10. Keep the default settings, as modifications are generally not needed.
# =============================================================================
# 4. Agent Configuration (Optional)
# =============================================================================
# Agent memory management settings
AGENT_ENABLED=true
AGENT_DEFAULT_SCOPE=AGENT
AGENT_DEFAULT_PRIVACY_LEVEL=PRIVATE
AGENT_DEFAULT_COLLABORATION_LEVEL=READ_ONLY
AGENT_DEFAULT_ACCESS_PERMISSION=OWNER_ONLY

# Agent Memory Mode (auto, multi_agent, multi_user, hybrid)
AGENT_MEMORY_MODE=auto


# =============================================================================
# 5. Intelligent Memory Configuration (Optional)
# =============================================================================
# Ebbinghaus forgetting curve settings
INTELLIGENT_MEMORY_ENABLED=true
INTELLIGENT_MEMORY_INITIAL_RETENTION=1.0
INTELLIGENT_MEMORY_DECAY_RATE=0.1
INTELLIGENT_MEMORY_REINFORCEMENT_FACTOR=0.3
INTELLIGENT_MEMORY_WORKING_THRESHOLD=0.3
INTELLIGENT_MEMORY_SHORT_TERM_THRESHOLD=0.6
INTELLIGENT_MEMORY_LONG_TERM_THRESHOLD=0.8

# Memory decay calculation settings
MEMORY_DECAY_ENABLED=true
MEMORY_DECAY_ALGORITHM=ebbinghaus
MEMORY_DECAY_BASE_RETENTION=1.0
MEMORY_DECAY_FORGETTING_RATE=0.1
MEMORY_DECAY_REINFORCEMENT_FACTOR=0.3


# =============================================================================
# 6. Performance Configuration (Optional)
# =============================================================================
# Memory management settings
MEMORY_BATCH_SIZE=100
MEMORY_CACHE_SIZE=1000
MEMORY_CACHE_TTL=3600
MEMORY_SEARCH_LIMIT=10
MEMORY_SEARCH_THRESHOLD=0.7

# Vector store settings
VECTOR_STORE_BATCH_SIZE=50
VECTOR_STORE_CACHE_SIZE=500
VECTOR_STORE_INDEX_REBUILD_INTERVAL=86400


# =============================================================================
# 7. Security Configuration (Optional)
# =============================================================================
# Encryption settings
ENCRYPTION_ENABLED=false
ENCRYPTION_KEY=
ENCRYPTION_ALGORITHM=AES-256-GCM

# Access control settings
ACCESS_CONTROL_ENABLED=true
ACCESS_CONTROL_DEFAULT_PERMISSION=READ_ONLY
ACCESS_CONTROL_ADMIN_USERS=admin,root


# =============================================================================
# 8. Telemetry Configuration (Optional)
# =============================================================================
# Usage analytics and monitoring
TELEMETRY_ENABLED=false
TELEMETRY_ENDPOINT=https://telemetry.powermem.ai
TELEMETRY_API_KEY=
TELEMETRY_BATCH_SIZE=100
TELEMETRY_FLUSH_INTERVAL=30
TELEMETRY_RETENTION_DAYS=30


# =============================================================================
# 9. Audit Configuration (Optional)
# =============================================================================
# Audit logging settings
AUDIT_ENABLED=true
AUDIT_LOG_FILE=./logs/audit.log
AUDIT_LOG_LEVEL=INFO
AUDIT_RETENTION_DAYS=90
AUDIT_COMPRESS_LOGS=true
AUDIT_LOG_ROTATION_SIZE=100MB


# =============================================================================
# 10. Logging Configuration (Optional)
# =============================================================================
# General logging settings
LOGGING_LEVEL=DEBUG
LOGGING_FORMAT=%(asctime)s - %(name)s - %(levelname)s - %(message)s
LOGGING_FILE=./logs/powermem.log
LOGGING_MAX_SIZE=100MB
LOGGING_BACKUP_COUNT=5
LOGGING_COMPRESS_BACKUPS=true

# Console logging
LOGGING_CONSOLE_ENABLED=true
LOGGING_CONSOLE_LEVEL=INFO
LOGGING_CONSOLE_FORMAT=%(levelname)s - %(message)s
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
