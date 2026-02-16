# Main Python Extension for Telnyx

This extension handles Telnyx SIP call integration for the TEN Framework voice assistant.

## Features

- Inbound and outbound call handling via Telnyx
- Real-time audio streaming using WebSocket
- Audio format conversion (μ-law to PCM)
- Integration with TEN Framework for STT → LLM → TTS pipeline

## Configuration

The extension requires the following configuration parameters:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `greeting` | string | "Hello, I am your AI assistant." | Greeting message played when call connects |
| `telnyx_api_key` | string | "" | Telnyx API key (required) |
| `telnyx_connection_id` | string | "" | Telnyx Connection ID (required) |
| `telnyx_from_number` | string | "" | Telnyx phone number (required) |
| `telnyx_server_port` | int | 8000 | Server port for HTTP and WebSocket |
| `telnyx_public_server_url` | string | "" | Public server URL for webhooks and media streams |
| `telnyx_use_https` | bool | true | Use HTTPS for webhooks |
| `telnyx_use_wss` | bool | true | Use WSS for media streams |

## Usage

Configure the extension in your `property.json`:

```json
{
  "name": "main_control",
  "addon": "main_python",
  "property": {
    "greeting": "Hello, I am your AI assistant.",
    "telnyx_api_key": "${env:TELNYX_API_KEY}",
    "telnyx_connection_id": "${env:TELNYX_CONNECTION_ID}",
    "telnyx_from_number": "${env:TELNYX_FROM_NUMBER}",
    "telnyx_server_port": 9000,
    "telnyx_public_server_url": "${env:TELNYX_PUBLIC_SERVER_URL}",
    "telnyx_use_https": true,
    "telnyx_use_wss": true
  }
}
```

## Environment Variables

Set the following environment variables in your `.env` file:

```bash
TELNYX_API_KEY=your_telnyx_api_key
TELNYX_CONNECTION_ID=your_connection_id
TELNYX_FROM_NUMBER=+1234567890
TELNYX_PUBLIC_SERVER_URL=your-domain.com:9000
```

## API Reference

### TelnyxControlExtension

The main extension class that handles Telnyx call integration.

#### Methods

- `on_init(ten_env)`: Initialize the extension with configuration
- `on_start(ten_env)`: Start the extension and server
- `on_stop(ten_env)`: Stop the extension and cleanup
- `on_audio_frame(ten_env, audio_frame)`: Handle outgoing audio frames
- `on_cmd(ten_env, cmd)`: Handle commands
- `on_data(ten_env, data)`: Handle data

### TelnyxConfig

Pydantic model for extension configuration.

```python
from config import TelnyxConfig

config = TelnyxConfig(
    greeting="Hello!",
    telnyx_api_key="your-api-key",
    telnyx_connection_id="your-connection-id",
    telnyx_from_number="+1234567890",
    telnyx_server_port=9000,
    telnyx_public_server_url="example.com:9000",
    telnyx_use_https=True,
    telnyx_use_wss=True
)
```