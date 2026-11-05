#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from __future__ import annotations

from typing import Any
from urllib.parse import urlparse
import copy

from ten_ai_base import utils

from pydantic import BaseModel, Field

DEFAULT_BASE_URL = "https://api.blaze.vn"
_SENSITIVE_SUBSTRINGS = ("key", "token", "secret", "signature", "password")
DEFAULT_SPEAKER_ID = "HN-Nu-CSKH-HuongGiang"
SUPPORTED_SAMPLE_RATES = {8000, 16000, 22050, 24000, 44100, 48000}

# Keys moved out of `params` into first-class fields. Everything left in
# `params` is added to each realtime query message.
CONFIG_KEYS = (
    "api_key",
    "base_url",
    "speaker_id",
    "language",
    "model",
    "audio_speed",
    "audio_quality",
    "normalization",
    "sample_rate",
)


def _is_sensitive_key(name: str) -> bool:
    lowered = name.lower()
    return any(token in lowered for token in _SENSITIVE_SUBSTRINGS)


def _redact_url(url: str) -> str:
    """Keep only scheme, host, port and path of a URL for logging."""
    try:
        parsed = urlparse(url)
        host = parsed.hostname or ""
        port = f":{parsed.port}" if parsed.port else ""
    except ValueError:
        return "<invalid url>"
    redacted = f"{parsed.scheme}://{host}{port}{parsed.path}"
    if parsed.username or parsed.password or parsed.query or parsed.fragment:
        redacted += " (credentials, query or fragment redacted)"
    return redacted


class BlazeTTSConfig(BaseModel):
    api_key: str = ""
    base_url: str = DEFAULT_BASE_URL

    speaker_id: str = DEFAULT_SPEAKER_ID
    language: str = "vi"
    model: str = "2.0-realtime"
    audio_speed: float = 1.0
    audio_quality: int = 64
    normalization: str = "basic"
    sample_rate: int = 24000

    dump: bool = False
    dump_path: str = "/tmp"
    params: dict[str, Any] = Field(default_factory=dict)

    def update_params(self) -> None:
        params = self._ensure_dict(self.params)
        self.params = params

        for key in CONFIG_KEYS:
            if key in params:
                value = params.pop(key)
                # Empty env values (`${env:VAR|}`) keep the defaults.
                if value is not None and value != "":
                    setattr(self, key, value)

        self.api_key = str(self.api_key or "")
        self.base_url = str(self.base_url or DEFAULT_BASE_URL).rstrip("/")
        self.speaker_id = str(self.speaker_id or DEFAULT_SPEAKER_ID)
        self.audio_speed = float(self.audio_speed)
        self.audio_quality = int(self.audio_quality)
        self.sample_rate = int(self.sample_rate)

    def validate_config(self) -> None:
        if not self.api_key:
            raise ValueError("API key is required")
        parsed = urlparse(self.base_url)
        # Blaze needs no credentials or query in the URL. Reject them so a
        # secret can never reach logs, metadata or status events, and do
        # not echo the URL in the error.
        if parsed.username or parsed.password or parsed.query:
            raise ValueError(
                "Blaze TTS base_url must not contain credentials or a query"
            )
        if parsed.fragment:
            raise ValueError("Blaze TTS base_url must not contain a fragment")
        if (
            parsed.scheme not in ("http", "https", "ws", "wss")
            or not parsed.hostname
        ):
            raise ValueError(
                "Blaze TTS base_url must be an http(s) or ws(s) URL"
            )
        if self.sample_rate not in SUPPORTED_SAMPLE_RATES:
            raise ValueError(f"Unsupported sample_rate: {self.sample_rate}")
        if self.audio_speed <= 0:
            raise ValueError("audio_speed must be greater than 0")

    @property
    def ws_url(self) -> str:
        """Realtime endpoint derived from base_url (http(s) -> ws(s))."""
        base = self.base_url
        if base.startswith("https://"):
            base = "wss://" + base[len("https://") :]
        elif base.startswith("http://"):
            base = "ws://" + base[len("http://") :]
        return f"{base}/v1/tts/realtime"

    def to_str(self, sensitive_handling: bool = True) -> str:
        """
        Convert the configuration to a string representation.
        """
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)
        config.base_url = _redact_url(config.base_url)
        for key, value in list(config.params.items()):
            if value and _is_sensitive_key(key):
                config.params[key] = utils.encrypt(str(value))

        return f"{config}"

    @staticmethod
    def _ensure_dict(value: Any) -> dict[str, Any]:
        if isinstance(value, dict):
            return value
        return {}
