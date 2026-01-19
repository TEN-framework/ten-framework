from typing import Any
import copy
from pathlib import Path
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig

from pydantic import Field


class InworldTTSConfig(AsyncTTS2HttpConfig):
    """Inworld TTS Config"""

    # Debug and logging
    dump: bool = Field(default=False, description="Inworld TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent / "inworld_tts_in.pcm"),
        description="Inworld TTS dump path",
    )
    params: dict[str, Any] = Field(
        default_factory=dict, description="Inworld TTS params"
    )

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # Keys to exclude from params (used internally, not passed to API)
        blacklist_keys = [
            "text",
            "endpoint",
        ]

        # Ensure sampleRate is an integer
        if "sampleRate" in self.params:
            self.params["sampleRate"] = int(self.params["sampleRate"])

        # Remove blacklisted keys from params
        for key in blacklist_keys:
            if key in self.params:
                del self.params[key]

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
        """Validate Inworld-specific configuration."""
        if "api_key" not in self.params or not self.params["api_key"]:
            raise ValueError("API key is required for Inworld TTS")
        if "voice" not in self.params or not self.params["voice"]:
            raise ValueError("Voice is required for Inworld TTS")
