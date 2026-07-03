#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import Any, Dict, List
from pydantic import BaseModel, Field
from ten_ai_base import utils


class StepFunTTSConfig(BaseModel):
    # StepFun TTS credentials
    api_key: str = ""
    base_url: str = "wss://api.stepfun.com/v1/realtime/audio"

    # StepFun TTS specific configs
    # Refer to: https://www.volcengine.com/docs/6561/1329505.
    model: str = "step-tts-mini"
    voice_id: str = "cixingnansheng"
    speed_ratio: float = 1.0
    volume_ratio: float = 1.0
    sample_rate: int = 16000

    # StepFun TTS pass through parameters
    params: Dict[str, Any] = Field(default_factory=dict)
    # Black list parameters, will be removed from params
    black_list_keys: List[str] = Field(default_factory=list)
    dump: bool = False
    dump_path: str = "/tmp"

    def validate_params(self) -> None:
        """Validate required configuration parameters."""
        required_fields = ["api_key"]

        for field_name in required_fields:
            value = getattr(self, field_name)
            if not value or (isinstance(value, str) and value.strip() == ""):
                raise ValueError(
                    f"required fields are missing or empty: params.{field_name}"
                )

    def to_str(self, sensitive_handling: bool = False) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = self.copy(deep=True)
        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)
        return f"{config}"

    def update_params(self) -> None:
        ##### get value from params #####
        if "api_key" in self.params:
            self.api_key = self.params["api_key"]
            del self.params["api_key"]

        if "base_url" in self.params:
            self.base_url = self.params["base_url"]
            del self.params["base_url"]

        if "model" in self.params:
            self.model = self.params["model"]
            del self.params["model"]

        if "voice_id" in self.params:
            self.voice_id = self.params["voice_id"]

        if "speed_ratio" in self.params:
            self.speed_ratio = self.params["speed_ratio"]

        if "volume_ratio" in self.params:
            self.volume_ratio = self.params["volume_ratio"]

        if "sample_rate" in self.params:
            self.sample_rate = self.params["sample_rate"]

    def get_model(self) -> str:
        """Get model name from params"""
        return self.model

    def get_voice(self) -> str:
        """Get voice name from params"""
        return self.voice_id

    def get_speed(self) -> float:
        """Get speed from params"""
        return self.speed_ratio

    def get_volume(self) -> float:
        """Get volume from params"""
        return self.volume_ratio

    def get_sample_rate(self) -> int:
        """Get sample rate from params"""
        return self.sample_rate
