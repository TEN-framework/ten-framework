from typing import Any
from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt
from .const import (
    FINALIZE_MODE_MUTE_PKG,
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
    api_key: str = ""
    api_url: str = "wss://openspeech.bytedance.com/api/v2/asr"
    cluster: str = "volcengine_streaming_common"
    language: str = "zh-CN"
    auth_method: str = "token"  # "token" or "signature" or "api_key"
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

    # Finalize configuration (distinct from volcano ASR VAD parameters)
    silence_pkg_length_ms: str = (
        "800"  # Length of silence audio packet to add during finalize (milliseconds, string format)
    )

    def is_black_list_params(self, key: str) -> bool:
        """Check if a parameter key is in the blacklist."""
        return key in list(self.black_list_params)

    def update(self, params: dict[str, Any]):
        """Update configuration with provided parameters."""
        for key, value in params.items():
            if hasattr(self, key):
                setattr(self, key, value)

    def ensure_user_field(self, default_uid: str = "default_user") -> None:
        # Ensure user field exists in params with required uid field.

        # Ensure params is a dict
        if not isinstance(self.params, dict):
            self.params = {}

        if "user" not in self.params:
            self.params["user"] = {"uid": default_uid}
            return

        if not isinstance(self.params["user"], dict):
            self.params["user"] = {"uid": default_uid}
            return

        if "uid" not in self.params["user"]:
            self.params["user"]["uid"] = default_uid
            return

    def ensure_request_field(self) -> None:
        # Ensure request field exists in params with required sequence field.
        # Ensure params is a dict
        if not isinstance(self.params, dict):
            self.params = {}

        # Default value for required field
        default_sequence = 1  # Required field, default value

        if "request" not in self.params:
            self.params["request"] = {"sequence": default_sequence}
            return

        if not isinstance(self.params["request"], dict):
            self.params["request"] = {"sequence": default_sequence}
            return

        if "sequence" not in self.params["request"]:
            self.params["request"]["sequence"] = default_sequence

    def ensure_audio_field(self) -> None:
        # Ensure audio field exists in params with required format field.
        # Ensure params is a dict
        if not isinstance(self.params, dict):
            self.params = {}

        # Default value for required field
        default_format = "raw"  # Required field, default value

        if "audio" not in self.params:
            self.params["audio"] = {"format": default_format}
            return

        if not isinstance(self.params["audio"], dict):
            self.params["audio"] = {"format": default_format}
            return

        if "format" not in self.params["audio"]:
            self.params["audio"]["format"] = default_format

    def to_json(self, sensitive_handling: bool = False) -> str:
        """Convert configuration to JSON string with optional sensitive data handling."""
        if not sensitive_handling:
            return self.model_dump_json()

        config = self.model_copy(deep=True)
        if config.appid:
            config.appid = encrypt(config.appid)

        if config.token:
            config.token = encrypt(config.token)

        if config.api_key:
            config.api_key = encrypt(config.api_key)

        if config.params:
            # Guard for static analyzers: ensure dict semantics for params
            params_dict: dict[str, Any] = (
                dict(config.params) if isinstance(config.params, dict) else {}
            )
            for key, value in params_dict.items():
                if key == "appid":
                    params_dict[key] = encrypt(value)

                if key == "token":
                    params_dict[key] = encrypt(value)

                if key == "api_key":
                    params_dict[key] = encrypt(value)
            config.params = params_dict

        return config.model_dump_json()
