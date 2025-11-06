from typing import Any
import copy
from pathlib import Path
from pydantic import Field
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


class PlayHTTTSConfig(AsyncTTS2HttpConfig):
    """PlayHT TTS Config"""

    # Top-level configuration fields
    api_key: str = Field(default="", description="PlayHT API key")
    user_id: str = Field(default="", description="PlayHT User ID")

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

        # Encrypt sensitive fields
        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)
        if config.user_id:
            config.user_id = utils.encrypt(config.user_id)

        return f"{config}"

    def validate(self) -> None:
        """Validate PlayHT-specific configuration."""
        if not self.api_key:
            raise ValueError("API key is required for PlayHT TTS")
        if not self.user_id:
            raise ValueError("User ID is required for PlayHT TTS")
