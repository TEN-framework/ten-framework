# Voice Assistant (with MemOS)

A voice assistant enhanced with MemOS memory management capabilities for persistent conversation context.


## MemOS Configuration

### Getting Started with MemOS

- **Official Website:** https://memos-docs.openmem.net/
- **Quick Trial:** You can quickly experience MemOS using the Cloud Version
- **API Key Setup:**
  1. Complete the registration process to obtain your API key
  2. Set the API key as an environment variable:
     ```bash
     export MEMOS_API_KEY="your_memos_api_key_here"
     ```

### Memory Features

MemOS provides simple and powerful memory management:

- **addMessage**: Automatically stores conversation messages with user and conversation context
- **searchMemory**: Retrieves relevant memories based on user queries using semantic search

For detailed documentation, visit: https://memos-docs.openmem.net/cn/usecase/home_assistant

## Quick Start

1. **Install dependencies:**
   ```bash
   task install
   ```

2. **Run the voice assistant with MemOS:**
   ```bash
   task run
   ```

3. **Access the application:**
   - Frontend: http://localhost:3000
   - API Server: http://localhost:8080
   - TMAN Designer: http://localhost:49483
