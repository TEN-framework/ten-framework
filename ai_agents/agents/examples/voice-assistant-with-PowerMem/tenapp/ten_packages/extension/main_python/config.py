from pydantic import BaseModel


class MainControlConfig(BaseModel):
    greeting: str = "Hello, I am your AI assistant."
    # Memory configuration
    agent_id: str = "voice_assistant_agent"
    user_id: str = "user"
    enable_memorization: bool = False
