from pydantic import BaseModel


class MainControlConfig(BaseModel):
    greeting: str = "Hello, I am your AI assistant."
    # Memory configuration
    agent_id: str = "voice_assistant_agent"
    agent_name: str = "Voice Assistant with Memory"
    user_id: str = "user"
    user_name: str = "User"
    memos_api_key: str = ""  # Optional, can also use MEMOS_API_KEY env var
    memos_base_url: str = ""  # MemOS API base URL, can also use MEMOS_BASE_URL env var
    enable_memorization: bool = False
