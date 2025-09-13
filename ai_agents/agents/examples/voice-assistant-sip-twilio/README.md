# Twilio Voice Assistant Example

This example demonstrates how to create a FastAPI server that can make outbound calls with Twilio and handle real-time audio streaming via WebSocket.

## Architecture

This example consists of:

1. **FastAPI Server** (`server.py`) - HTTP API for managing call sessions and WebSocket for audio streaming
2. **Twilio Integration** - Direct Twilio API calls for outbound calling
3. **WebSocket Streaming** - Real-time audio data handling

## Setup

1. Install dependencies:
```bash
pip install -r requirements.txt
```

2. Set environment variables:
```bash
export TWILIO_ACCOUNT_SID="your_account_sid"
export TWILIO_AUTH_TOKEN="your_auth_token"
export TWILIO_FROM_NUMBER="+1234567890"
export TWILIO_WEBHOOK_URL="your-domain.com"
```

3. Start the server:
```bash
python server.py
```

## API Endpoints

### POST /start_call
Start a new call session:
```json
{
  "phone_number": "+1234567890",
  "message": "Hello, this is a test call from the AI assistant."
}
```

### POST /stop_call
Stop the current call session.

### GET /health
Health check endpoint.

## How it Works

1. **HTTP Request** → FastAPI server receives `/start_call` request
2. **Call Initiation** → Server uses Twilio API to make outbound call
3. **WebSocket Connection** → Twilio connects to WebSocket for audio streaming
4. **Audio Processing** → Real-time audio data handling (ready for integration with ASR/TTS)

## Configuration

The server requires the following environment variables:

- **TWILIO_ACCOUNT_SID**: Your Twilio Account SID
- **TWILIO_AUTH_TOKEN**: Your Twilio Auth Token
- **TWILIO_FROM_NUMBER**: Your verified Twilio phone number
- **TWILIO_WEBHOOK_URL**: Your public webhook URL for Twilio WebSocket connections

## Requirements

- Python 3.8+
- Twilio account with Voice API access
- Public webhook URL (for WebSocket connections)
- FastAPI and Twilio Python SDK
