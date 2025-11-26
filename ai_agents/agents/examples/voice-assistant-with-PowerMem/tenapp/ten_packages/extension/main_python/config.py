from pydantic import BaseModel


class MainControlConfig(BaseModel):
    greeting: str = "Hello! I'm your AI assistant with memory. I can remember our previous conversations to provide more personalized help."
    # Memory configuration
    agent_id: str = "voice_assistant_agent"
    user_id: str = "user"
    enable_memorization: bool = False
