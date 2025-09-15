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

    # Server webhook configuration
    twilio_server_webhook_http_port: int = Field(
        default=8000, description="HTTP port for server webhook endpoints"
    )
    twilio_server_media_ws_port: int = Field(
        default=8001,
        description="WebSocket port for server media streaming endpoints",
    )
    twilio_client_media_ws_url: str = Field(
        default="",
        description="Media WebSocket URL for Twilio client (domain with or without port)",
    )
