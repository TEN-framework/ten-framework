# Simple Voice Assistant Usage Guide

## Overview

This is a simplified voice assistant system that supports both inbound and outbound calls using Twilio. The system provides minimal viable features for voice interactions.

## Core Features

### 1. Inbound Calls
- Automatically handles incoming calls
- Real-time speech recognition
- AI-powered responses
- Text-to-speech output

### 2. Outbound Calls
- RESTful API to start/stop outbound calls
- Direct API calls to manage calls
- Real-time conversation with called parties

## Quick Start

### Configuration
Set these environment variables:

```bash
export TWILIO_ACCOUNT_SID="your_account_sid"
export TWILIO_AUTH_TOKEN="your_auth_token"
export TWILIO_FROM_NUMBER="+1234567890"
export TWILIO_WEBHOOK_URL="your-domain.com"
```

### Starting a Call via API
```bash
curl -X POST http://localhost:9000/api/calls \
  -H "Content-Type: application/json" \
  -d '{
    "phone_number": "+1234567890",
    "message": "Hello from AI assistant!"
  }'
```

### Getting Call Information
```bash
curl http://localhost:9000/api/calls/CAxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

### Listing All Calls
```bash
curl http://localhost:9000/api/calls
```

### Stopping a Call
```bash
curl -X DELETE http://localhost:9000/api/calls/CAxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

## API Endpoints

### RESTful API Endpoints
- `POST /api/calls` - Create new outbound call
- `GET /api/calls` - List all active calls
- `GET /api/calls/{call_sid}` - Get call information
- `DELETE /api/calls/{call_sid}` - Stop and delete call
- `POST /webhook/status` - Twilio status callback
- `GET /health` - Health check

## How It Works

1. **Inbound Calls**: Twilio forwards calls to your webhook, system handles automatically
2. **Outbound Calls**: Use RESTful API to initiate calls
3. **Audio Processing**: Real-time WebSocket streaming for bidirectional audio
4. **AI Integration**: Speech recognition → LLM processing → Text-to-speech

## Requirements

- Twilio account with phone number
- Public webhook URL
- TEN Framework runtime
- ASR, LLM, and TTS extensions

## Troubleshooting

### Common Issues
1. **Call not starting**: Check Twilio credentials and phone number format
2. **No audio**: Verify WebSocket connection and webhook URL
3. **AI not responding**: Check LLM and TTS configuration

### Health Check
```bash
curl http://localhost:9000/health
```

## Example Usage

### Python Integration
```python
import aiohttp

async def make_call(phone_number, message):
    async with aiohttp.ClientSession() as session:
        async with session.post(
            "http://localhost:9000/api/calls",
            json={
                "phone_number": phone_number,
                "message": message
            }
        ) as response:
            return await response.json()

async def stop_call(call_sid):
    async with aiohttp.ClientSession() as session:
        async with session.delete(
            f"http://localhost:9000/api/calls/{call_sid}"
        ) as response:
            return await response.json()

async def list_calls():
    async with aiohttp.ClientSession() as session:
        async with session.get(
            "http://localhost:9000/api/calls"
        ) as response:
            return await response.json()

# Usage
result = await make_call("+1234567890", "Hello!")
print(f"Call started: {result['call_sid']}")

# List all calls
calls = await list_calls()
print(f"Active calls: {calls['total']}")

# Stop a call
await stop_call(result['call_sid'])
```

This simplified system provides the essential functionality for voice-based AI interactions without unnecessary complexity.
