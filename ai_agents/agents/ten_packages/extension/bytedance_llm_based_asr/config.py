from typing import Any
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
        "310"  # Model version: "310" (default) or "400" (better ITN performance)
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

    # Params field for property.json compatibility
    params: dict[str, Any] = Field(default_factory=dict)  # type: ignore

    def get_audio_config(self) -> dict[str, Any]:  # type: ignore
        """Get audio configuration for ASR request."""
        return {
            "format": self.audio_format,
            "codec": self.codec,
            "rate": self.sample_rate,
            "bits": self.bits,
            "channel": self.channel,
        }

    def get_request_config(self) -> dict[str, Any]:  # type: ignore
        """Get request configuration for ASR."""
        return {
            "model_name": self.model_name,
            "model_version": self.model_version,
            "enable_itn": self.enable_itn,
            "enable_punc": self.enable_punc,
            "enable_ddc": self.enable_ddc,
            "enable_nonstream": self.enable_nonstream,
            "show_utterances": self.show_utterances,
            "result_type": "single",
            "end_window_size": self.end_window_size,
        }

    def get_user_config(self) -> dict[str, Any]:  # type: ignore
        """Get user configuration for ASR."""
        return {"uid": self.user_uid}

    def update(self, params: dict[str, Any]) -> None:  # type: ignore
        """Update configuration with params from property.json."""
        for key, value in params.items():
            if hasattr(self, key):
                setattr(self, key, value)  # type: ignore

    def to_json(self, sensitive_handling: bool = False) -> str:
        """Convert configuration to JSON string with optional sensitive data handling."""
        if not sensitive_handling:
            return self.model_dump_json()

        config = self.model_copy(deep=True)
        if config.app_key:
            config.app_key = encrypt(config.app_key)
        if config.access_key:
            config.access_key = encrypt(config.access_key)

        params_dict = config.params
        if params_dict:
            encrypted_params: dict[str, Any] = {}  # type: ignore
            for key, value in params_dict.items():
                if key in ["app_key", "access_key"] and isinstance(value, str):
                    encrypted_params[key] = encrypt(value)  # type: ignore
                else:
                    encrypted_params[key] = value  # type: ignore
            config.params = encrypted_params

        return config.model_dump_json()
