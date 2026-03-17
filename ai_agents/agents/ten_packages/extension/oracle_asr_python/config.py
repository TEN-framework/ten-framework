#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
from typing import Dict, Any
from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


class OracleASRConfig(BaseModel):
    """Oracle Cloud Infrastructure Speech ASR Configuration"""

    dump: bool = False
    dump_path: str = "/tmp"

    params: Dict[str, Any] = Field(default_factory=dict)

    def update(self, params: Dict[str, Any]) -> None:
        updates = {k: v for k, v in params.items() if hasattr(self, k)}
        if updates:
            validated = self.model_validate({**self.model_dump(), **updates})
            for key in updates:
                object.__setattr__(self, key, getattr(validated, key))

    def to_json(self, sensitive_handling: bool = False) -> str:
        config_dict = self.model_dump()
        if sensitive_handling and config_dict["params"]:
            sensitive_keys = ["fingerprint", "key_file", "tenancy", "user"]
            for key in sensitive_keys:
                if key in config_dict["params"] and config_dict["params"][key]:
                    config_dict["params"][key] = encrypt(config_dict["params"][key])
        return json.dumps(config_dict)

    @property
    def normalized_language(self) -> str:
        language_map = {
            "zh": "zh-CN",
            "en": "en-US",
            "ja": "ja-JP",
            "ko": "ko-KR",
            "de": "de-DE",
            "fr": "fr-FR",
            "es": "es-ES",
            "pt": "pt-BR",
            "it": "it-IT",
            "hi": "hi-IN",
            "ar": "ar-AE",
        }
        params_dict = self.params or {}
        language_code = params_dict.get("language", "") or ""
        return language_map.get(language_code, language_code)
