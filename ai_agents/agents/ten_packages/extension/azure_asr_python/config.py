from dataclasses import field
from pydantic import BaseModel
from typing import Any

from .const import FINALIZE_MODE_MUTE_PKG
from ten_ai_base.utils import encrypt


class AzureASRConfig(BaseModel):
    key: str = ""
    region: str = ""
    language: str = "en-US"
    language_list: list[str] = field(default_factory=lambda: [])
    sample_rate: int = 16000
    params: dict[str, Any] = field(default_factory=dict)
    advanced_params_json: str = ""
    finalize_mode: str = FINALIZE_MODE_MUTE_PKG  # "disconnect" or "mute_pkg"
    mute_pkg_duration_ms: int = 800
    phrase_list: list[str] = field(default_factory=lambda: [])
    hotwords: list[str] = field(default_factory=lambda: [])
    dump: bool = False
    dump_path: str = "."

    def update(self, params: dict[str, Any]):
        for key, value in params.items():
            if hasattr(self, key):
                setattr(self, key, value)

        # If language string is divided by comma, split it and add to language_list
        if "," in self.language:
            self.language_list = self.language.split(",")
        else:
            self.language_list.append(self.language)

        # If hotwords is not empty, remove the content after | and add to phrase_list
        if self.hotwords and len(self.hotwords) > 0:
            self.phrase_list.clear()

            for hotword in self.hotwords:
                if "|" in hotword:
                    self.phrase_list.append(hotword.split("|")[0])
                else:
                    self.phrase_list.append(hotword)

    def to_json(self, sensitive_handling: bool = False) -> str:
        if not sensitive_handling:
            return self.model_dump_json()

        config = self.model_copy(deep=True)
        if config.key:
            config.key = encrypt(config.key)

        if config.params:
            for key, value in config.params.items():
                if key == "key":
                    config.params[key] = encrypt(value)

        return config.model_dump_json()

    def primary_language(self) -> str:
        return self.language_list[0]
