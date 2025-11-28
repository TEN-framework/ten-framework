from typing import Any, Dict, List

from pydantic import BaseModel, Field

from ten_ai_base import utils


class BytedanceTTSDuplexConfig(BaseModel):
    # Bytedance TTS credentials
    app_id: str = ""
    api_key: str = ""
    token: str = ""
    resource_id: str = "volc.service_type.10029"

    # Bytedance TTS specific configs
    # Refer to: https://www.volcengine.com/docs/6561/1329505.
    api_url: str = "wss://openspeech.bytedance.com/api/v3/tts/bidirection"
    speaker: str = ""
    sample_rate: int = 24000
    model: str = ""

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

        if "api_key" in self.params:
            self.api_key = self.params["api_key"]
            del self.params["api_key"]

        if "api_url" in self.params:
            self.api_url = self.params["api_url"]
            del self.params["api_url"]

        if "token" in self.params:
            self.token = self.params["token"]
            del self.params["token"]

        if "resource_id" in self.params:
            self.resource_id = self.params["resource_id"]
            del self.params["resource_id"]

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

        if "model" in self.params:
            self.model = self.params["model"]

        ##### use fixed value #####
        if "audio_params" not in self.params:
            self.params["audio_params"] = {}
        self.params["audio_params"]["format"] = "pcm"

        if self.enable_words:
            self.params["audio_params"]["enable_timestamp"] = True

    def validate_params(self) -> None:
        """Validate required configuration parameters."""
        # 检查 speaker 是否存在
        if not self.speaker or not self.speaker.strip():
            raise ValueError(
                "required field is missing or empty: params.speaker"
            )

        # 检查 app_id 和 api_key 是否至少存在一个
        app_id_present = self.app_id and self.app_id.strip()
        api_key_present = self.api_key and self.api_key.strip()
        token_present = self.token and self.token.strip()

        if not app_id_present and not api_key_present:
            raise ValueError(
                "at least one of 'app_id' or 'api_key' must be provided and not empty"
            )

        if app_id_present and not token_present:
            raise ValueError(
                "app_id is provided but token is not provided, please check your configuration"
            )

    def to_str(self, sensitive_handling: bool = False) -> str:
        if not sensitive_handling:
            return f"{self}"
        config = self.copy(deep=True)
        if config.app_id:
            config.app_id = utils.encrypt(config.app_id)
        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)
        if config.token:
            config.token = utils.encrypt(config.token)
        return f"{config}"
