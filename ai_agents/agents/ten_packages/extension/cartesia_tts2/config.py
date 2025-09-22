from typing import Any
import copy
from ten_ai_base import utils

from pydantic import BaseModel, Field


class CartesiaTTSConfig(BaseModel):
    api_key: str = ""

    sample_rate: int = 16000
    dump: bool = False
    dump_path: str = "/tmp"
    params: dict[str, Any] = Field(default_factory=dict)

    def update_params(self) -> None:
        # Remove params that are not used
        if "transcript" in self.params:
            del self.params["transcript"]

        if "api_key" in self.params:
            self.api_key = self.params["api_key"]
            del self.params["api_key"]

        # Remove params that are not used
        if "context_id" in self.params:
            del self.params["context_id"]

        # Remove params that are not used
        if "stream" in self.params:
            del self.params["stream"]

        # Use default sample rate value
        if "sample_rate" in self.params:
            self.sample_rate = self.params["sample_rate"]
            # Remove sample_rate from params to avoid parameter error
            del self.params["sample_rate"]

        if "output_format" not in self.params:
            self.params["output_format"] = {}

        # Use custom sample rate value
        if "sample_rate" in self.params["output_format"]:
            self.sample_rate = self.params["output_format"]["sample_rate"]
        else:
            self.params["output_format"]["sample_rate"] = self.sample_rate

        ##### use fixed value #####
        self.params["output_format"]["container"] = "raw"
        self.params["output_format"]["encoding"] = "pcm_s16le"

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
