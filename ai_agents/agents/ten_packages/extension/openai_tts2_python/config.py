from typing import Any, Dict

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


class OpenaiTTSConfig(BaseModel):
    api_key: str = ""

    dump: bool = False
    dump_path: str = "/tmp"
    params: Dict[str, Any] = Field(default_factory=dict)

    # Fixed value, it can not be changed
    # Refer to https://platform.openai.com/docs/api-reference/audio/createSpeech
    sample_rate: int = 24000

    def update_params(self) -> None:
        if "api_key" in self.params:
            self.api_key = self.params["api_key"]
            del self.params["api_key"]

        if "input" in self.params:
            del self.params["input"]

        # Remove sample_rate from params to avoid parameter error
        if "sample_rate" in self.params:
            del self.params["sample_rate"]

        # Use fixed value
        self.params["response_format"] = "pcm"
        self.sample_rate = 24000

    def to_str(self) -> str:
        """
        Convert the configuration to a string representation, masking sensitive data.
        """
        return (
            f"OpenaiTTSConfig(api_key={mask_sensitive_data(self.api_key)}, "
            f"sample_rate={self.sample_rate}, "
            f"dump={self.dump}, "
            f"dump_path={self.dump_path}, "
            f"params={self.params}, "
        )
