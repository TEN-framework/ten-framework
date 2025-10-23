from typing import Any
import copy
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig

from pydantic import Field


class RimeTTSConfig(AsyncTTS2HttpConfig):
    # RIME TTS API credentials
    api_key: str = ""
    # Debug and logging
    dump: bool = False
    dump_path: str = "/tmp"
    sample_rate: int = 16000
    params: dict[str, Any] = Field(default_factory=dict)
    black_list_keys: list[str] = ["api_key", "text"]

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # Extract API key
        if "api_key" in self.params:
            self.api_key = self.params["api_key"]
            del self.params["api_key"]

        self.params["audioFormat"] = "pcm"

        if "samplingRate" in self.params:
            self.sample_rate = int(self.params["samplingRate"])
        elif "sampling_rate" in self.params:
            self.sample_rate = int(self.params["sampling_rate"])

        self.params["segment"] = "immediate"

        # Remove sensitive keys from params
        for key in self.black_list_keys:
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
        if config.params and "api_key" in config.params:
            config.params["api_key"] = utils.encrypt(config.params["api_key"])

        return f"{config}"

    def validate(self) -> None:
        """Validate Rime-specific configuration."""
        if not self.api_key:
            raise ValueError("API key is required for Rime TTS")
