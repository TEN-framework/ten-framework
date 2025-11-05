from typing import Any
import copy
from pathlib import Path
from pydantic import Field
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


class PlayHTTTSConfig(AsyncTTS2HttpConfig):
    """PlayHT TTS Config"""

    dump: bool = Field(default=False, description="PlayHT TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(
            Path(__file__).parent / "playht_tts_in.pcm"
        ),
        description="PlayHT TTS dump path",
    )
    params: dict[str, Any] = Field(
        default_factory=dict, description="PlayHT TTS params"
    )

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # Force audio format to PCM by removing any user-provided format
        if "format" in self.params:
            del self.params["format"]

        # Set default protocol to "ws" if not provided
        if "protocol" not in self.params:
            self.params["protocol"] = "ws"

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Convert config to string with optional sensitive data handling."""
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        # Encrypt sensitive fields in params
        if config.params:
            if "api_key" in config.params:
                config.params["api_key"] = utils.encrypt(
                    config.params["api_key"]
                )
            if "user_id" in config.params:
                config.params["user_id"] = utils.encrypt(
                    config.params["user_id"]
                )

        return f"{config}"

    def validate(self) -> None:
        """Validate PlayHT-specific configuration."""
        if "api_key" not in self.params or not self.params["api_key"]:
            raise ValueError("API key is required for PlayHT TTS")
        if "user_id" not in self.params or not self.params["user_id"]:
            raise ValueError("User ID is required for PlayHT TTS")
