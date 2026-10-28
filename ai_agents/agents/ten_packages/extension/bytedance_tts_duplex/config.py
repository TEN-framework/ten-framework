import json
import tempfile
from typing import Any

from pydantic import BaseModel, Field

from ten_ai_base import utils


def _safe_int(value: Any, default: int) -> int:
    if isinstance(value, bool):
        return default
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _mask_header_value(value: Any) -> Any:
    if not isinstance(value, str):
        return value
    return utils.encrypt(value)


def _disable_cache(json_text: str) -> str:
    try:
        values = json.loads(json_text or "{}")
    except json.JSONDecodeError as error:
        raise ValueError("additions must be a JSON object") from error
    if not isinstance(values, dict):
        raise ValueError("additions must be a JSON object")
    cache_config = values.get("cache_config")
    if isinstance(cache_config, dict) and "use_cache" in cache_config:
        return json_text
    if not isinstance(cache_config, dict):
        cache_config = {}
    cache_config["use_cache"] = False
    values["cache_config"] = cache_config
    return json.dumps(values, ensure_ascii=False)


class BytedanceTTSDuplexConfig(BaseModel):
    # Bytedance TTS credentials
    app_id: str = ""
    api_key: str = ""
    token: str = ""
    resource_id: str = "volc.service_type.10029"

    # Bytedance TTS specific configs
    # Refer to: https://www.volcengine.com/docs/6561/1329505.
    api_url: str = "wss://openspeech.bytedance.com/api/v3/tts/bidirection"
    url: str = ""
    headers: dict[str, Any] = Field(default_factory=dict)
    speaker: str = ""
    sample_rate: int = 24000
    model: str = ""

    # Bytedance TTS pass through parameters
    params: dict[str, Any] = Field(default_factory=dict)
    # Black list parameters, will be removed from params
    black_list_keys: list[str] = Field(default_factory=list)

    # Debug and dump settings
    dump: bool = False
    dump_path: str = tempfile.gettempdir()
    enable_words: bool = False

    def update_params(self) -> None:
        ##### get value from params #####
        for field_name in (
            "app_id",
            "api_key",
            "api_url",
            "token",
            "resource_id",
        ):
            if field_name in self.params:
                value = self.params.pop(field_name)
                if isinstance(value, str):
                    setattr(self, field_name, value)

        if self.url != "":
            self.api_url = self.url

        audio_params = self.params.get("audio_params")
        if not isinstance(audio_params, dict):
            audio_params = {}
            self.params["audio_params"] = audio_params
        if "sample_rate" in audio_params:
            self.sample_rate = _safe_int(audio_params["sample_rate"], self.sample_rate)
        audio_params["sample_rate"] = self.sample_rate

        for field_name in ("speaker", "model"):
            if field_name in self.params:
                value = self.params[field_name]
                if isinstance(value, str):
                    setattr(self, field_name, value)

        ##### use fixed value #####
        audio_params["format"] = "pcm"

        if self.enable_words:
            audio_params["enable_timestamp"] = True
            audio_params["enable_subtitle"] = True
            additions = self.params.get("additions", "")
            self.params["additions"] = _disable_cache(additions)

    def _merge_dict(self, dest: dict[str, Any], src: dict[str, Any]) -> None:
        """Recursively merge src into dest."""
        for k, v in src.items():
            if k in dest and isinstance(dest[k], dict) and isinstance(v, dict):
                self._merge_dict(dest[k], v)
            else:
                dest[k] = v

    def merge_with_dict(self, updates: dict[str, Any]) -> None:
        """
        Merge incremental updates from a dict and re-run param extraction/validation.
        """
        # 1. Dump current model to dict
        current_data = self.model_dump()

        # 2. Merge incremental updates
        self._merge_dict(current_data, updates)

        # 3. Create a temporary model to validate everything
        # We use model_validate to handle type conversion
        merged_model = self.__class__.model_validate(current_data)
        merged_model.update_params()

        # 4. If everything passed, update current instance fields
        for field in self.__class__.model_fields:
            setattr(self, field, getattr(merged_model, field))

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
        if config.headers:
            for header_key in (
                "X-Api-Key",
                "X-Api-App-Key",
                "X-Api-Access-Key",
                "X-Api-Resource-Id",
            ):
                if header_key in config.headers:
                    config.headers[header_key] = _mask_header_value(
                        config.headers[header_key],
                    )
        return f"{config}"
