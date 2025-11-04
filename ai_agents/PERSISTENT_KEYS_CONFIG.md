# Persistent API Keys Configuration

This file documents the location and configuration of API keys that persist across repository updates.

## Environment Variables Location

**Primary .env file**: `/home/ubuntu/ten-framework/ai_agents/.env`

This is the **ONLY** .env file used by the system. All environment variables must be defined here.

## Current API Keys

### HeyGen
```bash
HEYGEN_API_KEY=NGNkNmQ5YWM0MmFjNDgxYzgwODcyZTI1NjE2MTViZmYtMTczMzg2OTY0MQ==
```
Used by: `heygen_avatar_python` extension

### Thymia
```bash
THYMIA_API_KEY=0gDmM8synx3LxbUFjT7LTareeaXFRCAraWBa2rY5
```
Used by: `thymia_analyzer_python` extension

### Deepgram
```bash
DEEPGRAM_API_KEY=<key>
```
Used by: `deepgram_ws_asr_python` extension

### Agora
```bash
AGORA_APP_ID=<id>
AGORA_APP_CERTIFICATE=<cert>
```
Used by: Multiple extensions for RTC streaming

### OpenAI
```bash
OPENAI_API_KEY=<key>
OPENAI_MODEL=<model>
```
Used by: LLM extensions

### Other Services
```bash
CARTESIA_TTS_KEY=<key>
ELEVENLABS_TTS_KEY=<key>
ELEVENLABS_VOICE_ID=<id>
RIME_TTS_API_KEY=<key>
WEATHERAPI_API_KEY=<key>
```

## Important Notes

1. **Never commit API keys to git** - Add `.env` to `.gitignore`
2. **After updating .env**: Restart the Docker container or source the file and restart the API server
3. **Property.json pattern**: Use `${env:VAR_NAME|}` for optional keys with empty default
4. **Required keys pattern**: Use `${env:VAR_NAME}` for required keys that error if missing

## Updating Keys

To update a key:

```bash
# Edit the .env file
nano /home/ubuntu/ten-framework/ai_agents/.env

# Option 1: Restart container (slower but guaranteed)
cd /home/ubuntu/ten-framework/ai_agents
docker compose down && docker compose up -d

# Option 2: Restart API server only (faster)
docker exec -d ten_agent_dev bash -c "pkill -9 -f 'bin/api' && \
  set -a && source /app/.env && set +a && \
  cd /app/server && ./bin/api -tenapp_dir=/app/agents/examples/voice-assistant-advanced/tenapp > /tmp/task_run.log 2>&1"
```

## Security Best Practices

- Store production keys in a secure password manager
- Rotate keys regularly
- Use different keys for development and production
- Monitor API usage for unauthorized access
- Restrict API key permissions to minimum required scope
