from pydantic import BaseModel, Field


class MainControlConfig(BaseModel):
    greeting: str = "Hello, I am your AI assistant."

    # Plivo configuration
    plivo_auth_id: str = Field(
        default="", description="Plivo Auth ID"
    )
    plivo_auth_token: str = Field(default="", description="Plivo Auth Token")
    plivo_from_number: str = Field(
        default="", description="Plivo phone number to call from"
    )

    # Server configuration
    plivo_server_port: int = Field(
        default=9000,
        description="Port for server (supports both HTTP API and WebSocket)",
    )

    # Public server URL configuration
    plivo_public_server_url: str = Field(
        default="",
        description="Public server URL without protocol (e.g., 'your-domain.com:9000') - used for both media stream and webhooks",
    )

    # Protocol configuration
    plivo_use_https: bool = Field(
        default=True,
        description="Use HTTPS for webhooks (True) or HTTP (False)",
    )
    plivo_use_wss: bool = Field(
        default=True,
        description="Use WSS for media stream (True) or WS (False)",
    )
