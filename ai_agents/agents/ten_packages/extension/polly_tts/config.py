from typing import Any
import copy
from pydantic import Field
from pathlib import Path
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


class PollyTTSConfig(AsyncTTS2HttpConfig):
    """Amazon Polly TTS Config"""

    dump: bool = Field(default=False, description="Amazon Polly TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent / "polly_tts_in.pcm"),
        description="Amazon Polly TTS dump path",
    )
    params: dict[str, Any] = Field(
        default_factory=dict, description="Amazon Polly TTS params"
    )

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # No cleanup needed - all params are valid for Polly

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Convert config to string with optional sensitive data handling."""
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        # Encrypt sensitive fields in params
        if config.params:
            for key in [
                "aws_access_key_id",
                "aws_secret_access_key",
                "aws_session_token",
            ]:
                if key in config.params:
                    config.params[key] = utils.encrypt(config.params[key])

        return f"{config}"

    def validate(self) -> None:
        """Validate Polly-specific configuration."""
        if (
            "aws_access_key_id" not in self.params
            or not self.params["aws_access_key_id"]
        ):
            raise ValueError(
                "aws_access_key_id is required for Amazon Polly TTS"
            )
        if (
            "aws_secret_access_key" not in self.params
            or not self.params["aws_secret_access_key"]
        ):
            raise ValueError(
                "aws_secret_access_key is required for Amazon Polly TTS"
            )
