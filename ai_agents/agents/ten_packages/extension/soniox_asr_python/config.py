from typing import Any

from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


class SonioxASRConfig(BaseModel):
    url: str = "wss://stt-rt.soniox.com/transcribe-websocket"
    sample_rate: int = 16000
    params: dict[str, Any] = Field(default_factory=dict)
    dump: bool = False
    dump_path: str = "."

    def update(self, params: dict[str, Any]):
        special_params = ["url", "sample_rate", "dump", "dump_path"]
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

        for param_key, value in default_params.items():
            if param_key not in self.params:
                self.params[param_key] = value

        # Remove unsupported features
        if self.params.get("translation"):
            del self.params["translation"]

    def to_json(self, sensitive_handling: bool = False) -> str:
        if not sensitive_handling:
            return self.model_dump_json()

        config = self.model_copy(deep=True)

        if config.params:
            for key, value in config.params.items():
                if key == "api_key":
                    config.params[key] = encrypt(value)

        return config.model_dump_json()

    def to_str(self, sensitive_handling: bool = False) -> str:
        return self.to_json(sensitive_handling)
