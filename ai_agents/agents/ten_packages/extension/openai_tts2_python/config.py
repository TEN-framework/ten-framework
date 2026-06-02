from typing import Any
import copy
from pathlib import Path
from pydantic import Field
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


class OpenAITTSConfig(AsyncTTS2HttpConfig):
    """OpenAI TTS Config"""

    dump: bool = Field(default=False, description="OpenAI TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(
            Path(__file__).parent / "openai_tts_in.pcm"
        ),
        description="OpenAI TTS dump path",
    )
    url: str | None = Field(
        default=None,
        description="Direct endpoint URL (takes precedence over base_url)",
    )
    headers: dict[str, Any] = Field(
        default_factory=dict, description="OpenAI TTS headers"
    )
    params: dict[str, Any] = Field(
        default_factory=dict, description="OpenAI TTS params"
    )

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # Set default values if not specified
        if "model" not in self.params:
            self.params["model"] = "gpt-4o-mini-tts"
        if "voice" not in self.params:
            self.params["voice"] = "coral"
        if "speed" not in self.params:
            self.params["speed"] = 1.0
        if "instructions" not in self.params:
            self.params["instructions"] = ""

        # Remove input if present (will be set from text)
        if "input" in self.params:
            del self.params["input"]

        # Use fixed value
        self.params["response_format"] = "pcm"

        # Set endpoint URL from base_url if url is not provided
        if not self.url:
            if "url" in self.params:
                self.url = self.params["url"]
                self.params.pop("url", None)  # pylint: disable=no-member
            else:
                base_url = self.params.get(  # pylint: disable=no-member
                    "base_url", "https://api.openai.com/v1"
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
        """Validate OpenAI-specific configuration."""
        # Check if API key is provided in params or Authorization header
        has_api_key_in_params = (
            "api_key" in self.params and self.params["api_key"]
        )
        # pylint: disable=no-member
        has_authorization_header = self.headers.get("Authorization") is not None
        if not has_api_key_in_params and not has_authorization_header:
            raise ValueError(
                "API key or Authorization header is required for OpenAI TTS"
            )
        if "model" not in self.params or not self.params["model"]:
            raise ValueError("Model is required for OpenAI TTS")
        if "voice" not in self.params or not self.params["voice"]:
            raise ValueError("Voice is required for OpenAI TTS")
