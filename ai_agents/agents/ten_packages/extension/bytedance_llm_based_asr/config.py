from typing import Any, Optional
from pydantic import BaseModel, Field, ConfigDict
from ten_ai_base.utils import encrypt  # type: ignore


class BytedanceASRLLMConfig(BaseModel):
    """Volcengine ASR LLM Configuration

    Configuration for Volcengine ASR Large Language Model service.
    Refer to: https://www.volcengine.com/docs/6561/1354869
    """

    # Pydantic configuration to disable protected namespace warnings
    model_config = ConfigDict(protected_namespaces=())

    # Authentication
    app_key: str = ""
    access_key: str = ""
    api_key: str = ""
    auth_method: str = "token"

    # API Configuration
    api_url: str = "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async"
    resource_id: str = "volc.bigasr.sauc.duration"

    # Audio Configuration
    sample_rate: int = 16000
    audio_format: str = "pcm"  # Use PCM format for raw audio data
    codec: str = "raw"  # Raw PCM data (default for PCM format)
    bits: int = 16
    channel: int = 1

    # ASR Model Configuration
    model_name: str = "bigmodel"
    model_version: str = (
        "400"  # Model version: "310" (default) or "400" (better ITN performance)
    )
    enable_itn: bool = True  # Enable Inverse Text Normalization
    enable_punc: bool = True  # Enable punctuation
    enable_ddc: bool = True  # Enable speaker diarization
    show_utterances: bool = True
    enable_nonstream: bool = True

    # User Configuration
    user_uid: str = "default_user"

    # Reconnection Configuration
    max_retries: int = 5
    base_delay: float = 0.3

    # Audio Processing
    segment_duration_ms: int = 100  # Audio segment duration in milliseconds
    end_window_size: int = (
        200  # End window size in milliseconds for voice activity detection
    )

    # Extension Configuration
    dump: bool = False
    dump_path: str = "."

    # Language Configuration
    language: str = "zh-CN"

    # Payload for full passthrough mode (optional)
    # If set, user/audio/request in payload will override individual config fields
    # Allows complete control over the full client request parameters
    payload: dict[str, Any] = Field(default_factory=dict)

    # Params field for property.json compatibility
    params: dict[str, Any] = Field(default_factory=dict)

    def get_audio_config(self) -> dict[str, Any]:
        """Get audio configuration for ASR request.

        Must be provided via payload.audio (passthrough mode).
        Raises ValueError if not provided.
        """
        # pylint: disable=no-member
        audio_config = self.payload.get("audio") if self.payload else None
        if not audio_config:
            raise ValueError(
                "Missing required parameter: payload.audio must be provided. "
            )
        return audio_config

    def get_request_config(self) -> dict[str, Any]:
        """Get request configuration for ASR.

        Must be provided via payload.request (passthrough mode).
        Raises ValueError if not provided.
        """
        # pylint: disable=no-member
        request_config = self.payload.get("request") if self.payload else None
        if not request_config:
            raise ValueError(
                "Missing required parameter: payload.request must be provided. "
            )
        return request_config

    def get_user_config(self) -> Optional[dict[str, Any]]:
        """Get user configuration for ASR.

        If payload.user is set, use it directly (passthrough mode).
        Otherwise, build from user_uid field.
        Returns None if user config should not be included in request.
        """
        # pylint: disable=no-member
        user_config = self.payload.get("user") if self.payload else None
        if user_config:
            # Return None for empty dict to omit user field from request
            return user_config if user_config else None

        # For backward compatibility, return uid only if not default
        if self.user_uid and self.user_uid != "default_user":
            return {"uid": self.user_uid}
        return None

    def get_sample_rate(self) -> int:
        """Get sample rate from payload.audio.

        Returns the sample rate configured in payload.
        Raises ValueError if payload.audio is not configured.
        """
        audio_config = self.get_audio_config()
        return audio_config.get("rate", 16000)

    def get_bits(self) -> int:
        """Get bits from payload.audio.

        Returns the bits configured in payload.
        Raises ValueError if payload.audio is not configured.
        """
        audio_config = self.get_audio_config()
        return audio_config.get("bits", 16)

    def get_channel(self) -> int:
        """Get channel from payload.audio.

        Returns the channel configured in payload.
        Raises ValueError if payload.audio is not configured.
        """
        audio_config = self.get_audio_config()
        return audio_config.get("channel", 1)

    def update(self, params: dict[str, Any]) -> None:
        """Update configuration with params from property.json."""
        for key, value in params.items():
            if hasattr(self, key):
                setattr(self, key, value)

    def to_json(self, sensitive_handling: bool = False) -> str:
        """Convert configuration to JSON string with optional sensitive data handling."""
        if not sensitive_handling:
            return self.model_dump_json()

        config = self.model_copy(deep=True)
        if config.app_key:
            config.app_key = encrypt(config.app_key)
        if config.access_key:
            config.access_key = encrypt(config.access_key)
        if config.api_key:
            config.api_key = encrypt(config.api_key)

        params_dict = config.params
        if params_dict:
            encrypted_params: dict[str, Any] = {}
            for key, value in params_dict.items():
                if key in ["app_key", "access_key", "api_key"] and isinstance(
                    value, str
                ):
                    encrypted_params[key] = encrypt(value)
                else:
                    encrypted_params[key] = value
            config.params = encrypted_params

        return config.model_dump_json()
