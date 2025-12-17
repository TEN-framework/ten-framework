from typing import Any, Optional
from pydantic import BaseModel, Field, ConfigDict, model_validator
from ten_ai_base.utils import encrypt  # type: ignore


class BytedanceASRLLMConfig(BaseModel):
    """Volcengine ASR LLM Configuration

    Configuration for Volcengine ASR Large Language Model service.
    Refer to: https://www.volcengine.com/docs/6561/1354869

    All configuration values should be provided via params dict.
    """

    # Pydantic configuration to disable protected namespace warnings
    model_config = ConfigDict(protected_namespaces=())

    # Core configuration fields
    params: dict[str, Any] = Field(default_factory=dict)
    dump: bool = False
    dump_path: str = "."
    language: str = "zh-CN"

    @model_validator(mode="after")
    def override_language_from_params(self) -> "BytedanceASRLLMConfig":
        """Override language field if provided in params."""
        if "language" in self.params:
            self.language = self.params["language"]
        return self

    def get_audio_config(self) -> dict[str, Any]:
        """Get audio configuration for ASR request.

        Must be provided via params.audio.
        Raises ValueError if not provided.
        """
        if "audio" not in self.params:
            raise ValueError(
                "Missing required parameter: audio must be provided in params."
            )
        return self.params["audio"]

    def get_enable_utterance_grouping(self) -> bool:
        """Get enable utterance grouping from params."""
        return self.params.get("enable_utterance_grouping", True)

    def get_request_config(self) -> dict[str, Any]:
        """Get request configuration for ASR.

        Must be provided via params.request.
        Raises ValueError if not provided.
        Sets default values for missing fields:
        - enable_nonstream: true
        - end_window_size: 800
        - model_name: "bigmodel"
        - result_type: "single"
        - show_utterances: true
        """
        if "request" not in self.params:
            raise ValueError(
                "Missing required parameter: request must be provided in params."
            )

        request_config = self.params["request"].copy()

        # Set default values for missing fields
        defaults = {
            "enable_nonstream": True,
            "end_window_size": 800,
            "model_name": "bigmodel",
            "result_type": "single",
            "show_utterances": True,
        }

        # Apply defaults only for missing keys
        for key, default_value in defaults.items():
            if key not in request_config:
                request_config[key] = default_value

        return request_config

    def get_user_config(self) -> Optional[dict[str, Any]]:
        """Get user configuration for ASR.

        Returns params.user if provided, otherwise None.
        """
        user = self.params.get("user")
        return user if user else None

    def get_sample_rate(self) -> int:
        """Get sample rate from params.audio."""
        audio_config = self.get_audio_config()
        return audio_config.get("rate", 16000)

    def get_bits(self) -> int:
        """Get bits from params.audio."""
        audio_config = self.get_audio_config()
        return audio_config.get("bits", 16)

    def get_channel(self) -> int:
        """Get channel from params.audio."""
        audio_config = self.get_audio_config()
        return audio_config.get("channel", 1)

    def get_segment_duration_ms(self) -> int:
        """Get audio segment duration in milliseconds."""
        return self.params.get("segment_duration_ms", 100)

    def get_mute_pkg_duration_ms(self) -> int:
        """Get mute package duration in milliseconds."""
        return self.params.get("mute_pkg_duration_ms", 800)

    def get_resource_id(self) -> str:
        """Get resource ID."""
        return self.params.get("resource_id", "volc.bigasr.sauc.duration")

    def get_api_url(self) -> str:
        """Get API URL."""
        return self.params.get(
            "api_url",
            "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async",
        )

    def get_app_key(self) -> str:
        """Get app key."""
        return self.params.get("app_key", "")

    def get_access_key(self) -> str:
        """Get access key."""
        return self.params.get("access_key", "")

    def get_api_key(self) -> str:
        """Get API key."""
        return self.params.get("api_key", "")

    def get_auth_method(self) -> str:
        """Get authentication method."""
        return self.params.get("auth_method", "token")

    def to_json(self, sensitive_handling: bool = False) -> str:
        """Convert configuration to JSON string with optional sensitive data handling."""
        if not sensitive_handling:
            return self.model_dump_json()

        config = self.model_copy(deep=True)

        # Encrypt sensitive fields in params
        if config.params:
            encrypted_params: dict[str, Any] = {}
            for key, value in config.params.items():
                if key in ["app_key", "access_key", "api_key"] and isinstance(
                    value, str
                ):
                    encrypted_params[key] = encrypt(value)
                else:
                    encrypted_params[key] = value
            config.params = encrypted_params

        return config.model_dump_json()
