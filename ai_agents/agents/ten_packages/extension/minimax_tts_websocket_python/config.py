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

    api_key: str = ""
    group_id: str = ""
    url: str = "wss://api.minimaxi.com/ws/v1/t2a_v2"
    sample_rate: int = 16000
    channels: int = 1
    dump: bool = False
    dump_path: str = ""
    params: Dict[str, Any] = Field(default_factory=dict)
    black_list_params: List[str] = Field(default_factory=list)

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params

    def update_params(self) -> None:
        ##### get value from params #####
        if "api_key" in self.params:
            self.api_key = self.params["api_key"]
            del self.params["api_key"]

        if "group_id" in self.params:
            self.group_id = self.params["group_id"]
            del self.params["group_id"]

        if (
            "audio_setting" in self.params
            and "sample_rate" in self.params["audio_setting"]
        ):
            self.sample_rate = int(self.params["audio_setting"]["sample_rate"])

        if (
            "audio_setting" in self.params
            and "channels" in self.params["audio_setting"]
        ):
            self.channels = int(self.params["audio_setting"]["channels"])

        ##### use fixed value #####
        if "audio_setting" not in self.params:
            self.params["audio_setting"] = {}
        self.params["audio_setting"]["format"] = "pcm"

    def to_str(self) -> str:
        """
        Convert the configuration to a string representation, masking sensitive data.
        """
        return (
            f"MinimaxTTSWebsocketConfig(key={mask_sensitive_data(self.api_key)}, "
            f"group_id={self.group_id}, "
            f"url={self.url}, "
            f"sample_rate={self.sample_rate}, "
            f"channels={self.channels}, "
            f"dump={self.dump}, "
            f"dump_path={self.dump_path}, "
            f"params={self.params}, "
            f"black_list_params={self.black_list_params})"
        )
