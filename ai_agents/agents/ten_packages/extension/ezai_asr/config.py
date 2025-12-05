from typing import Any, Dict, List
from pydantic import BaseModel, Field
from dataclasses import dataclass
from ten_ai_base.utils import encrypt


@dataclass
class ASRConfig(BaseModel):
    token: str = ""
    url: str = "wss://rt2.ezai-k8s.freeddns.org"
    language: str = "en-US"
    language_list: List[str] = Field(default_factory=lambda: ["en-US"])
    sample_rate: int = 16000
    channels: int = 1
    sampwidth: int = 2
    encoding: str = "linear16"
    interim_results: bool = True
    punctuate: bool = True
    finalize_mode: str = "disconnect"  # "disconnect" or "mute_pkg"
    mute_pkg_duration_ms: int = 100
    dump: bool = False
    dump_path: str = "/tmp"
    advanced_params_json: str = ""
    params: Dict[str, Any] = Field(default_factory=dict)
    black_list_params: List[str] = Field(
        default_factory=lambda: [
            "channels",
            "encoding",
            "multichannel",
            "sample_rate",
            "callback_method",
            "callback",
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
        if sensitive_handling and self.token:
            config_dict["token"] = encrypt(config_dict["token"])
        if config_dict["params"]:
            for key, value in config_dict["params"].items():
                if key == "token":
                    config_dict["params"][key] = encrypt(value)
        return str(config_dict)

    @property
    def normalized_language(self):
        if self.language == "zh-CN":
            return "zh-CN"
        elif self.language == "en-US":
            return "en-US"
        elif self.language == "es-ES":
            return "es"
        elif self.language == "ja-JP":
            return "ja"
        elif self.language == "ko-KR":
            return "ko-KR"
        elif self.language == "ar-AE":
            return "ar"
        elif self.language == "hi-IN":
            return "hi"
        else:
            return self.language
