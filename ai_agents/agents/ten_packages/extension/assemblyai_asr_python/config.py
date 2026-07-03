from typing import Any, Dict, List, Optional
from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


class AssemblyAIASRConfig(BaseModel):
    """AssemblyAI ASR Configuration"""

    # Authentication
    api_key: str = ""

    # WebSocket endpoint
    ws_url: str = "wss://streaming.assemblyai.com/v3/ws"

    # Audio settings
    sample_rate: int = 16000
    encoding: str = "pcm_s16le"

    # Real-time transcription settings
    end_of_turn_confidence_threshold: Optional[float] = 0.4
    format_turns: bool = True
    keyterms_prompt: Optional[List[str]] = Field(default_factory=list)
    min_end_of_turn_silence_when_confident: Optional[int] = 160
    max_turn_silence: Optional[int] = 400

    # Language settings
    language: str = "en-US"

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

    @property
    def normalized_language(self) -> str:
        """Convert language code to normalized format"""
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
        }
        return language_map.get(self.language, self.language)
