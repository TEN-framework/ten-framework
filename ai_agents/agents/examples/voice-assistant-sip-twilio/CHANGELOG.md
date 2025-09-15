# Changelog - RESTful Voice Assistant API

## Version 1.3.0 - RESTful API Implementation

### New Features
- **RESTful API**: Standard REST endpoints for call management
- **CRUD Operations**: Create, Read, Update, Delete calls via API
- **Simplified Integration**: Direct API calls instead of tool integration

### Core Features
- **Inbound Calls**: Automatic handling of incoming calls
- **Outbound Calls**: RESTful API to start/stop calls
- **Real-time Audio**: WebSocket streaming for bidirectional audio
- **AI Integration**: Speech recognition → LLM → Text-to-speech pipeline

### RESTful API Endpoints
- `POST /api/calls` - Create new outbound call
- `GET /api/calls` - List all active calls
- `GET /api/calls/{call_sid}` - Get call information
- `DELETE /api/calls/{call_sid}` - Stop and delete call
- `POST /webhook/status` - Twilio status callback
- `GET /health` - Health check

### Documentation
- `SIMPLE_USAGE_GUIDE.md` - Simplified usage guide
- Essential API documentation
- Basic configuration requirements

## Technical Details

### Files Modified
- `server.py` - Simplified to essential endpoints only
- `extension.py` - Streamlined call handling and tool integration

### Dependencies
- No new dependencies required
- Uses existing Twilio and FastAPI infrastructure

### Backward Compatibility
- Core functionality preserved
- Simplified API surface
- Reduced complexity

## Usage Examples

### Create Call
```bash
curl -X POST http://localhost:9000/api/calls \
  -H "Content-Type: application/json" \
  -d '{"phone_number": "+1234567890", "message": "Hello!"}'
```

### List Calls
```bash
curl http://localhost:9000/api/calls
```

### Get Call Info
```bash
curl http://localhost:9000/api/calls/CAxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

### Stop Call
```bash
curl -X DELETE http://localhost:9000/api/calls/CAxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

## Requirements
- Twilio account with phone number
- Public webhook URL
- TEN Framework runtime

## Migration Notes
- Simplified implementation reduces complexity
- Core functionality preserved
- Easier to understand and maintain
