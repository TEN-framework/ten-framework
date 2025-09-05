import copy
from pydantic import BaseModel, Field
from typing import Any

from ten_ai_base import utils


# Docs: https://cloud.tencent.com/document/product/1073/108595
class TencentTTSConfig(BaseModel):
    # Tencent Cloud credentials
    app_id: str = ""  # Tencent Cloud App ID
    secret_key: str = ""  # Tencent Cloud Secret Key
    secret_id: str = ""  # Tencent Cloud Secret ID

    # TTS specific configs
    codec: str = "pcm"  # Audio codec
    emotion_category: str = ""  # Emotion category
    emotion_intensity: int = 0  # Emotion intensity
    enable_subtitle: bool = False  # Enable subtitle
    sample_rate: int = 24000  # Audio sample rate
    speed: float = 0  # Speed range [-2.0, 6.0]
    voice_type: int = 0  # Voice type ID
    volume: float = 0  # Volume range [-10, 10]

    # Debug and dump settings
    dump: bool = False
    dump_path: str = "/tmp"

    # Parameters
    # Function reserved, currently empty, may need to add content later
    black_list_params: list[str] = Field(default_factory=list)
    params: dict[str, Any] = Field(default_factory=dict)

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Convert config to string with optional sensitive data handling."""
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        # Encrypt sensitive fields
        if config.secret_key:
            config.secret_key = utils.encrypt(config.secret_key)
        if config.params and "secret_key" in config.params:
            config.params["secret_key"] = utils.encrypt(
                config.params["secret_key"]
            )

        return f"{config}"

    def update_params(self) -> None:
        """Update config attributes from params dictionary."""
        param_names = [
            "app_id",
            "secret_key",
            "secret_id",
            "emotion_category",
            "emotion_intensity",
            "enable_words",
            "sample_rate",
            "speed",
            "voice_type",
            "volume",
        ]

        for param_name in param_names:
            if param_name in self.params and not self.is_black_list_params(
                param_name
            ):
                setattr(self, param_name, self.params[param_name])

    def validate_params(self) -> None:
        """Validate required configuration parameters."""
        if not self.app_id.isdigit():
            raise ValueError(
                f"app_id value must be an integer: params.app_id={self.app_id}"
            )
        required_fields = [
            "app_id",
            "secret_key",
            "secret_id",
        ]

        for field_name in required_fields:
            value = getattr(self, field_name)
            if not value or (isinstance(value, str) and value.strip() == ""):
                raise ValueError(
                    f"required fields are missing or empty: params.{field_name}"
                )
