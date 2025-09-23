from typing import Any, Dict, List

from pydantic import BaseModel, Field
from ten_ai_base import utils


class AliyunTTSPythonConfig(BaseModel):

    token: str = "e5d325b22f514766b6e30ec536e8a801"  # token
    appkey: str = "HBd1B2RCzVuJrA9W"
    url: str = "wss://nls-gateway-cn-beijing.aliyuncs.com/ws/v1"
    sample_rate: int = 16000
    dump: bool = False
    dump_path: str = ""
    enable_words: bool = False
    params: Dict[str, Any] = Field(default_factory=dict)
    black_list_params: List[str] = Field(default_factory=list)

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params

    def update_params(self) -> None:
        ##### get value from params #####
        if "token" in self.params:
            self.token = self.params["token"]
            del self.params["token"]

        if "appkey" in self.params:
            self.appkey = self.params["appkey"]
            del self.params["appkey"]

        if "url" in self.params:
            self.url = self.params["url"]
            del self.params["url"]

        if "sample_rate" in self.params:
            self.sample_rate = int(self.params["sample_rate"])
        else:
            self.params["sample_rate"] = self.sample_rate

        if "enable_words" in self.params:
            self.enable_words = self.params["enable_words"]
            del self.params["enable_words"]

        # ##### use fixed value #####
        # if self.enable_words:
        #     # TODO: auto set subtitle_enable and subtitle_type if enable_words is True
        #     if "enable_subtitle" not in self.params:
        #         self.params["enable_subtitle"] = True

        if "format" not in self.params:
            self.params["format"] = "pcm"

    def validate_params(self) -> None:
        """Validate required configuration parameters."""
        required_fields = ["token", "appkey"]

        for field_name in required_fields:
            value = getattr(self, field_name)
            if not value or (isinstance(value, str) and value.strip() == ""):
                raise ValueError(
                    f"required fields are missing or empty: params.{field_name}"
                )

    def to_str(self, sensitive_handling: bool = False) -> str:
        if not sensitive_handling:
            return f"{self}"
        config = self.copy(deep=True)
        if config.token:
            config.token = utils.encrypt(config.token)
        return f"{config}"

    def get_voice_ids(self) -> str:
        if not self.params:
            return ""

        if "voice" in self.params:
            return self.params["voice"]

        return ""
