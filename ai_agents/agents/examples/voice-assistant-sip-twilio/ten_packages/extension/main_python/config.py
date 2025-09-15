from pydantic import BaseModel, Field


class MainControlConfig(BaseModel):
    greeting: str = "Hello, I am your AI assistant."

    # Twilio configuration
    twilio_account_sid: str = Field(default="", description="Twilio Account SID")
    twilio_auth_token: str = Field(default="", description="Twilio Auth Token")
    twilio_from_number: str = Field(default="", description="Twilio phone number to call from")
    twilio_webhook_url: str = Field(default="", description="Webhook URL for Twilio to connect to")

    # Server configuration
    http_port: int = Field(default=8000, description="HTTP server port")
    websocket_port: int = Field(default=8001, description="WebSocket server port")
