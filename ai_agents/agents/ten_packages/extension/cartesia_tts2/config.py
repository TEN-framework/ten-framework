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


class CartesiaTTSConfig(BaseModel):
    api_key: str = ""

    sample_rate: int = 16000
    dump: bool = False
    dump_path: str = "/tmp"
    params: Dict[str, Any] = Field(default_factory=dict)

    def update_params(self) -> None:
        # Remove params that are not used
        if "transcript" in self.params:
            del self.params["transcript"]

        if "api_key" in self.params:
            self.api_key = self.params["api_key"]
            del self.params["api_key"]

        # Remove params that are not used
        if "context_id" in self.params:
            del self.params["context_id"]

        # Remove params that are not used
        if "stream" in self.params:
            del self.params["stream"]

        # Use default sample rate value
        if "sample_rate" in self.params:
            self.sample_rate = self.params["sample_rate"]
            # Remove sample_rate from params to avoid parameter error
            del self.params["sample_rate"]

        if "output_format" not in self.params:
            self.params["output_format"] = {}

        # Use custom sample rate value
        if "sample_rate" in self.params["output_format"]:
            self.sample_rate = self.params["output_format"]["sample_rate"]
        else:
            self.params["output_format"]["sample_rate"] = self.sample_rate

        ##### use fixed value #####
        self.params["output_format"]["container"] = "raw"
        self.params["output_format"]["encoding"] = "pcm_s16le"

    def to_str(self) -> str:
        """
        Convert the configuration to a string representation, masking sensitive data.
        """
        return (
            f"CartesiaTTSConfig(api_key={mask_sensitive_data(self.api_key)}, "
            f"sample_rate={self.sample_rate}, "
            f"dump={self.dump}, "
            f"dump_path={self.dump_path}, "
            f"params={self.params}, "
        )
