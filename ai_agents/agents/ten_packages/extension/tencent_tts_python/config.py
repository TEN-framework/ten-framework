import copy
from pydantic import BaseModel, Field
from typing import Any


def mask_sensitive_data(
    s: str, unmasked_start: int = 5, unmasked_end: int = 5, mask_char: str = "*"
) -> str:
    """
    Mask a sensitive string by replacing the middle part with asterisks.

    Parameters:
        s (str): The input string (e.g., API key).
        unmasked_start (int): Number of visible characters at the beginning.
        unmasked_end (int): Number of visible characters at the end.
        mask_char (str): Character used for masking.

    Returns:
        str: Masked string, e.g., "abc****xyz"
    """
    if not s or len(s) <= unmasked_start + unmasked_end:
        return mask_char * len(s)

    return (
        s[:unmasked_start]
        + mask_char * (len(s) - unmasked_start - unmasked_end)
        + s[-unmasked_end:]
    )


# Docs: https://cloud.tencent.com/document/product/1073/108595
class TencentTTSConfig(BaseModel):
    # Tencent Cloud credentials
    app_id: str  # Tencent Cloud App ID
    secret_key: str  # Tencent Cloud Secret Key
    secret_id: str  # Tencent Cloud Secret ID

    # TTS specific configs
    codec: str = "pcm"  # Audio codec
    emotion_category: str = ""  # Emotion category
    emotion_intensity: int = 0  # Emotion intensity
    enable_words: bool = False  # Enable word-level timing
    sample_rate: int = 24000  # Audio sample rate
    speed: float = 0  # Speed range [-2.0, 6.0]
    voice_type: str = "0"  # Voice type ID
    volume: float = 0  # Volume range [-10, 10]

    # Debug and dump settings
    dump: bool = False
    dump_path: str = "/tmp"

    # Parameters
    params: dict[str, Any] = Field(default_factory=dict)

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Convert config to string with optional sensitive data handling."""
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        # Encrypt sensitive fields
        if config.secret_key:
            config.secret_key = mask_sensitive_data(config.secret_key)
        if config.params and "secret_key" in config.params:
            config.params["secret_key"] = mask_sensitive_data(
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
            if param_name in self.params:
                setattr(self, param_name, self.params[param_name])

    def validate_params(self) -> None:
        """Validate required configuration parameters."""
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
