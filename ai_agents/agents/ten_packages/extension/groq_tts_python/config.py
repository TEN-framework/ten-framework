from typing import Any
import copy
from pydantic import Field
from pathlib import Path
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


class GroqTTSConfig(AsyncTTS2HttpConfig):
    """Groq TTS Config"""

    # Top-level configuration fields
    api_key: str = Field(default="", description="Groq API key")

    dump: bool = Field(default=False, description="Groq TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent / "groq_tts_in.pcm"),
        description="Groq TTS dump path",
    )
    params: dict[str, Any] = Field(
        default_factory=dict, description="Groq TTS params"
    )

    def update_params(self) -> None:
        """Update configuration from params dictionary"""

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Convert config to string with optional sensitive data handling."""
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        # Encrypt sensitive fields
        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)

        return f"{config}"

    def validate(self) -> None:
        """Validate Groq-specific configuration."""
        if not self.api_key:
            raise ValueError("API key is required for Groq TTS")
