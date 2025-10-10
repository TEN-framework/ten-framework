# Agent Demo

A comprehensive demo showcasing various AI agent capabilities with multiple LLM providers and voice processing features. The deployed version can be accessed via [here](https://agent.theten.ai)

## Quick Start

1. **Install dependencies:**
   ```bash
   task install
   ```

2. **Run the demo:**
   ```bash
   task run
   ```

3. **Access the application:**
   - Frontend: http://localhost:3000
   - API Server: http://localhost:8080
   - TMAN Designer: http://localhost:49483

## Available Tasks

- `task install` - Install all dependencies
- `task run` - Start all services

---

## Release as Docker image

### Build image
```bash
# Run at project root
docker build -f agents/examples/demo/Dockerfile -t demo-app .
```

### Run container
```bash
# Use local .env (optional)
docker run --rm -it \
  --env-file .env \
  -p 8080:8080 \
  -p 3000:3000 \
  demo-app
```

### Access
- Frontend: http://localhost:3000
- API Server: http://localhost:8080