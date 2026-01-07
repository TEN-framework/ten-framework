from typing import Dict, Any
from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


class DeepgramASRConfig(BaseModel):
    """Deepgram ASR Configuration"""

    # Debugging and dumping
    dump: bool = False
    dump_path: str = "/tmp"
    finalize_mode: str = "disconnect"  # "disconnect" or "mute_pkg"
    mute_pkg_duration_ms: int = 1000
    # Additional parameters
    params: Dict[str, Any] = Field(default_factory=dict)

    def update(self, params: Dict[str, Any]) -> None:
        """Update configuration with additional parameters."""
        for key, value in params.items():
            if hasattr(self, key):
                setattr(self, key, value)

    def to_json(self, sensitive_handling: bool = False) -> str:
        """Convert config to JSON string with optional sensitive data handling."""
        config_dict = self.model_dump()
        if sensitive_handling and config_dict["params"]:
            for key, value in config_dict["params"].items():
                if key == "api_key":
                    config_dict["params"][key] = encrypt(value)
                if key == "key":
                    config_dict["params"][key] = encrypt(value)
        return str(config_dict)

    @property
    def normalized_language(self) -> str:
        """Convert language code to normalized format for Deepgram"""
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
        params_dict = self.params or {}
        language_code = params_dict.get("language", "") or ""
        return language_map.get(language_code, language_code)
