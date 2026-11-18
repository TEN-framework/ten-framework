from copy import deepcopy
import re
from typing import Any, Dict
from urllib.parse import urlsplit

from pydantic import BaseModel, Field

from ten_ai_base.utils import encrypt, redact_url

# Pylint cannot infer Pydantic's Field-backed mapping attributes.
# pylint: disable=no-member


DEFAULT_WS_URL = "wss://api.elevenlabs.io/v1/speech-to-text/realtime"
DEFAULT_MODEL_ID = "scribe_v2_realtime"
SUPPORTED_AUDIO_FORMATS = {
    "pcm_8000": 8000,
    "pcm_16000": 16000,
    "pcm_22050": 22050,
    "pcm_24000": 24000,
    "pcm_44100": 44100,
    "pcm_48000": 48000,
    "ulaw_8000": 8000,
}
SUPPORTED_COMMIT_STRATEGIES = {"manual", "vad", "turn_prediction"}


class ElevenLabsASRConfig(BaseModel):
    """Configuration for the ElevenLabs realtime Scribe API."""

    dump: bool = False
    dump_path: str = "/tmp"
    # ``language`` is the public ASR property used by TEN integrations.  The
    # ElevenLabs query parameter is ``language_code`` and accepts ISO-639
    # values, so it is copied and normalized in ``apply_defaults``.
    language: str | None = None
    params: Dict[str, Any] = Field(default_factory=dict)

    def apply_defaults(self) -> None:
        """Normalize vendor parameters without dropping pass-through options."""
        defaults = {
            "ws_url": DEFAULT_WS_URL,
            "model_id": DEFAULT_MODEL_ID,
            "audio_format": "pcm_16000",
            "sample_rate": 16000,
            "include_timestamps": False,
            "commit_strategy": "vad",
            "vad_threshold": 0.4,
            "vad_silence_threshold_secs": 0.35,
            "min_speech_duration_ms": 100,
            "min_silence_duration_ms": 100,
            "enable_logging": True,
            "connect_timeout": 10,
            "finalize_timeout": 2,
        }
        for key, value in defaults.items():
            self.params.setdefault(key, value)

        # Keep an explicitly supplied vendor language untouched.  When a
        # customer only supplies TEN's top-level language, translate its
        # BCP-47 value to the ISO code expected by ElevenLabs.
        if "language_code" not in self.params:
            public_language = self.params.get("language") or self.language
            if isinstance(public_language, str) and public_language.strip():
                self.params["language_code"] = self.to_provider_language_code(
                    public_language
                )

    def validate_config(self) -> None:
        """Validate values that otherwise fail only after opening a socket."""
        self.apply_defaults()
        api_key = self.params.get("api_key")
        token = self.params.get("token")
        if (not isinstance(api_key, str) or not api_key.strip()) and (
            not isinstance(token, str) or not token.strip()
        ):
            raise ValueError("params.api_key or params.token is required")

        ws_url = self.params.get("ws_url")
        parsed_url = urlsplit(ws_url) if isinstance(ws_url, str) else None
        if (
            parsed_url is None
            or parsed_url.scheme != "wss"
            or not parsed_url.netloc
        ):
            raise ValueError("params.ws_url must be a wss:// URL")

        model_id = self.params.get("model_id")
        if model_id != DEFAULT_MODEL_ID:
            raise ValueError(
                f"params.model_id must be {DEFAULT_MODEL_ID!r} for realtime ASR"
            )

        audio_format = self.params.get("audio_format")
        if audio_format not in SUPPORTED_AUDIO_FORMATS:
            raise ValueError(
                "params.audio_format must be one of "
                f"{sorted(SUPPORTED_AUDIO_FORMATS)}"
            )
        if audio_format == "ulaw_8000":
            raise ValueError(
                "params.audio_format ulaw_8000 is unsupported because TEN "
                "audio input is PCM16"
            )

        sample_rate = self.params.get("sample_rate")
        expected_rate = SUPPORTED_AUDIO_FORMATS[audio_format]
        if not isinstance(sample_rate, int) or isinstance(sample_rate, bool):
            raise ValueError("params.sample_rate must be an integer")
        if sample_rate != expected_rate:
            raise ValueError(
                f"params.sample_rate must be {expected_rate} for {audio_format}"
            )

        language_code = self.params.get("language_code")
        if language_code is not None and (
            not isinstance(language_code, str)
            or not re.fullmatch(
                r"[A-Za-z]{2,3}(?:[-_][A-Za-z0-9]{2,8})*", language_code
            )
        ):
            raise ValueError(
                "params.language_code must be an ISO-639 code or BCP-47 tag"
            )

        secondary_languages = self.params.get("secondary_languages")
        if secondary_languages is not None and (
            not isinstance(secondary_languages, list)
            or any(
                not isinstance(code, str)
                or not re.fullmatch(r"[A-Za-z]{2,3}", code)
                for code in secondary_languages
            )
        ):
            raise ValueError(
                "params.secondary_languages must be a list of ISO "
                "language codes"
            )

        keyterms = self.params.get("keyterms")
        if keyterms is not None and (
            not isinstance(keyterms, list)
            or len(keyterms) > 50
            or any(
                not isinstance(term, str) or len(term) > 20 for term in keyterms
            )
        ):
            raise ValueError(
                "params.keyterms must contain at most 50 strings of 20 "
                "characters"
            )

        if self.params.get("filter_background_audio") and self.params.get(
            "include_timestamps"
        ):
            raise ValueError(
                "params.filter_background_audio cannot be combined with "
                "params.include_timestamps"
            )

        keepalive = self.params.get("keepalive_interval_ms")
        if keepalive is not None and (
            not isinstance(keepalive, int)
            or isinstance(keepalive, bool)
            or not 500 <= keepalive <= 10000
        ):
            raise ValueError(
                "params.keepalive_interval_ms must be between 500 and 10000"
            )

        vad_silence = self.params.get("vad_silence_threshold_secs")
        if vad_silence is not None and (
            not isinstance(vad_silence, (int, float))
            or isinstance(vad_silence, bool)
            or not 0.192 <= vad_silence <= 3.0
        ):
            raise ValueError(
                "params.vad_silence_threshold_secs must be between "
                "0.192 and 3.0"
            )

        for timeout_name in ("connect_timeout", "finalize_timeout"):
            timeout = self.params.get(timeout_name)
            if (
                not isinstance(timeout, (int, float))
                or isinstance(timeout, bool)
                or timeout <= 0
            ):
                raise ValueError(f"params.{timeout_name} must be positive")

        commit_strategy = self.params.get("commit_strategy")
        if commit_strategy not in SUPPORTED_COMMIT_STRATEGIES:
            raise ValueError(
                "params.commit_strategy must be one of "
                f"{sorted(SUPPORTED_COMMIT_STRATEGIES)}"
            )

    def update(self, params: dict[str, Any]) -> None:
        """Merge vendor parameters without dropping pass-through fields."""
        self.params.update(params)

    def to_json(self, sensitive_handling: bool = True) -> str:
        """Serialize config for logs, redacting the API key by default."""
        config = deepcopy(self)
        if sensitive_handling:
            for key in ("api_key", "token"):
                if config.params.get(key):
                    config.params[key] = encrypt(config.params[key])
            if config.params.get("ws_url"):
                config.params["ws_url"] = redact_url(config.params["ws_url"])
        return config.model_dump_json()

    def to_str(self, sensitive_handling: bool = True) -> str:
        return self.to_json(sensitive_handling=sensitive_handling)

    @staticmethod
    def to_provider_language_code(language_code: str) -> str:
        """Convert a public language tag to ElevenLabs' ISO language code."""
        return language_code.strip().replace("_", "-").split("-", 1)[0].lower()

    @staticmethod
    def normalize_language_code(language_code: str | None) -> str:
        """Normalize provider or public language tags for TEN results."""
        if not language_code:
            return "und"
        language_code = language_code.strip().replace("_", "-")
        parts = language_code.split("-")
        base = parts[0].lower()
        if len(parts) > 1:
            # Preserve a caller/provider supplied region while making the
            # emitted TEN language stable (for example en-us -> en-US).
            return "-".join([base, parts[1].upper(), *parts[2:]])
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
            "hi": "hi-IN",
            "ar": "ar-AE",
        }
        return language_map.get(base, language_code)

    @property
    def normalized_language(self) -> str:
        """Return the configured language in TEN's display form."""
        return self.normalize_language_code(self.params.get("language_code"))
