from typing import Any
from enum import Enum

from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


class FinalizeMode(str, Enum):
    DEFAULT = "default"
    IGNORE = "ignore"
    MUTE_PKG = "mute_pkg"


class SonioxASRConfig(BaseModel):
    url: str = "wss://stt-rt.soniox.com/transcribe-websocket"
    sample_rate: int = 16000
    params: dict[str, Any] = Field(default_factory=dict)
    dump: bool = False
    dump_path: str = "."
    finalize_mode: FinalizeMode = FinalizeMode.DEFAULT
    finalize_holding: bool = False
    mute_pkg_duration_ms: int = 800

    def update(self, params: dict[str, Any]):
        special_params = [
            "url",
            "sample_rate",
            "dump",
            "dump_path",
            "finalize_mode",
            "finalize_holding",
            "mute_pkg_duration_ms",
        ]
        for key in special_params:
            if key in params:
                setattr(self, key, params[key])
                del params[key]

        # Set default parameters if not provided
        default_params = {
            "max_non_final_tokens_duration_ms": 360,
            "model": "stt-rt-preview",
            "enable_language_identification": True,
            "audio_format": "pcm_s16le",
            "num_channels": 1,
            "sample_rate": self.sample_rate,
        }

        params_map: dict[str, Any] = self.params

        for param_key, value in default_params.items():
            if param_key not in params_map:
                params_map[param_key] = value

    def to_json(self, sensitive_handling: bool = False) -> str:
        if not sensitive_handling:
            return self.model_dump_json()

        config = self.model_copy(deep=True)

        if config.params:
            params_map: dict[str, Any] = dict(config.params)
            if "api_key" in params_map:
                params_map["api_key"] = encrypt(params_map["api_key"])
            config.params = params_map

        return config.model_dump_json()

    def to_str(self, sensitive_handling: bool = False) -> str:
        return self.to_json(sensitive_handling)
