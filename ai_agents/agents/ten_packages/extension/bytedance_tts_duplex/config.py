from typing import Any, Dict, List
from pydantic import BaseModel, Field
from ten_ai_base import utils


class BytedanceTTSDuplexConfig(BaseModel):
    # Bytedance TTS credentials
    app_id: str = ""
    token: str = ""

    # Bytedance TTS specific configs
    # Refer to: https://www.volcengine.com/docs/6561/1329505.
    api_url: str = "wss://openspeech.bytedance.com/api/v3/tts/bidirection"
    speaker: str = ""
    sample_rate: int = 24000

    # Bytedance TTS pass through parameters
    params: Dict[str, Any] = Field(default_factory=dict)
    # Black list parameters, will be removed from params
    black_list_keys: List[str] = Field(default_factory=list)

    # Debug and dump settings
    dump: bool = False
    dump_path: str = "/tmp"
    enable_words: bool = False

    def update_params(self) -> None:
        ##### get value from params #####
        if "app_id" in self.params:
            self.app_id = self.params["app_id"]
            del self.params["app_id"]

        if "token" in self.params:
            self.token = self.params["token"]
            del self.params["token"]

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

        if "speaker" in self.params:
            self.speaker = self.params["speaker"]

        ##### use fixed value #####
        if "audio_params" not in self.params:
            self.params["audio_params"] = {}
        self.params["audio_params"]["format"] = "pcm"

    def validate_params(self) -> None:
        """Validate required configuration parameters."""
        required_fields = ["app_id", "token", "speaker"]

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
        if config.app_id:
            config.app_id = utils.encrypt(config.app_id)
        if config.token:
            config.token = utils.encrypt(config.token)
        return f"{config}"
