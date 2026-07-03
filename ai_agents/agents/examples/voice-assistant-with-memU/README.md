# Voice Assistant (with MemU)

A voice assistant enhanced with MemU memory management capabilities for persistent conversation context.


## MemU Configuration

### Getting Started with MemU

- **Official Website:** https://memu.pro/
- **Quick Trial:** You can quickly experience MemU using the Cloud Version
- **API Key Setup:**
  1. Complete the registration process to obtain your API key
  2. Set the API key as an environment variable:
     ```bash
     export MEMU_API_KEY="your_memu_api_key_here"
     ```

### Memory Retrieval Methods

For detailed documentation, visit: https://memu.pro/docs#retrieve-memory

The following two retrieval methods are currently implemented:
1. **Default Categories Retrieval**
2. **Related Categories Retrieval**

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
