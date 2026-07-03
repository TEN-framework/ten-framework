from typing import Any, Dict, List

from pydantic import BaseModel, Field

from ten_ai_base import utils


class MinimaxTTSWebsocketConfig(BaseModel):

    key: str = ""
    group_id: str = ""
    url: str = "wss://api.minimax.io/ws/v1/t2a_v2"
    sample_rate: int = 16000
    channels: int = 1
    dump: bool = False
    dump_path: str = ""
    enable_words: bool = False
    params: Dict[str, Any] = Field(default_factory=dict)
    black_list_params: List[str] = Field(default_factory=list)

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params

    def update_params(self) -> None:
        ##### get value from params #####
        if "key" in self.params:
            self.key = self.params["key"]
            del self.params["key"]

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

        if "enable_words" in self.params:
            self.enable_words = self.params["enable_words"]
            del self.params["enable_words"]

        ##### use fixed value #####
        if "audio_setting" not in self.params:
            self.params["audio_setting"] = {}
        self.params["audio_setting"]["format"] = "pcm"
        self.params["audio_setting"]["sample_rate"] = self.sample_rate

        if self.enable_words:
            # TODO: auto set subtitle_enable and subtitle_type if enable_words is True
            if "subtitle_enable" not in self.params:
                self.params["subtitle_enable"] = True
            if "subtitle_type" not in self.params:
                self.params["subtitle_type"] = "word"

    def validate_params(self) -> None:
        """Validate required configuration parameters."""
        required_fields = ["key"]

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
        if config.key:
            config.key = utils.encrypt(config.key)
        return f"{config}"

    def get_voice_ids(self) -> str:
        if not self.params:
            return ""

        if "timber_weights" in self.params:
            voice_ids = []
            for weight in self.params["timber_weights"]:
                if "voice_id" in weight:
                    voice_id = weight["voice_id"]
                    if not voice_id:
                        continue
                    voice_ids.append(voice_id)
            return ",".join(voice_ids)

        if (
            "voice_setting" in self.params
            and "voice_id" in self.params["voice_setting"]
        ):
            return self.params["voice_setting"]["voice_id"]

        return ""
