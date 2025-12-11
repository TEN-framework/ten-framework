from typing import Any, Dict
from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


class ElevenLabsASRConfig(BaseModel):
    """ElevenLabs ASR Configuration"""

    # Debugging and dumping
    dump: bool = False
    dump_path: str = "/tmp"

    # Additional parameters
    params: Dict[str, Any] = Field(default_factory=dict)

    def update(self, params: Dict[str, Any]) -> None:
        """Update configuration with additional parameters."""
        for key, value in params.items():
            if hasattr(self, key):
                setattr(self, key, value)

    def to_json(self) -> str:
        """Convert config to JSON string."""
        config_dict = self.model_dump()
        if config_dict["params"]:
            for key, value in config_dict["params"].items():
                if key == "api_key":
                    config_dict["params"][key] = encrypt(value)
        return str(config_dict)

    @property
    def normalized_language(self) -> str:
        """Convert language code to normalized format for ElevenLabs"""
        # ElevenLabs uses ISO 639-1 language codes
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
        params_dict = self.params if isinstance(self.params, dict) else {}
        language_code = params_dict.get("language_code", "") or ""
        return language_map.get(language_code, language_code)
