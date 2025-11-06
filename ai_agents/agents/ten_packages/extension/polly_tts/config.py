from typing import Any
import copy
from pydantic import Field
from pathlib import Path
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


class PollyTTSConfig(AsyncTTS2HttpConfig):
    """Amazon Polly TTS Config"""

    # Top-level configuration fields (AWS credentials and session config)
    aws_access_key_id: str = Field(default="", description="AWS Access Key ID")
    aws_secret_access_key: str = Field(default="", description="AWS Secret Access Key")
    aws_session_token: str = Field(default="", description="AWS Session Token")
    region_name: str = Field(default="", description="AWS Region Name")
    profile_name: str = Field(default="", description="AWS Profile Name")
    aws_account_id: str = Field(default="", description="AWS Account ID")

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

        # Encrypt sensitive fields
        if config.aws_access_key_id:
            config.aws_access_key_id = utils.encrypt(config.aws_access_key_id)
        if config.aws_secret_access_key:
            config.aws_secret_access_key = utils.encrypt(config.aws_secret_access_key)
        if config.aws_session_token:
            config.aws_session_token = utils.encrypt(config.aws_session_token)

        return f"{config}"

    def validate(self) -> None:
        """Validate Polly-specific configuration."""
        if not self.aws_access_key_id:
            raise ValueError(
                "aws_access_key_id is required for Amazon Polly TTS"
            )
        if not self.aws_secret_access_key:
            raise ValueError(
                "aws_secret_access_key is required for Amazon Polly TTS"
            )
