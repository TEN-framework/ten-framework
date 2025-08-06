from typing import Any
from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt
from .const import (
    FINALIZE_MODE_MUTE_PKG,
    DEFAULT_WORKFLOW,
)


class BytedanceASRConfig(BaseModel):
    """Bytedance ASR Configuration

    Refer to: https://www.volcengine.com/docs/6561/80818.
    agora_rtc subscribe_audio_samples_per_frame needs to be set to 3200
    according to https://www.volcengine.com/docs/6561/111522
    """

    # Basic ASR configuration
    appid: str = ""
    token: str = ""
    api_url: str = "wss://openspeech.bytedance.com/api/v2/asr"
    cluster: str = "volcengine_streaming_common"
    language: str = "zh-CN"
    workflow: str = DEFAULT_WORKFLOW  # ASR processing workflow

    # Business configuration
    finalize_mode: str = FINALIZE_MODE_MUTE_PKG  # "disconnect" or "mute_pkg"
    finalize_timeout: float = 10.0  # Finalize timeout in seconds

    # Reconnection configuration
    max_retries: int = 5  # Maximum number of reconnection attempts
    base_delay: float = 0.3  # Base delay for exponential backoff (seconds)

    # Extension configuration
    params: dict[str, Any] = Field(default_factory=dict)
    black_list_params: list[str] = Field(default_factory=list)
    dump: bool = False
    dump_path: str = "."

    def is_black_list_params(self, key: str) -> bool:
        """Check if a parameter key is in the blacklist."""
        return key in self.black_list_params

    def update(self, params: dict[str, Any]):
        """Update configuration with provided parameters."""
        for key, value in params.items():
            if hasattr(self, key):
                setattr(self, key, value)

    def to_json(self, sensitive_handling: bool = False) -> str:
        """Convert configuration to JSON string with optional sensitive data handling."""
        if not sensitive_handling:
            return self.model_dump_json()

        config = self.model_copy(deep=True)
        if config.appid:
            config.appid = encrypt(config.appid)

        if config.token:
            config.token = encrypt(config.token)

        if config.params:
            for key, value in config.params.items():
                if key == "appid":
                    config.params[key] = encrypt(value)

                if key == "token":
                    config.params[key] = encrypt(value)

        return config.model_dump_json()
