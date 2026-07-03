from typing import Any
import copy
from ten_ai_base import utils

from pydantic import BaseModel, Field


class RimeTTSConfig(BaseModel):
    # RIME TTS API credentials
    base_url: str = "wss://users.rime.ai/ws2"
    api_key: str = ""
    # Debug and logging
    dump: bool = False
    dump_path: str = "/tmp"
    sampling_rate: int = 16000
    params: dict[str, Any] = Field(default_factory=dict)
    black_list_keys: list[str] = ["api_key", "text"]

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # Extract API key
        if "api_key" in self.params:
            self.api_key = self.params["api_key"]
            del self.params["api_key"]

        if "base_url" in self.params:
            self.base_url = self.params["base_url"]
            del self.params["base_url"]

        self.params["audioFormat"] = "pcm"

        if "samplingRate" in self.params:
            self.sampling_rate = int(self.params["samplingRate"])
        elif "sampling_rate" in self.params:
            self.sampling_rate = int(self.params["sampling_rate"])

        self.params["segment"] = "immediate"

        # Remove sensitive keys from params
        for key in self.black_list_keys:
            if key in self.params:
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
