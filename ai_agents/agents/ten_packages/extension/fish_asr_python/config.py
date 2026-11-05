from typing import Any
from urllib.parse import parse_qsl, urlencode, urlparse, urlsplit, urlunsplit

from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


def _redact_configured_url(value: str) -> str:
    """Remove URL userinfo and sensitive query values."""
    if not value:
        return value
    try:
        parsed = urlsplit(value)
        if parsed.hostname is None:
            return "[REDACTED_URL]"

        hostname = parsed.hostname
        if ":" in hostname and not hostname.startswith("["):
            hostname = f"[{hostname}]"
        netloc = hostname
        if parsed.port is not None:
            netloc = f"{netloc}:{parsed.port}"
        query = []
        for key, query_value in parse_qsl(parsed.query, keep_blank_values=True):
            normalized_key = "".join(
                character for character in key.lower() if character.isalnum()
            )
            sensitive = any(
                marker in normalized_key
                for marker in (
                    "key",
                    "token",
                    "secret",
                    "signature",
                    "sig",
                    "auth",
                    "credential",
                    "password",
                    "passwd",
                    "pass",
                    "expires",
                    "expiry",
                )
            )
            query.append((key, "[REDACTED]" if sensitive else query_value))
        sanitized = urlunsplit(
            (
                parsed.scheme,
                netloc,
                parsed.path,
                urlencode(query, doseq=True),
                "",
            )
        )
        return sanitized
    except (TypeError, ValueError):
        # Validation reports malformed URLs separately. Keep logging safe even
        # when it is called before validation has completed.
        return "[REDACTED_URL]"


def _is_url_field(key: str) -> bool:
    normalized = key.lower()
    return (
        "url" in normalized
        or normalized.endswith("uri")
        or normalized.endswith("endpoint")
    )


def _redact_urls(value: Any, key: str | None = None) -> Any:
    if isinstance(value, str):
        if key and _is_url_field(key):
            return _redact_configured_url(value)
        return value
    if isinstance(value, dict):
        return {
            item_key: _redact_urls(item, str(item_key))
            for item_key, item in value.items()
        }
    if isinstance(value, list):
        return [_redact_urls(item) for item in value]
    return value


class FishAudioASRConfig(BaseModel):
    """Configuration for Fish Audio's native realtime ASR API."""

    api_key: str = ""
    url: str = "wss://api.fish.audio/v1/asr/realtime"
    model: str = "transcribe-stream-1"
    sample_rate: int = 16000
    turn_mode: str = "auto"
    eager: bool = True
    drafts: bool = True
    ignore_timestamps: bool = False
    diarize: bool | str = False
    vad_events: bool = False
    eot_events: bool = False
    silence_timeout_ms: int | None = None
    context_history: list[dict[str, str]] = Field(default_factory=list)
    context_terms: list[str] = Field(default_factory=list)
    client_reference_id: str = ""
    dump: bool = False
    dump_path: str = "/tmp"
    params: dict[str, Any] = Field(default_factory=dict)

    def update(self, params: dict[str, Any]) -> None:
        """Apply values supplied through the extension's params property."""
        for key, value in params.items():
            if hasattr(self, key) and key != "params":
                setattr(self, key, value)

    def validate_config(self, supported_sample_rates: set[int]) -> None:
        """Validate values before opening a billed realtime session."""
        if not self.api_key.strip():
            raise ValueError("api_key is required")
        parsed_url = urlparse(self.url)
        if parsed_url.scheme not in {"ws", "wss"} or not parsed_url.hostname:
            raise ValueError("url must be a ws:// or wss:// URL with a host")
        if self.model not in {
            "transcribe-stream-1",
            "transcribe-1-pro-realtime",
        }:
            raise ValueError(
                "model must be transcribe-stream-1 or "
                "transcribe-1-pro-realtime"
            )
        if self.sample_rate not in supported_sample_rates:
            raise ValueError(
                "sample_rate must be one of "
                f"{sorted(supported_sample_rates)}"
            )
        if self.turn_mode not in {"auto", "vad", "manual"}:
            raise ValueError("turn_mode must be auto, vad, or manual")
        if self.diarize not in {True, False, "auto"}:
            raise ValueError("diarize must be true, false, or auto")
        if self.silence_timeout_ms is not None:
            lower_bound = 3000 if self.turn_mode == "auto" else 300
            if not lower_bound <= self.silence_timeout_ms <= 60000:
                raise ValueError(
                    "silence_timeout_ms must be between "
                    f"{lower_bound} and 60000 in {self.turn_mode} mode"
                )
        if self.context_history and (
            len(self.context_history) > 32
            or any(
                item.get("role") not in {"user", "assistant"}
                or len(item.get("text", "")) > 2000
                for item in self.context_history
            )
        ):
            raise ValueError(
                "context_history must contain at most 32 user/assistant "
                "items of 2000 characters"
            )
        if len(self.context_terms) > 100 or any(
            len(term) > 32 for term in self.context_terms
        ):
            raise ValueError(
                "context_terms must contain at most 100 terms of 32 "
                "characters"
            )
        if len(self.client_reference_id) > 256:
            raise ValueError(
                "client_reference_id must be at most 256 characters"
            )

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Serialize configuration with credentials and URLs redacted."""
        config = self.model_copy(deep=True)
        if sensitive_handling:
            if config.api_key:
                config.api_key = encrypt(config.api_key)
            if config.params and "api_key" in config.params:
                config.params["api_key"] = encrypt(config.params["api_key"])
            config.url = _redact_configured_url(config.url)
            config.params = _redact_urls(config.params)
        return config.model_dump_json()

    def build_config_message(self) -> dict[str, Any]:
        """Build the first MessagePack config map accepted by Fish Audio."""
        audio = {
            "format": "pcm_s16le",
            "sample_rate": int(self.sample_rate),
            "channels": 1,
        }
        turn_detection: dict[str, Any] = {"mode": self.turn_mode}
        if self.eager and self.turn_mode == "auto":
            turn_detection["eager"] = True
        if self.silence_timeout_ms is not None:
            turn_detection["silence_timeout_ms"] = int(self.silence_timeout_ms)

        results = {
            "drafts": bool(self.drafts),
            "ignore_timestamps": bool(self.ignore_timestamps),
            "diarize": self.diarize,
            "vad_events": bool(self.vad_events),
            "eot_events": bool(self.eot_events),
        }
        message: dict[str, Any] = {
            "type": "config",
            "audio": audio,
            "turn_detection": turn_detection,
            "results": results,
        }
        history = self.context_history
        terms = self.context_terms
        if history or terms:
            message["context"] = {}
            if history:
                message["context"]["history"] = history
            if terms:
                message["context"]["terms"] = terms
        if self.client_reference_id:
            message["client_reference_id"] = self.client_reference_id
        return message


def redact_configured_url(value: str) -> str:
    """Public URL redactor used by status metadata as well as config logs."""
    return _redact_configured_url(value)
