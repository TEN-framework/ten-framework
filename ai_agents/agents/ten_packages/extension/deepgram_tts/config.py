#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from __future__ import annotations

from typing import Any
import copy

from ten_ai_base import utils

from pydantic import BaseModel, Field


class DeepgramTTSConfig(BaseModel):
    api_key: str = ""
    base_url: str = "wss://api.deepgram.com/v1/speak"

    model: str = "aura-2-thalia-en"
    encoding: str = "linear16"
    sample_rate: int = 24000

    dump: bool = False
    dump_path: str = "/tmp"
    params: dict[str, Any] = Field(default_factory=dict)

    def update_params(self) -> None:
        params = self._ensure_dict(self.params)
        self.params = params

        if "api_key" in params:
            self.api_key = params["api_key"]
            del params["api_key"]

        if "base_url" in params:
            self.base_url = params["base_url"]
            del params["base_url"]

        if "model" in params:
            self.model = params["model"]
            del params["model"]

        if "encoding" in params:
            self.encoding = params["encoding"]
            del params["encoding"]

        if "sample_rate" in params:
            self.sample_rate = params["sample_rate"]
            del params["sample_rate"]

    def to_str(self, sensitive_handling: bool = True) -> str:
        """
        Convert the configuration to a string representation.
        """
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)

        return f"{config}"

    @staticmethod
    def _ensure_dict(value: Any) -> dict[str, Any]:
        if isinstance(value, dict):
            return value
        return {}
