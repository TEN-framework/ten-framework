from typing import Any, Dict, List
from pydantic import BaseModel, Field
from ten_ai_base import utils


class MinimaxTTSWebsocketConfig(BaseModel):
    # Minimax TTS credentials
    api_key: str = ""
    group_id: str = ""

    # Minimax TTS specific configs
    url: str = "wss://api.minimaxi.com/ws/v1/t2a_v2"
    sample_rate: int = 16000
    channels: int = 1  # channels

    # Minimax TTS pass through parameters
    params: Dict[str, Any] = Field(default_factory=dict)
    # Black list parameters, will be removed from params
    black_list_keys: List[str] = Field(default_factory=list)

    # Debug and dump settings
    dump: bool = False
    dump_path: str = "/tmp"
    enable_words: bool = False

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_keys

    def update_params(self) -> None:
        ##### get value from params #####
        if "api_key" in self.params:
            self.api_key = self.params["api_key"]
            del self.params["api_key"]

        if "group_id" in self.params:
            self.group_id = self.params["group_id"]
            del self.params["group_id"]

        if "url" in self.params:
            self.url = self.params["url"]
            del self.params["url"]

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

    def validate_params(self) -> None:
        """Validate required configuration parameters."""
        required_fields = ["api_key", "group_id"]

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
        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)
        return f"{config}"
