from typing import Any

from pydantic import BaseModel, Field


def mask_sensitive_data(
    s: str, unmasked_start: int = 3, unmasked_end: int = 3, mask_char: str = "*"
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


class RimeTTSConfig(BaseModel):
    # RIME TTS API credentials
    api_key: str = ""
    # Debug and logging
    dump: bool = False
    dump_path: str = "/tmp"
    sampling_rate: int = 16000
    params: dict[str, Any] = Field(default_factory=dict)
    black_list_keys: list[str] = ["api_key", "text"]

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # Extract API key
        if "api_key" in self.params:
            self.api_key = self.params["api_key"]
            del self.params["api_key"]

        self.params["audioFormat"] = "pcm"

        if "samplingRate" in self.params:
            self.sampling_rate = int(self.params["samplingRate"])
        elif "sampling_rate" in self.params:
            self.sampling_rate = int(self.params["sampling_rate"])

        self.params["segment"] = "immediate"

        # Remove sensitive keys from params
        for key in self.black_list_keys:
            if key in self.params:
                del self.params[key]

    def to_str(self) -> str:
        """
        Convert the configuration to a string representation, masking sensitive data.
        """
        return (
            f"RimeTTSConfig(api_key={mask_sensitive_data(self.api_key)}, "
            f"sampling_rate={self.sampling_rate}, "
            f"params={self.params}, "
            f"dump={self.dump}, "
            f"dump_path={self.dump_path})"
        )
