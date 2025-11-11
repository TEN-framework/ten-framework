from typing import Any
import copy
from pathlib import Path
from pydantic import Field
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


class OpenAITTSConfig(AsyncTTS2HttpConfig):
    """OpenAI TTS Config"""

    dump: bool = Field(default=False, description="OpenAI TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(
            Path(__file__).parent / "openai_tts_in.pcm"
        ),
        description="OpenAI TTS dump path",
    )
    params: dict[str, Any] = Field(
        default_factory=dict, description="OpenAI TTS params"
    )

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # Remove input if present (will be set from text)
        if "input" in self.params:
            del self.params["input"]

        # Remove sample_rate from params to avoid parameter error
        # OpenAI TTS sample rate is fixed at 24000 Hz
        if "sample_rate" in self.params:
            del self.params["sample_rate"]

        # Use fixed value
        self.params["response_format"] = "pcm"

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Convert config to string with optional sensitive data handling."""
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        # Encrypt sensitive fields in params
        if config.params and "api_key" in config.params:
            config.params["api_key"] = utils.encrypt(config.params["api_key"])

        return f"{config}"

    def validate(self) -> None:
        """Validate OpenAI-specific configuration."""
        if "api_key" not in self.params or not self.params["api_key"]:
            raise ValueError("API key is required for OpenAI TTS")
        if "model" not in self.params or not self.params["model"]:
            raise ValueError("Model is required for OpenAI TTS")
        if "voice" not in self.params or not self.params["voice"]:
            raise ValueError("Voice is required for OpenAI TTS")
