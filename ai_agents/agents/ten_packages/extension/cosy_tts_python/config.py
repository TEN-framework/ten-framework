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


class CosyTTSConfig(BaseModel):
    # Cosy TTS credentials
    api_key: str = ""  # Cosy TTS API Key

    # TTS specific configs
    model: str = "cosyvoice-v1"  # Model name
    sample_rate: int = 16000  # Audio sample rate
    voice: str = "longxiaochun"  # Voice name

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
        if config.api_key:
            config.api_key = mask_sensitive_data(config.api_key)
        if config.params and "api_key" in config.params:
            config.params["api_key"] = mask_sensitive_data(
                config.params["api_key"]
            )

        return f"{config}"

    def update_params(self) -> None:
        """Update config attributes from params dictionary."""
        param_names = [
            "api_key",
            "model",
            "sample_rate",
            "voice",
        ]

        for param_name in param_names:
            if param_name in self.params and not self.is_black_list_params(
                param_name
            ):
                setattr(self, param_name, self.params[param_name])

    def validate_params(self) -> None:
        """Validate required configuration parameters."""
        required_fields = [
            "api_key",
        ]

        for field_name in required_fields:
            value = getattr(self, field_name)
            if not value or (isinstance(value, str) and value.strip() == ""):
                raise ValueError(
                    f"required fields are missing or empty: params.{field_name}"
                )
