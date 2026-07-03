from typing import Any, Dict
from pathlib import Path
from pydantic import Field
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


class HumeAiTTSConfig(AsyncTTS2HttpConfig):
    """Hume AI TTS Config"""

    # Hume AI TTS pass through parameters
    params: Dict[str, Any] = Field(
        default_factory=dict, description="Hume AI TTS params"
    )

    # Debug and dump settings
    dump: bool = Field(default=False, description="Hume AI TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(
            Path(__file__).parent / "humeai_tts_in.pcm"
        ),
        description="Hume AI TTS dump path",
    )

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # No special processing needed - all params are passed through

    def validate(self) -> None:
        """Validate required configuration parameters."""
        if "key" not in self.params or not self.params["key"]:
            raise ValueError("API key is required for Hume AI TTS")

    def to_str(self, sensitive_handling: bool = False) -> str:
        """Convert config to string with optional sensitive data handling."""
        if not sensitive_handling:
            return f"{self}"

        config = self.copy(deep=True)

        # Encrypt sensitive fields in params
        if config.params and "key" in config.params:
            config.params["key"] = utils.encrypt(config.params["key"])

        return f"{config}"
