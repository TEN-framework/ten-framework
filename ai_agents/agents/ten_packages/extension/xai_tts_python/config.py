from __future__ import annotations

from typing import Any
import copy

from ten_ai_base import utils

from pydantic import BaseModel, Field


_SENSITIVE_SUBSTRINGS = ("key", "token", "secret", "signature", "password")


def _is_sensitive_key(name: str) -> bool:
    lowered = name.lower()
    return any(token in lowered for token in _SENSITIVE_SUBSTRINGS)


class XAITTSConfig(BaseModel):
    api_key: str = ""
    base_url: str = "wss://api.x.ai/v1/tts"
    voice_id: str = "eve"
    language: str = "en"
    codec: str = "pcm"
    sample_rate: int = 24000
    bit_rate: int = 128000
    optimize_streaming_latency: int = 0
    text_normalization: bool = False

    dump: bool = False
    dump_path: str = "/tmp"
    params: dict[str, Any] = Field(default_factory=dict)

    _TYPED_PARAM_KEYS = (
        "api_key",
        "base_url",
        "voice_id",
        "language",
        "codec",
        "sample_rate",
        "bit_rate",
        "optimize_streaming_latency",
        "text_normalization",
    )

    def update_params(self) -> None:
        params = self._ensure_dict(self.params)
        self.params = params

        self.api_key = str(params.get("api_key", self.api_key) or "")
        self.base_url = str(params.get("base_url", self.base_url) or "")
        self.voice_id = str(params.get("voice_id", self.voice_id) or "")
        self.language = str(params.get("language", self.language) or "")
        self.codec = str(params.get("codec", self.codec) or "")
        self.sample_rate = int(
            params.get("sample_rate", self.sample_rate) or self.sample_rate
        )
        self.bit_rate = int(
            params.get("bit_rate", self.bit_rate) or self.bit_rate
        )
        self.optimize_streaming_latency = int(
            params.get(
                "optimize_streaming_latency",
                self.optimize_streaming_latency,
            )
            or self.optimize_streaming_latency
        )
        self.text_normalization = self._coerce_bool(
            params.get("text_normalization", self.text_normalization),
            self.text_normalization,
        )
        for key in self._TYPED_PARAM_KEYS:
            params.pop(key, None)

    def validate_config(self) -> None:
        if not self.api_key:
            raise ValueError("API key is required")
        if self.sample_rate not in {8000, 16000, 22050, 24000, 44100, 48000}:
            raise ValueError(f"Unsupported sample rate: {self.sample_rate}")
        if self.codec not in {"pcm", "mp3", "wav", "mulaw", "ulaw", "alaw"}:
            raise ValueError(f"Unsupported codec: {self.codec}")

    def to_str(self, sensitive_handling: bool = True) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)

        if isinstance(config.params, dict):
            for key, value in list(config.params.items()):
                if value and _is_sensitive_key(key):
                    config.params[key] = utils.encrypt(str(value))

        return f"{config}"

    @staticmethod
    def _ensure_dict(value: Any) -> dict[str, Any]:
        if isinstance(value, dict):
            return value
        return {}

    @staticmethod
    def _coerce_bool(value: Any, default: bool) -> bool:
        if isinstance(value, bool):
            return value
        if isinstance(value, str):
            normalized = value.strip().lower()
            if normalized in {"true", "1", "yes", "on"}:
                return True
            if normalized in {"false", "0", "no", "off", ""}:
                return False
            return default
        if isinstance(value, (int, float)):
            return bool(value)
        return default
