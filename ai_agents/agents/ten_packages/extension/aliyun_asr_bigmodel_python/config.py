from typing import Any, Dict, List
from pydantic import BaseModel, Field
from dataclasses import dataclass
from ten_ai_base.utils import encrypt


@dataclass
class AliyunASRBigmodelConfig(BaseModel):
    api_key: str = ""
    language: str = "en-US"
    language_hints: List[str] = Field(default_factory=lambda: ["en"])
    model: str = "paraformer-realtime-v2"
    sample_rate: int = 16000
    disfluency_removal_enabled: bool = False
    semantic_punctuation_enabled: bool = False
    multi_threshold_mode_enabled: bool = False
    punctuation_prediction_enabled: bool = True
    inverse_text_normalization_enabled: bool = True
    heartbeat: bool = False
    max_sentence_silence: int = 200  # 200ms~6000ms，def 800ms。
    finalize_mode: str = "disconnect"  # "disconnect" or "mute_pkg"
    vocabulary_id: str = ""
    vocabulary_prefix: str = "prefix"
    vocabulary_target_model: str = "paraformer-realtime-v2"
    vocabulary_list: List[Dict[str, Any]] = []
    mute_pkg_duration_ms: int = 1000  # must be greater than max_sentence_silence
    dump: bool = False
    dump_path: str = "/tmp"
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
    def normalized_language(self):
        if self.language_hints[0] == "zh":
            return "zh-CN"
        elif self.language_hints[0] == "en":
            return "en-US"
        elif self.language_hints[0] == "ja":
            return "ja-JP"
        elif self.language_hints[0] == "ko":
            return "ko-KR"
        elif self.language_hints[0] == "de":
            return "de-DE"
        elif self.language_hints[0] == "fr":
            return "fr-FR"
        elif self.language_hints[0] == "ru":
            return "ru-RU"
        else:
            return self.language_hints[0]
