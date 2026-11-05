#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
from typing import Any
from urllib.parse import urlparse

from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt

from .const import STT_REALTIME_PATH

DEFAULT_BASE_URL = "https://api.blaze.vn"
# The realtime endpoint accepts 16 kHz PCM16 mono only.
SUPPORTED_SAMPLE_RATES = {16000}

_SENSITIVE_SUBSTRINGS = ("key", "token", "secret", "signature", "password")

DEFAULT_PARAMS = {
    "base_url": DEFAULT_BASE_URL,
    "language": "vi",
    "model": "stt-stream-1.5",
    "sample_rate": 16000,
}

# Keys sent in the realtime init message besides the token.
INIT_MESSAGE_KEYS = ("language", "model", "topic", "context")

LANGUAGE_MAP = {
    "vi": "vi-VN",
    "en": "en-US",
}


def _is_sensitive_key(name: str) -> bool:
    lowered = name.lower()
    return any(token in lowered for token in _SENSITIVE_SUBSTRINGS)


class BlazeSTTConfig(BaseModel):
    """Blaze realtime STT configuration."""

    dump: bool = False
    dump_path: str = "/tmp"
    # Silence sent on finalize so Blaze's server-side endpointing closes
    # the current utterance; Blaze has no explicit flush message.
    finalize_silence_ms: int = 800
    finalize_timeout_ms: int = 2000
    params: dict[str, Any] = Field(default_factory=dict)

    def apply_defaults(self) -> None:
        params = self.params if isinstance(self.params, dict) else {}
        for key, value in DEFAULT_PARAMS.items():
            if params.get(key) in (None, ""):
                params[key] = value
        params["base_url"] = str(params["base_url"]).rstrip("/")
        self.params = params

    def validate_config(self) -> None:
        if not self.params.get("api_key"):
            raise ValueError("Blaze STT api_key is required")
        base_url = str(self.params.get("base_url", ""))
        parsed = urlparse(base_url)
        if (
            parsed.scheme not in ("http", "https", "ws", "wss")
            or not parsed.netloc
        ):
            raise ValueError(f"Invalid Blaze STT base_url: {base_url!r}")
        if not self.params.get("language"):
            raise ValueError("Blaze STT language is required")
        if self.sample_rate not in SUPPORTED_SAMPLE_RATES:
            raise ValueError(
                f"Unsupported sample_rate: {self.sample_rate}; "
                "Blaze realtime STT accepts 16000"
            )
        if self.finalize_silence_ms <= 0 or self.finalize_timeout_ms <= 0:
            raise ValueError(
                "finalize_silence_ms and finalize_timeout_ms must be positive"
            )

    def to_json(self, sensitive_handling: bool = False) -> str:
        config_dict = self.model_dump()
        if sensitive_handling and config_dict["params"]:
            for key, value in list(config_dict["params"].items()):
                if value and _is_sensitive_key(key):
                    config_dict["params"][key] = encrypt(str(value))
        return json.dumps(config_dict, ensure_ascii=False)

    @property
    def api_key(self) -> str:
        return str((self.params or {}).get("api_key", "") or "")

    @property
    def sample_rate(self) -> int:
        return int((self.params or {}).get("sample_rate", 16000))

    def ws_url(self) -> str:
        base = str(self.params.get("base_url", DEFAULT_BASE_URL)).rstrip("/")
        if base.startswith("https://"):
            base = "wss://" + base[len("https://") :]
        elif base.startswith("http://"):
            base = "ws://" + base[len("http://") :]
        return f"{base}{STT_REALTIME_PATH}"

    def init_message(self) -> dict[str, Any]:
        """First WebSocket message: token plus session options."""
        message: dict[str, Any] = {"token": self.api_key}
        for key in INIT_MESSAGE_KEYS:
            value = self.params.get(key)
            if value not in (None, ""):
                message[key] = value
        return message

    @property
    def normalized_language(self) -> str:
        language = str((self.params or {}).get("language", "") or "")
        return LANGUAGE_MAP.get(language, language)

    @property
    def asr_result_language(self) -> str:
        """ASRResult.language; params.result_language_default overrides."""
        override = (self.params or {}).get("result_language_default")
        if override is not None and str(override).strip():
            return str(override).strip()
        return self.normalized_language
