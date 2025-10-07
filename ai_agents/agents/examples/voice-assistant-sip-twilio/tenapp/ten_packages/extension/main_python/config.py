from pydantic import BaseModel, Field


class MainControlConfig(BaseModel):
    greeting: str = "Hello, I am your AI assistant."

    # Twilio configuration
    twilio_account_sid: str = Field(
        default="", description="Twilio Account SID"
    )
    twilio_auth_token: str = Field(default="", description="Twilio Auth Token")
    twilio_from_number: str = Field(
        default="", description="Twilio phone number to call from"
    )

    # Server configuration
    twilio_server_port: int = Field(
        default=8000,
        description="Port for server (supports both HTTP API and WebSocket)",
    )

    # Public server URL configuration
    twilio_public_server_url: str = Field(
        default="",
        description="Public server URL without protocol (e.g., 'your-domain.com:8000') - used for both media stream and webhooks",
    )

    # Protocol configuration
    twilio_use_https: bool = Field(
        default=True,
        description="Use HTTPS for webhooks (True) or HTTP (False)",
    )
    twilio_use_wss: bool = Field(
        default=True,
        description="Use WSS for media stream (True) or WS (False)",
    )
