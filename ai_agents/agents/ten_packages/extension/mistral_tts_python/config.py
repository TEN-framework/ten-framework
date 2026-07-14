from typing import Any
import copy
from pathlib import Path
from pydantic import Field
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


# Mistral (Voxtral) TTS defaults.
DEFAULT_BASE_URL = "https://api.mistral.ai/v1"
DEFAULT_MODEL = "voxtral-mini-tts-2603"
# Voxtral can emit pcm/wav/flac/mp3/aac/opus. We request the raw `pcm` stream
# (lowest latency: no container header to buffer before the first audio) and
# convert it to PCM16 mono on the fly. Mistral's `pcm` is headerless float32 LE
# at 24 kHz, which is NOT the PCM16 the TEN `pcm_frame` contract expects, so the
# client rescales each float32 sample to int16 (see Float32ToPcm16).
DEFAULT_RESPONSE_FORMAT = "pcm"


class MistralTTSConfig(AsyncTTS2HttpConfig):
    """Mistral (Voxtral) TTS Config"""

    dump: bool = Field(default=False, description="Mistral TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(
            Path(__file__).parent / "mistral_tts_in.pcm"
        ),
        description="Mistral TTS dump path",
    )
    url: str | None = Field(
        default=None,
        description="Direct endpoint URL (takes precedence over base_url)",
    )
    headers: dict[str, Any] = Field(
        default_factory=dict, description="Mistral TTS headers"
    )
    params: dict[str, Any] = Field(
        default_factory=dict, description="Mistral TTS params"
    )

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # The playground adds sample_rate to every TTS configuration, but the
        # Mistral speech endpoint rejects it as an unknown request field.  The
        # output rate is fixed at 24 kHz and is declared by the extension, so
        # this generic frontend setting must not be forwarded to the vendor.
        self.params.pop("sample_rate", None)  # pylint: disable=no-member

        # Set default values if not specified
        if "model" not in self.params:
            self.params["model"] = DEFAULT_MODEL

        # Remove input if present (will be set from text)
        if "input" in self.params:
            del self.params["input"]

        # Always request the raw float32 `pcm` stream; the client rescales it
        # to PCM16 mono (the format Voxtral's `pcm` actually emits).
        self.params["response_format"] = DEFAULT_RESPONSE_FORMAT

        # Set endpoint URL from base_url if url is not provided
        if not self.url:
            if "url" in self.params:
                self.url = self.params["url"]
                self.params.pop("url", None)  # pylint: disable=no-member
            else:
                base_url = self.params.get(  # pylint: disable=no-member
                    "base_url", DEFAULT_BASE_URL
                )
                # Remove trailing slash from base_url
                base_url = base_url.rstrip("/")
                self.url = f"{base_url}/audio/speech"
                # Remove base_url from params since it's been used to set url
                self.params.pop("base_url", None)  # pylint: disable=no-member

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Convert config to string with optional sensitive data handling."""
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        # Encrypt sensitive fields in params
        if config.params and "api_key" in config.params:
            config.params["api_key"] = utils.encrypt(config.params["api_key"])

        return f"{config}"

    def validate(self) -> None:
        """Validate Mistral-specific configuration."""
        # Check if API key is provided in params or Authorization header
        has_api_key_in_params = (
            "api_key" in self.params and self.params["api_key"]
        )
        # pylint: disable=no-member
        has_authorization_header = self.headers.get("Authorization") is not None
        if not has_api_key_in_params and not has_authorization_header:
            raise ValueError(
                "API key or Authorization header is required for Mistral TTS"
            )
        if "model" not in self.params or not self.params["model"]:
            raise ValueError("Model is required for Mistral TTS")
        # `voice` / `voice_id` is intentionally not required here: Voxtral
        # accepts a preset `voice`, a saved `voice_id`, or a one-off `ref_audio`
        # clip, and may fall back to a default voice. Whatever the caller puts
        # in `params` is forwarded to the vendor unchanged.
