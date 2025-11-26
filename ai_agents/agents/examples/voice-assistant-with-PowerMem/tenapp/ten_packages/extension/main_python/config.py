from pydantic import BaseModel


class MainControlConfig(BaseModel):
    greeting: str = "Hello, I am your AI assistant."
    # Memory configuration
    agent_id: str = "voice_assistant_agent"
    agent_name: str = "Voice Assistant with Memory"
    user_id: str = "user"
    user_name: str = "User"
    memu_base_url: str = "https://api.memu.so"
    memu_api_key: str = ""
    self_hosting: bool = False
    enable_memorization: bool = False
