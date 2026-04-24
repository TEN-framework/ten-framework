from typing import Any

from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


XAI_DEFAULT_PARAMS = {
    "base_url": "wss://api.x.ai/v1/stt",
    "sample_rate": 16000,
    "encoding": "pcm",
    "interim_results": True,
    "endpointing": 300,
    "language": "en",
    "diarize": False,
    "multichannel": False,
    "channels": 1,
}


class XAIASRConfig(BaseModel):
    dump: bool = False
    dump_path: str = "/tmp"
    finalize_timeout_ms: int = 2000
    params: dict[str, Any] = Field(default_factory=dict)

    def apply_defaults(self) -> None:
        params = self.params if isinstance(self.params, dict) else {}
        for key, value in XAI_DEFAULT_PARAMS.items():
            params.setdefault(key, value)
        self.params = params

    def validate(self) -> None:
        if not self.params.get("api_key"):
            raise ValueError("xAI API key is required")
        if self.params.get("sample_rate") not in {
            8000,
            16000,
            22050,
            24000,
            44100,
            48000,
        }:
            raise ValueError(
                f"Unsupported sample_rate: {self.params.get('sample_rate')}"
            )
        if self.params.get("encoding") not in {"pcm", "mulaw", "alaw"}:
            raise ValueError(
                f"Unsupported encoding: {self.params.get('encoding')}"
            )

    def to_json(self, sensitive_handling: bool = False) -> str:
        config_dict = self.model_dump()
        if sensitive_handling and config_dict["params"]:
            api_key = config_dict["params"].get("api_key")
            if api_key:
                config_dict["params"]["api_key"] = encrypt(api_key)
        return str(config_dict)

    @property
    def normalized_language(self) -> str:
        language_map = {
            "zh": "zh-CN",
            "en": "en-US",
            "ja": "ja-JP",
            "ko": "ko-KR",
            "de": "de-DE",
            "fr": "fr-FR",
            "ru": "ru-RU",
            "es": "es-ES",
            "pt": "pt-PT",
            "it": "it-IT",
            "hi": "hi-IN",
            "ar": "ar-AE",
        }
        language_code = (self.params or {}).get("language", "") or ""
        return language_map.get(language_code, language_code)
