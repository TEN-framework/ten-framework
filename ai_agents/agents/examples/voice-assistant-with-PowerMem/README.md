# Voice Assistant (with PowerMem)

A voice assistant enhanced with [PowerMem](https://github.com/oceanbase/powermem/) memory management capabilities for persistent conversation context.

## PowerMem Configuration

Set the environment variable into `.env` file:
```bash
# Database
DATABASE_PROVIDER=oceanbase
OCEANBASE_HOST=127.0.0.1
OCEANBASE_PORT=2881
OCEANBASE_USER=root
OCEANBASE_PASSWORD=password
OCEANBASE_DATABASE=oceanbase
OCEANBASE_COLLECTION=memories

# LLM Provider (for PowerMem)
LLM_PROVIDER=qwen
LLM_API_KEY=your_qwen_api_key
LLM_MODEL=qwen-plus

# Embedding Provider (for PowerMem)
EMBEDDING_PROVIDER=qwen
EMBEDDING_API_KEY=your_qwen_api_key
EMBEDDING_MODEL=text-embedding-v4
EMBEDDING_DIMS=1536
```

## Quick Start

1. **Start seekdb server**
   ```bash
   docker run -d \
      --name seekdb \
      -p 2881:2881 \
      -p 2886:2886 \
      -v ./data:/var/lib/oceanbase \
      -e SEEKDB_DATABASE=powermem \
      -e ROOT_PASSWORD=password \
      oceanbase/seekdb:latest
   ```

2. **Install dependencies:**
   ```bash
   task install
   ```

3. **Run the voice assistant with MemU:**
   ```bash
   task run
   ```

4. **Access the application:**
   - Frontend: http://localhost:3000
   - API Server: http://localhost:8080
   - TMAN Designer: http://localhost:49483
