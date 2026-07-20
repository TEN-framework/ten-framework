from typing import Any, Dict, List
from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


# Pulse speaks bare ISO 639-1 codes on the wire, while TEN asr_result
# consumers expect BCP-47 tags (e.g. the guarder validates "en-US").
LANGUAGE_TAG_MAP = {
    "en": "en-US",
    "zh": "zh-CN",
    "es": "es-ES",
    "fr": "fr-FR",
    "de": "de-DE",
    "it": "it-IT",
    "pt": "pt-PT",
    "ja": "ja-JP",
    "ko": "ko-KR",
    "ru": "ru-RU",
    "ar": "ar-AE",
    "hi": "hi-IN",
    "ta": "ta-IN",
    "te": "te-IN",
    "kn": "kn-IN",
    "ml": "ml-IN",
    "bn": "bn-IN",
    "gu": "gu-IN",
    "mr": "mr-IN",
    "pa": "pa-IN",
    "or": "or-IN",
}


class SmallestASRConfig(BaseModel):
    api_key: str = ""
    url: str = "wss://api.smallest.ai/waves/v1/stt/live"
    language: str = "en"  # ISO language code, e.g. "en", "hi"
    model: str = "pulse"  # Pulse is the only streaming-capable model
    sample_rate: int = 16000
    encoding: str = "linear16"
    dump: bool = False
    dump_path: str = "/tmp"
    params: Dict[str, Any] = Field(default_factory=dict)
    black_list_params: List[str] = Field(
        default_factory=lambda: [
            "api_key",
            "url",
            "model",
            "language",
            "sample_rate",
            "encoding",
            "dump",
            "dump_path",
        ]
    )

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params

    def update(self, params: Dict[str, Any]) -> None:
        """Update configuration with additional parameters."""
        for key, value in params.items():
            if hasattr(self, key):
                setattr(self, key, value)

    def to_json(self, sensitive_handling: bool = False) -> str:
        """Convert config to JSON string with optional sensitive data handling."""
        config_dict = self.model_dump()
        if sensitive_handling and self.api_key:
            config_dict["api_key"] = encrypt(config_dict["api_key"])
        if config_dict["params"]:
            for key, value in config_dict["params"].items():
                if key == "api_key":
                    config_dict["params"][key] = encrypt(value)
        return str(config_dict)

    def get_ws_url(self) -> str:
        """Get the WebSocket URL."""
        return self.url

    def wire_language(self) -> str:
        """Language code sent to Pulse: bare ISO 639-1 (en-US -> en)."""
        return self.language.split("-")[0].lower()

    def report_language(self, vendor_language: str | None = None) -> str:
        """Language tag reported in asr_result: BCP-47.

        The configured language wins (a full tag like en-IN is passed
        through; a bare ISO code is normalized via LANGUAGE_TAG_MAP).
        Pulse's per-result detection is only trusted in auto/multi
        modes, where no single language was configured.
        """
        if "-" in self.language and not self.language.startswith("multi"):
            return self.language
        code = self.language
        if code.startswith("multi") or code == "north_indic":
            code = vendor_language or code
        code = code.split("-")[0].lower()
        return LANGUAGE_TAG_MAP.get(code, code)
