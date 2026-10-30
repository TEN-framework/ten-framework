from typing import Any
import copy
from pathlib import Path
from pydantic import Field
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


# BreezeBlue TTS defaults.
DEFAULT_BASE_URL = "https://api.breeze.blue"
# HTTP streaming emits raw 16-bit little-endian PCM, 24 kHz mono, which is
# exactly what the TEN `pcm_frame` contract expects (no conversion needed).
DEFAULT_OUTPUT_FORMAT = "pcm"
# Per-request character cap. 1000 by default; approved accounts may raise it
# (to at most 2000), in which case set `max_text_length` to match.
DEFAULT_MAX_TEXT_LENGTH = 1000

# The BreezeBlue request schema is `additionalProperties: false`, so unknown
# fields are rejected with a 422. Only these keys are ever forwarded.
BODY_FIELDS = ("model_id", "language_code", "instructions")
VOICE_SETTING_FIELDS = (
    "guidance_scale",
    "speed",
    "volume",
    "use_speaker_boost",
)


class BreezeBlueTTSConfig(AsyncTTS2HttpConfig):
    """BreezeBlue TTS Config"""

    dump: bool = Field(default=False, description="BreezeBlue TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(
            Path(__file__).parent / "breezeblue_tts_in.pcm"
        ),
        description="BreezeBlue TTS dump path",
    )
    url: str | None = Field(
        default=None,
        description="Full streaming endpoint URL (takes precedence over base_url)",
    )
    headers: dict[str, Any] = Field(
        default_factory=dict, description="BreezeBlue TTS headers"
    )
    params: dict[str, Any] = Field(
        default_factory=dict, description="BreezeBlue TTS params"
    )
    max_text_length: int = Field(
        default=DEFAULT_MAX_TEXT_LENGTH,
        description="Longer text is split into several requests",
    )

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # The playground adds sample_rate to every TTS configuration, but the
        # BreezeBlue request schema rejects unknown fields. The output rate is
        # fixed at 24 kHz and declared by the extension, so drop it.
        self.params.pop("sample_rate", None)  # pylint: disable=no-member

        # `max_text_length` may also arrive in params (the common TEN style).
        if (
            "max_text_length" in self.params
        ):  # pylint: disable=unsupported-membership-test
            self.max_text_length = int(
                self.params.pop("max_text_length")  # pylint: disable=no-member
            )

        # Build the endpoint URL unless an explicit one was given. The voice is
        # part of the path, so it must be known here.
        if not self.url:
            if "url" in self.params:
                self.url = self.params.pop("url")  # pylint: disable=no-member
            else:
                base_url = self.params.get(  # pylint: disable=no-member
                    "base_url", DEFAULT_BASE_URL
                ).rstrip("/")
                voice_id = self.params.get("voice_id")
                # Without a voice there is no valid endpoint; leave the URL
                # unset so validate() reports the missing voice_id.
                if voice_id:
                    self.url = f"{base_url}/v1/text-to-speech/{voice_id}/stream"
        self.params.pop("base_url", None)  # pylint: disable=no-member

    def build_request_body(self, text: str) -> dict[str, Any]:
        """Build the JSON body from the whitelisted params."""
        body: dict[str, Any] = {"text": text}
        for key in BODY_FIELDS:
            if self.params.get(key):  # pylint: disable=no-member
                body[key] = self.params[
                    key
                ]  # pylint: disable=unsubscriptable-object

        # Voice settings may be given nested (`voice_settings`) or flat
        # (`speed`, `volume`, ...) like other TEN TTS extensions. Flat keys win.
        settings = dict(
            self.params.get("voice_settings") or {}
        )  # pylint: disable=no-member
        for key in VOICE_SETTING_FIELDS:
            if (
                key in self.params
            ):  # pylint: disable=unsupported-membership-test
                settings[key] = self.params[
                    key
                ]  # pylint: disable=unsubscriptable-object
        settings = {
            k: v for k, v in settings.items() if k in VOICE_SETTING_FIELDS
        }
        if settings:
            body["voice_settings"] = settings
        return body

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
        """Validate BreezeBlue-specific configuration."""
        # pylint: disable=no-member
        has_api_key = bool(self.params.get("api_key"))
        has_auth_header = bool(
            self.headers.get("xi-api-key") or self.headers.get("Authorization")
        )
        if not has_api_key and not has_auth_header:
            raise ValueError("API key is required for BreezeBlue TTS")
        # Unlike Mistral there is no server-side default voice: the voice is
        # part of the URL path, and voices are account-specific.
        if not self.url and not self.params.get("voice_id"):
            raise ValueError("voice_id is required for BreezeBlue TTS")
        if not 1 <= self.max_text_length <= 2000:
            raise ValueError("max_text_length must be between 1 and 2000")
