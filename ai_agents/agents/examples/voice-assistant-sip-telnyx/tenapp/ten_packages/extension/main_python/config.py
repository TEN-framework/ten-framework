from pydantic import BaseModel, Field


class TelnyxConfig(BaseModel):
    greeting: str = "Hello, I am your AI assistant."

    # Telnyx configuration
    telnyx_api_key: str = Field(
        default="", description="Telnyx API Key"
    )
    telnyx_connection_id: str = Field(
        default="", description="Telnyx Connection ID"
    )
    telnyx_from_number: str = Field(
        default="", description="Telnyx phone number to call from"
    )

    # Server configuration
    telnyx_server_port: int = Field(
        default=8000,
        description="Port for server (supports both HTTP API and WebSocket)",
    )

    # Public server URL configuration
    telnyx_public_server_url: str = Field(
        default="",
        description="Public server URL without protocol (e.g., 'your-domain.com:9000') - used for both media stream and webhooks",
    )

    # Protocol configuration
    telnyx_use_https: bool = Field(
        default=True,
        description="Use HTTPS for webhooks (True) or HTTP (False)",
    )
    telnyx_use_wss: bool = Field(
        default=True,
        description="Use WSS for media stream (True) or WS (False)",
    )