from typing import Dict, Any

from pydantic import BaseModel, Field


class MainControlConfig(BaseModel):
    greeting: str = (
        "Hello! I'm your AI assistant with memory. I can remember our previous conversations to provide more personalized help."
    )
    # Memory configuration
    agent_id: str = "voice_assistant_agent"
    user_id: str = "user"
    enable_memorization: bool = True
    enable_user_memory: bool = True
    # Memory save rules
    memory_save_interval_turns: int = (
        5  # Save memory every N turns of conversation
    )
    # Save memory after N seconds of inactivity
    memory_idle_timeout_seconds: float = 30.0
    # PowerMem Configuration
    powermem_config: Dict[str, Any] = Field(default_factory=dict)
