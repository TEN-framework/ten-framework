# OpenAI TTS Proxy Server

A simple HTTP proxy server that forwards requests from TTS Client to OpenAI API and streams responses back.

## Features

- **Transparent Proxy**: Forwards all requests and responses between TTS Client and OpenAI API
- **Streaming Support**: Efficiently streams audio data without buffering
- **Header Forwarding**: Preserves all request headers, especially Authorization
- **Connection Pooling**: Uses httpx with connection pooling for better performance
- **Health Check**: Provides `/health` endpoint for monitoring

## Usage

### Start the Proxy Server

```bash
cd proxy
pip install -r requirements.txt
python proxy_server.py
```

Or with environment variables:

```bash
PROXY_HOST=0.0.0.0 PROXY_PORT=8081 OPENAI_BASE_URL=https://api.openai.com/v1 python proxy_server.py
```

### Environment Variables

- `PROXY_HOST`: Host to bind to (default: `0.0.0.0`)
- `PROXY_PORT`: Port to listen on (default: `8081`)
- `OPENAI_BASE_URL`: OpenAI API base URL (default: `https://api.openai.com/v1`)
- `OPENAI_API_KEY`: OpenAI API key (optional, can be provided via Authorization header)

The proxy will forward requests to OpenAI using the API key from either:
1. The `Authorization` header in the request (preferred)
2. The `OPENAI_API_KEY` environment variable (fallback)

## API Endpoints

- `POST /audio/speech`: Proxy endpoint that forwards to OpenAI `/audio/speech`
- `GET /health`: Health check endpoint

## Docker Usage

When running in a container with TTS Client:

1. Start the proxy server as a background process or separate service
2. Configure TTS Client to send request to `http://localhost:8081` 
3. Both services will run in the same container and communicate via localhost
