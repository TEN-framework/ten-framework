# Deepgram TTS Python Extension

A TEN Framework extension for Deepgram Text-to-Speech with persistent WebSocket connections and robust reconnection logic.

## Features

- **Persistent WebSocket Connection**: Maintains a single connection throughout the extension lifecycle for optimal performance
- **Robust Reconnection Logic**: Handles network issues, server restarts, and connection timeouts with exponential backoff
- **Health Check System**: Periodic ping/pong validation to ensure connection health
- **Fallback Strategy**: Automatic fallback to REST API when WebSocket fails
- **Request Queuing**: Handles requests before WebSocket is ready
- **Production Ready**: Comprehensive error handling and logging

## Configuration

Set the following environment variable:
```bash
export DEEPGRAM_API_KEY="your_deepgram_api_key"
```

## Properties

- `api_key`: Deepgram API key (from environment variable)
- `model`: TTS model to use (default: "aura-luna-en")
- `voice`: Voice to use (default: "aura-luna-en")
- `encoding`: Audio encoding (default: "linear16")
- `sample_rate`: Audio sample rate (default: 24000)
- `container`: Audio container format (default: "none")
- `use_rest_fallback`: Enable REST API fallback (default: true)
- `websocket_timeout`: WebSocket connection timeout (default: 10.0)
- `reconnect_attempts`: Max reconnection attempts (default: 5)
- `reconnect_delay`: Base delay between reconnection attempts (default: 1.0)
- `keepalive_interval`: Health check frequency (default: 30.0)
- `max_request_retries`: Request-level retries (default: 2)
- `health_check_timeout`: Health check timeout (default: 5.0)

## Usage

This extension is compatible with the TEN Framework TTS2 interface and can be used as a drop-in replacement for other TTS extensions.
