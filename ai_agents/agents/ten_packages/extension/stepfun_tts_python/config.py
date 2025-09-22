#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import Any, Dict, List
from pydantic import BaseModel, Field
from ten_ai_base import utils


class StepFunTTSConfig(BaseModel):
    api_key: str = ""
    base_url: str = "https://api.stepfun.com/v1"
    dump: bool = False
    dump_path: str = "/tmp"
    params: Dict[str, Any] = Field(default_factory=dict)
    black_list_keys: List[str] = ["api_key"]

    def to_str(self, sensitive_handling: bool = False) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = self.copy(deep=True)
        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)
        return f"{config}"

    def update_params(self) -> None:
        # This function allows overriding default config values with 'params' from property.json
        params_dict: Dict[str, Any] = self.params

        for key, value in params_dict.items():
            if hasattr(self, key):
                setattr(self, key, value)

        # Delete keys after iteration is complete
        for key in self.black_list_keys:
            if key in params_dict:
                del params_dict[key]

    def get_model(self) -> str:
        """Get model name from params"""
        return self.params.get("model", "step-tts-mini")

    def get_voice(self) -> str:
        """Get voice name from params"""
        return self.params.get("voice", "cixingnansheng")

    def get_response_format(self) -> str:
        """Get response format from params"""
        return self.params.get("response_format", "mp3")

    def get_speed(self) -> float:
        """Get speed from params"""
        return self.params.get("speed", 1.0)

    def get_volume(self) -> float:
        """Get volume from params"""
        return self.params.get("volume", 1.0)

    def get_sample_rate(self) -> int:
        """Get sample rate from params"""
        return self.params.get("sample_rate", 24000)

    def get_voice_label(self) -> Dict[str, str]:
        """Get voice label from params"""
        return self.params.get("voice_label", {})




