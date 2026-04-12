#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from __future__ import annotations

import copy
from typing import Any

from pydantic import AliasChoices, BaseModel, ConfigDict, Field, field_validator
from ten_ai_base import utils
from ten_runtime import AsyncTenEnv


VALID_QUALITIES = {"low", "medium", "high"}
VALID_VIDEO_ENCODINGS = {"H264", "VP8", "AV1"}
VALID_AREAS = {
    "GLOBAL",
    "NORTH_AMERICA",
    "EUROPE",
    "ASIA",
    "INDIA",
    "JAPAN",
}

_PARAM_FIELD_ALIASES: dict[str, str] = {
    "api_key": "generic_video_api_key",
    "generic_video_api_key": "generic_video_api_key",
    "agora_appid": "agora_appid",
    "agora_appcert": "agora_appcert",
    "channel": "channel",
    "agora_channel_name": "channel",
    "agora_avatar_uid": "agora_avatar_uid",
    "agora_video_uid": "agora_avatar_uid",
    "avatar_id": "avatar_id",
    "quality": "quality",
    "version": "version",
    "video_encoding": "video_encoding",
    "enable_string_uid": "enable_string_uid",
    "activity_idle_timeout": "activity_idle_timeout",
    "area": "area",
    "start_endpoint": "start_endpoint",
    "stop_endpoint": "stop_endpoint",
    "input_audio_sample_rate": "input_audio_sample_rate",
}

_PASSTHROUGH_EXCLUDED_KEYS = set(_PARAM_FIELD_ALIASES)


class GenericVideoConfig(BaseModel):
    model_config = ConfigDict(populate_by_name=True, extra="ignore")

    agora_appid: str = ""
    agora_appcert: str = ""
    channel: str = Field(
        default="",
        validation_alias=AliasChoices("channel", "agora_channel_name"),
    )
    agora_avatar_uid: int = Field(
        default=0,
        validation_alias=AliasChoices(
            "agora_avatar_uid",
            "agora_video_uid",
        ),
    )
    generic_video_api_key: str = ""
    avatar_id: str = "16cb73e7de08"
    quality: str = "high"
    version: str = "v1"
    video_encoding: str = "H264"
    enable_string_uid: bool = False
    activity_idle_timeout: int = 120
    area: str = "GLOBAL"
    start_endpoint: str = "https://api.example.com/v1/sessions/start"
    stop_endpoint: str = "https://api.example.com/v1/sessions/stop"
    input_audio_sample_rate: int = 48000
    params: dict[str, Any] = Field(default_factory=dict)

    @classmethod
    async def create_async(
        cls, ten_env: AsyncTenEnv
    ) -> "GenericVideoConfig":
        config_json, _ = await ten_env.get_property_to_json("")
        config = cls.model_validate_json(config_json or "{}")
        config.normalize_params()
        config.validate_required()
        return config

    def normalize_params(self) -> None:
        params = self._ensure_dict(self.params)
        normalized_params: dict[str, Any] = {}

        for key, value in params.items():
            if value is None:
                continue
            field_name = _PARAM_FIELD_ALIASES.get(key)
            if field_name:
                setattr(self, field_name, value)
                continue
            normalized_params[key] = value

        self.params = normalized_params

    def validate_required(self) -> None:
        required_fields = {
            "agora_appid": self.agora_appid,
            "channel": self.channel,
            "generic_video_api_key": self.generic_video_api_key,
            "avatar_id": self.avatar_id,
            "start_endpoint": self.start_endpoint,
            "stop_endpoint": self.stop_endpoint,
        }

        for field_name, value in required_fields.items():
            if not value or (isinstance(value, str) and value.strip() == ""):
                raise ValueError(
                    f"Required field is missing or empty: {field_name}"
                )

    def to_str(self, sensitive_handling: bool = True) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)
        if config.generic_video_api_key:
            config.generic_video_api_key = utils.encrypt(
                config.generic_video_api_key
            )
        if "api_key" in config.params and config.params["api_key"]:
            config.params["api_key"] = utils.encrypt(config.params["api_key"])
        return f"{config}"

    @property
    def vendor_params(self) -> dict[str, Any]:
        return {
            key: value
            for key, value in self.params.items()
            if key not in _PASSTHROUGH_EXCLUDED_KEYS and value is not None
        }

    @field_validator("quality")
    @classmethod
    def validate_quality(cls, value: str) -> str:
        if value not in VALID_QUALITIES:
            raise ValueError(
                f"quality must be one of: {', '.join(sorted(VALID_QUALITIES))}"
            )
        return value

    @field_validator("video_encoding")
    @classmethod
    def validate_video_encoding(cls, value: str) -> str:
        if value not in VALID_VIDEO_ENCODINGS:
            raise ValueError(
                "video_encoding must be one of: "
                f"{', '.join(sorted(VALID_VIDEO_ENCODINGS))}"
            )
        return value

    @field_validator("area")
    @classmethod
    def validate_area(cls, value: str) -> str:
        if value not in VALID_AREAS:
            raise ValueError(
                f"area must be one of: {', '.join(sorted(VALID_AREAS))}"
            )
        return value

    @field_validator("activity_idle_timeout")
    @classmethod
    def validate_activity_idle_timeout(cls, value: int) -> int:
        if value < 0:
            raise ValueError("activity_idle_timeout must be >= 0")
        return value

    @field_validator("input_audio_sample_rate")
    @classmethod
    def validate_input_audio_sample_rate(cls, value: int) -> int:
        if value <= 0:
            raise ValueError("input_audio_sample_rate must be > 0")
        return value

    @staticmethod
    def _ensure_dict(value: Any) -> dict[str, Any]:
        if isinstance(value, dict):
            return dict(value)
        if value is None:
            return {}
        return dict(value)
