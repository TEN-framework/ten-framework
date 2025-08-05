from typing import Any, Dict, List

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


class MinimaxTTSWebsocketConfig(BaseModel):

    api_key: str
    group_id: str
    url: str
    voice_id: str
    sample_rate: int
    model: str
    dump: bool = False
    dump_path: str = "/tmp"
    params: Dict[str, Any] = Field(default_factory=dict)
    black_list_params: List[str] = Field(default_factory=list)


    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params

    def update_params(self) -> None:
        ##### get value from params #####
        if (
            "audio_setting" in self.params
            and "sample_rate" in self.params["audio_setting"]
        ):
            self.sample_rate = int(self.params["audio_setting"]["sample_rate"])

        if (
            "audio_setting" not in self.params
            or "sample_rate" not in self.params["audio_setting"]
        ):
            if "audio_setting" not in self.params:
                self.params["audio_setting"] = {}
            self.params["audio_setting"]["sample_rate"] = self.sample_rate

        ##### use fixed value #####
        if "audio_setting" not in self.params:
            self.params["audio_setting"] = {}
        self.params["audio_setting"]["format"] = "pcm"

        if "voice_setting" not in self.params:
            self.params["voice_setting"] = {}
        self.params["voice_setting"]["voice_id"] = self.voice_id

        if "model" not in self.params:
            self.params["model"] = self.model

    def to_str(self) -> str:
        """
        Convert the configuration to a string representation, masking sensitive data.
        """
        return (
            f"MinimaxTTSWebsocketConfig(key={mask_sensitive_data(self.api_key)}, "
            f"group_id={self.group_id}, "
            f"voice_id={self.voice_id}, "
            f"sample_rate={self.sample_rate}, "
            f"url={self.url}, "
            f"dump={self.dump}, "
            f"dump_path={self.dump_path}, "
            f"params={self.params}, "
            f"black_list_params={self.black_list_params})"
        )
