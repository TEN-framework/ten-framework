from typing import Any
import copy
from pathlib import Path
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig

from pydantic import Field


class RimeTTSConfig(AsyncTTS2HttpConfig):
    """Rime TTS Config"""

    # Top-level configuration fields
    api_key: str = Field(default="", description="Rime API key")

    # Debug and logging
    dump: bool = Field(default=False, description="Rime TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent / "rime_tts_in.pcm"),
        description="Rime TTS dump path",
    )
    params: dict[str, Any] = Field(
        default_factory=dict, description="Rime TTS params"
    )

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # Keys to exclude from params after processing (not passthrough params)
        blacklist_keys = ["text", "sample_rate"]

        self.params["audioFormat"] = "pcm"

        # Normalize sample rate key
        if "samplingRate" in self.params:
            sample_rate = int(self.params["samplingRate"])
            self.params["sample_rate"] = sample_rate
        elif "sampling_rate" in self.params:
            self.params["sample_rate"] = int(self.params["sampling_rate"])

        self.params["segment"] = "immediate"

        # Remove blacklisted keys from params
        for key in blacklist_keys:
            if key in self.params:
                del self.params[key]

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
        """Validate Rime-specific configuration."""
        if not self.api_key:
            raise ValueError("API key is required for Rime TTS")
