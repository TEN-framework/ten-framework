# Voice Assistant SIP Twilio Server

Standalone Twilio server application migrated from `ten_packages/extension/main_python`.

## Features

- FastAPI server providing REST API
- Support for outbound calls, receiving calls, and status callbacks
- **Note**: WebSocket functionality and audio processing are handled by the main_python extension

## Installation and Running

### 1. Install Dependencies

```bash
# Run in project root directory
tman run build
```

### 2. Configure Environment Variables

Set the following environment variables:

```bash
export TWILIO_ACCOUNT_SID="your_twilio_account_sid"
export TWILIO_AUTH_TOKEN="your_twilio_auth_token"
export TWILIO_FROM_NUMBER="+1234567890"
export TWILIO_HTTP_PORT="8000"
export TWILIO_GREETING="Hello, I am your AI assistant."
```

### 3. Start Server

```bash
# Run using tman
tman run twilio-server

# Or run directly
python3 server/run_server.py
```

## API Endpoints

### HTTP API (Port 8000)

- `POST /api/calls` - Create outbound call
- `GET /api/calls` - List all calls
- `GET /api/calls/{call_sid}` - Get call information
- `DELETE /api/calls/{call_sid}` - Stop call
- `POST /webhook/status` - Twilio status callback
- `GET /health` - Health check

### WebSocket and Audio Processing

WebSocket functionality and audio processing are handled by the `main_python` extension in `ten_packages/extension/main_python/`. This server only provides HTTP API endpoints for call management.

## Project Structure

```
server/
├── __init__.py          # Package initialization
├── main.py              # Main entry file
├── config.py            # Configuration model
├── twilio_server.py     # Twilio server logic
├── run_server.py        # Startup script
├── requirements.txt     # Python dependencies
└── README.md           # Documentation
```

## Differences from Original Code

1. **Standalone Operation**: Completely independent, no external framework dependencies
2. **Configuration Management**: Uses environment variables for configuration
3. **Logging System**: Uses Python standard logging library
4. **Modular Design**: Cleaner code structure, easier to maintain
5. **Separation of Concerns**: WebSocket and audio processing remain in the main_python extension

## Notes

- Ensure Twilio account configuration is correct
- Ensure firewall allows access to relevant ports
- WebSocket functionality is handled by the main_python extension
- This server only provides HTTP API endpoints for call management
