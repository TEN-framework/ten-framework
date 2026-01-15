from typing import Any
import copy
from ten_ai_base import utils

from pydantic import BaseModel, Field


class MurfTTSConfig(BaseModel):
    # MURF TTS API credentials
    base_url: str = "wss://global.api.murf.ai/v1/speech/stream-input"
    api_key: str = ""
    # Debug and logging
    dump: bool = False
    dump_path: str = "/tmp"
    sample_rate: int = 24000
    params: dict[str, Any] = Field(default_factory=dict)
    audio_format: str = "PCM"
    model: str = "FALCON"

    black_list_keys: list[str] = ["audio_format"]

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # Remove sensitive keys from params
        for key in self.black_list_keys:
            if key in self.params:
                del self.params[key]

        for key, value in list(self.params.items()):
            if hasattr(self, key):
                setattr(self, key, value)
                del self.params[key]

    def to_str(self, sensitive_handling: bool = True) -> str:
        """
        Convert the configuration to a string representation, masking sensitive data.
        """
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        # Encrypt sensitive fields
        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)
        if config.params and "api_key" in config.params:
            config.params["api_key"] = utils.encrypt(config.params["api_key"])

        return f"{config}"
