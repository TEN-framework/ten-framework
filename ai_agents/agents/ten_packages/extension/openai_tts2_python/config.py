from typing import Any
import copy
from ten_ai_base import utils

from pydantic import BaseModel, Field


class OpenaiTTSConfig(BaseModel):
    api_key: str = ""

    dump: bool = False
    dump_path: str = "/tmp"
    params: dict[str, Any] = Field(default_factory=dict)

    # Fixed value, it can not be changed
    # Refer to https://platform.openai.com/docs/api-reference/audio/createSpeech
    sample_rate: int = 24000

    def update_params(self) -> None:
        if "api_key" in self.params:
            self.api_key = self.params["api_key"]
            del self.params["api_key"]

        if "input" in self.params:
            del self.params["input"]

        # Remove sample_rate from params to avoid parameter error
        if "sample_rate" in self.params:
            del self.params["sample_rate"]

        # Use fixed value
        self.params["response_format"] = "pcm"
        self.sample_rate = 24000

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
