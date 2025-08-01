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


class BytedanceTTSDuplexConfig(BaseModel):
    appid: str
    token: str

    # Refer to: https://www.volcengine.com/docs/6561/1257544.
    voice_type: str = "zh_female_shuangkuaisisi_moon_bigtts"
    sample_rate: int = 24000
    api_url: str = "wss://openspeech.bytedance.com/api/v3/tts/bidirection"
    dump: bool = False
    dump_path: str = "/tmp"
    params: Dict[str, Any] = Field(default_factory=dict)
    enable_words: bool = False

    def update_params(self) -> None:
        ##### get value from params #####
        if (
            "audio_params" in self.params
            and "sample_rate" in self.params["audio_params"]
        ):
            self.sample_rate = int(self.params["audio_params"]["sample_rate"])

        if (
            "audio_params" not in self.params
            or "sample_rate" not in self.params["audio_params"]
        ):
            if "audio_params" not in self.params:
                self.params["audio_params"] = {}
            self.params["audio_params"]["sample_rate"] = self.sample_rate

        ##### use fixed value #####
        if "audio_params" not in self.params:
            self.params["audio_params"] = {}
        self.params["audio_params"]["format"] = "pcm"

    def to_str(self) -> str:
        """
        Convert the configuration to a string representation, masking sensitive data.
        """
        return (
            f"BytedanceTTSDuplexConfig(appid={self.appid}, "
            f"token={mask_sensitive_data(self.token)}, "
            f"voice_type={self.voice_type}, "
            f"sample_rate={self.sample_rate}, "
            f"api_url={self.api_url}, "
            f"dump={self.dump}, "
            f"dump_path={self.dump_path}, "
            f"params={self.params}, "
            f"enable_words={self.enable_words})"
        )
