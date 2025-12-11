# Voice Assistant (with PowerMem)

A voice assistant enhanced with [PowerMem](https://github.com/oceanbase/powermem/) memory management capabilities for persistent conversation context.

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

# For a complete list of timezones, see: https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
TIMEZONE=Asia/Shanghai

# =============================================================================
# 1. Database Configuration (Required)
# =============================================================================
# Choose your database provider: sqlite, oceanbase, postgres
DATABASE_PROVIDER=sqlite

# -----------------------------------------------------------------------------
# SQLite Configuration (Default - Recommended for development)
# -----------------------------------------------------------------------------
SQLITE_PATH=./data/powermem_dev.db
SQLITE_ENABLE_WAL=true
SQLITE_TIMEOUT=30
SQLITE_COLLECTION=memories

# -----------------------------------------------------------------------------
# OceanBase Configuration
# -----------------------------------------------------------------------------
OCEANBASE_HOST=127.0.0.1
OCEANBASE_PORT=2881
OCEANBASE_USER=root@sys
OCEANBASE_PASSWORD=password
OCEANBASE_DATABASE=powermem
OCEANBASE_COLLECTION=memories

## Keep the default settings, as modifications are generally not needed.
OCEANBASE_INDEX_TYPE=IVF_FLAT
OCEANBASE_VECTOR_METRIC_TYPE=cosine
OCEANBASE_TEXT_FIELD=document
OCEANBASE_VECTOR_FIELD=embedding
OCEANBASE_EMBEDDING_MODEL_DIMS=1536
OCEANBASE_PRIMARY_FIELD=id
OCEANBASE_METADATA_FIELD=metadata
OCEANBASE_VIDX_NAME=memories_vidx

# -----------------------------------------------------------------------------
# PostgreSQL Configuration
# -----------------------------------------------------------------------------
POSTGRES_HOST=127.0.0.1
POSTGRES_PORT=5432
POSTGRES_USER=postgres
POSTGRES_PASSWORD=password
POSTGRES_DATABASE=powermem
POSTGRES_COLLECTION=memories

## Keep the default settings, as modifications are generally not needed.
POSTGRES_EMBEDDING_MODEL_DIMS=1536
POSTGRES_DISKANN=true
POSTGRES_HNSW=true
# DATABASE_SSLMODE=prefer
# DATABASE_POOL_SIZE=10
# DATABASE_MAX_OVERFLOW=20


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


# =============================================================================
# 11. Graph Store Configuration (Optional)
# =============================================================================
# Graph store for knowledge graph storage and retrieval
# Enable graph store functionality
GRAPH_STORE_ENABLED=false

# Graph store provider (currently supports: oceanbase)
GRAPH_STORE_PROVIDER=oceanbase

# OceanBase Graph Configuration
GRAPH_STORE_HOST=127.0.0.1
GRAPH_STORE_PORT=2881
GRAPH_STORE_USER=root@sys
GRAPH_STORE_PASSWORD=password
GRAPH_STORE_DB_NAME=powermem

# Optional: Graph traversal settings
GRAPH_STORE_MAX_HOPS=3

# Optional: Graph store vector and index settings
# GRAPH_STORE_VECTOR_METRIC_TYPE=l2
# GRAPH_STORE_INDEX_TYPE=HNSW

# Optional: Custom prompts for graph operations
# GRAPH_STORE_CUSTOM_PROMPT=
# GRAPH_STORE_CUSTOM_EXTRACT_RELATIONS_PROMPT=
# GRAPH_STORE_CUSTOM_UPDATE_GRAPH_PROMPT=
# GRAPH_STORE_CUSTOM_DELETE_RELATIONS_PROMPT=
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
