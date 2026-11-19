#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import copy
from typing import Any
from urllib.parse import urlsplit, urlunsplit

from pydantic import BaseModel, Field
from ten_ai_base import utils

DEFAULT_BASE_URL = "wss://api.kalpalabs.ai"
ENDPOINT_PATH = "/v1/tts/{voice_id}/stream"
DEFAULT_MODEL = "kalpa-tts-1"
DEFAULT_VOICE_ID = "18c26f49-9f9c-400c-bec9-7467f28e1db5"  # Arjun
DEFAULT_SAMPLE_RATE = 24000
# The output rates Kalpa offers.
SAMPLE_RATES = (8000, 16000, 22050, 24000, 32000, 44100, 48000)

# Keys of the Kalpa wire protocol that the extension sets itself. A value in
# `params` must not reach the generation parameters under one of these names.
_RESERVED_PARAMS = (
    "output_format",
    "generation_config",
    "type",
    "text",
    "flush",
)


def _without_credentials(url: str) -> str:
    """The scheme, host and path of a URL: user info and query may hold a key."""
    try:
        parts = urlsplit(url)
        host = parts.hostname or ""
        if ":" in host:
            host = f"[{host}]"
        if parts.port:
            host = f"{host}:{parts.port}"
    except ValueError:
        return ""
    return urlunsplit((parts.scheme, host, parts.path, "", ""))


class KalpaTTSConfig(BaseModel):
    api_key: str = ""
    # Full WebSocket endpoint. `{voice_id}` in it is replaced by the voice.
    url: str = ""
    base_url: str = ""
    voice_id: str = DEFAULT_VOICE_ID
    model: str = DEFAULT_MODEL
    sample_rate: Any = DEFAULT_SAMPLE_RATE
    chunk_length_schedule: list[int] | None = None
    dump: bool = False
    dump_path: str = "/tmp"
    # After update_params(): Kalpa generation parameters only.
    params: dict[str, Any] = Field(default_factory=dict)

    def update_params(self) -> None:
        """Lift the settings the extension consumes out of `params`.

        What stays is passed to Kalpa as generation parameters
        (`temperature`, `acoustic_temperature`, `audio_quality`, ...).
        """
        params = self.params
        if "api_key" in params:
            self.api_key = str(params.pop("api_key") or "")
        # Endpoint precedence: params.url, the top-level url, params.base_url.
        nested_url = params.pop("url", None)
        if isinstance(nested_url, str) and nested_url.strip():
            self.url = nested_url
        for name in ("base_url", "voice_id", "model"):
            value = params.pop(name, None)
            if value:
                setattr(self, name, str(value))
        if "sample_rate" in params:
            self.sample_rate = params.pop("sample_rate")
        # Left out or empty, Kalpa's own schedule applies.
        schedule = params.pop("chunk_length_schedule", None)
        if schedule:
            if (
                not isinstance(schedule, list)
                or len(schedule) > 10
                or not all(
                    type(item) is int and 50 <= item <= 2000
                    for item in schedule
                )
            ):
                raise ValueError(
                    "'chunk_length_schedule' must be a list of 1 to 10 "
                    "integers from 50 to 2000."
                )
            self.chunk_length_schedule = schedule
        for name in _RESERVED_PARAMS:
            params.pop(name, None)

    def check(self) -> None:
        """Raise ValueError naming the first setting that cannot be used."""
        if not self.api_key:
            raise ValueError("Required parameter 'api_key' is missing.")
        if not self.voice_id:
            raise ValueError("Required parameter 'voice_id' is missing.")
        # `True` and `24000.0` equal an integer without being one.
        if (
            type(self.sample_rate) is not int
            or self.sample_rate not in SAMPLE_RATES
        ):
            rates = ", ".join(str(rate) for rate in SAMPLE_RATES)
            raise ValueError(f"'sample_rate' must be one of {rates}.")
        # No retry can open an address that is not a WebSocket one. The
        # address is not quoted in the error: it may hold a key.
        try:
            parts = urlsplit(self.websocket_url)
            usable = parts.scheme in ("ws", "wss") and bool(parts.hostname)
            _ = parts.port
        except ValueError:
            usable = False
        if not usable:
            raise ValueError(
                "The endpoint ('url' or 'base_url') must be a ws:// or "
                "wss:// address."
            )

    @property
    def websocket_url(self) -> str:
        if self.url:
            return self.url.replace("{voice_id}", self.voice_id)
        base_url = (self.base_url or DEFAULT_BASE_URL).rstrip("/")
        return base_url + ENDPOINT_PATH.format(voice_id=self.voice_id)

    @property
    def safe_url(self) -> str:
        """The endpoint as it may be logged and reported."""
        return _without_credentials(self.websocket_url)

    def session_message(self) -> dict[str, Any]:
        """The `initializeConnection` message. It carries the API key."""
        generation_config: dict[str, Any] = {}
        if self.chunk_length_schedule:
            generation_config["chunk_length_schedule"] = (
                self.chunk_length_schedule
            )
        message: dict[str, Any] = {
            "type": "initializeConnection",
            "api_key": self.api_key,
            "model": self.model,
            "output_format": "pcm_s16le",
            "sample_rate": self.sample_rate,
            # Present, even when empty: audio starts at the first complete
            # sentences instead of waiting for the end of the request.
            "generation_config": generation_config,
        }
        if self.params:
            message["params"] = dict(self.params)
        return message

    def to_str(self, sensitive_handling: bool = True) -> str:
        if not sensitive_handling:
            return f"{self}"
        config = copy.deepcopy(self)
        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)
        config.url = _without_credentials(config.url)
        config.base_url = _without_credentials(config.base_url)
        config.params = utils.redact_json(config.params)
        return f"{config}"
