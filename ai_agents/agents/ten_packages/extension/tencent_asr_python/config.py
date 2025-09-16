from pydantic import BaseModel, Field, ConfigDict, model_validator
from typing_extensions import Self, Any
from enum import Enum
from pathlib import Path
from .tencent_asr_client import RequestParams
from .utils import encrypting_serializer


class Params(BaseModel):
    class FinalizeMode(Enum):
        DISCONNECT = "disconnect"
        MUTE_PKG = "mute_pkg"
        VENDOR_DEFINED = "vendor_defined"

    finalize_mode: FinalizeMode = Field(
        default=FinalizeMode.MUTE_PKG,
        description="Tencent ASR finalize mode",
    )

    # only used when finalize_mode is MUTE_PKG
    mute_pkg_duration_ms: int | None = Field(
        default=None,
        description="Tencent ASR mute pkg duration in milliseconds, default: None",
    )

    keep_alive_interval: int | None = Field(
        default=None, description="Tencent ASR keep alive interval in seconds"
    )
    log_level: str = Field(default="INFO", description="Tencent ASR log level")

    # vendor request params
    appid: str = Field(
        description="Tencent Cloud registered account app ID",
        min_length=1,
        alias="app_id",
    )
    secretkey: str = Field(
        description="Tencent Cloud registered account secret key",
        min_length=1,
        alias="secret_key",
    )

    secretid: str = Field(
        description="Tencent Cloud registered account secret ID",
        min_length=1,
        alias="secret_id",
    )

    input_sample_rate: int | None = Field(
        default=None,
        description="Input sample rate, only supports 8000 for PCM format",
    )
    voice_format: RequestParams.VoiceFormat = Field(
        default=RequestParams.VoiceFormat.PCM,
        description="Audio encoding format: 1-pcm, 4-speex, 6-silk, 8-mp3, 10-opus, 12-wav, 14-m4a, 16-aac. default: 4(speex)",
    )
    needvad: int = Field(
        default=1, description="VAD switch: 0-off, 1-on. default: 0"
    )
    vad_silence_time: int | None = Field(
        default=1000,
        description="VAD silence detection threshold in ms, range 240-2000. default: 1000. needvad=1 is required",
    )
    model_config = ConfigDict(extra="allow", populate_by_name=True)

    _encrypt_fields = encrypting_serializer("appid", "secretkey", "secretid")

    @model_validator(mode="before")
    @classmethod
    def compatible_config(cls, data: dict[str, Any]) -> Any:
        if "app_id" in data:
            data["appid"] = data.pop("app_id")
        if "secret_key" in data:
            data["secretkey"] = data.pop("secret_key")
        if "secret_id" in data:
            data["secretid"] = data.pop("secret_id")

        # compatible old config, it's not recommended.
        if "key" in data:
            data["secretid"] = data.pop("key")
        if "secret" in data:
            data["secretkey"] = data.pop("secret")
        if "language" in data and "engine_model_type" not in data:
            language_model_map = {
                "zh-CN": "16k_zh",
                "en-US": "16k_en",
                "es-ES": "16k_es",
                "ja-JP": "16k_ja",
                "ko-KR": "16k_ko",
                "ar-AE": "16k_ar",
                "hi-IN": "16k_hi",
            }
            data["engine_model_type"] = language_model_map[data.pop("language")]

        if "hotword_list" in data and isinstance(data["hotword_list"], list):
            data["hotword_list"] = ",".join(data.pop("hotword_list"))

        return data

    @model_validator(mode="after")
    def check_mute_pkg_duration_ms(self) -> Self:
        if self.finalize_mode == Params.FinalizeMode.MUTE_PKG:
            if self.needvad == 0:
                raise ValueError(
                    "needvad must be 1 when finalize_mode is MUTE_PKG"
                )
            if self.vad_silence_time is None:
                raise ValueError(
                    "vad_silence_time must be set when finalize_mode is MUTE_PKG"
                )
            if self.mute_pkg_duration_ms is None:
                self.mute_pkg_duration_ms = self.vad_silence_time + 200
        return self

    def to_request_params(self) -> RequestParams:
        return RequestParams(
            **self.model_dump(
                exclude_none=True,
                by_alias=False,
                exclude={
                    "finalize_mode",
                    "mute_pkg_duration_ms",
                    "keep_alive_interval",
                    "log_level",
                },
            )
        )


class TencentASRConfig(BaseModel):
    dump: bool = Field(default=False, description="Tencent ASR dump")
    dump_path: str = Field(
        default_factory=lambda: str(
            Path(__file__).parent / "tencent_asr_in.pcm"
        ),
        description="Tencent ASR dump path",
    )
    params: Params = Field(..., description="Tencent ASR params")
    model_config = ConfigDict(extra="ignore", populate_by_name=True)

    @model_validator(mode="before")
    @classmethod
    def compatible_config(cls, data: dict[str, Any]) -> Any:
        if "language" in data:
            data["params"]["language"] = data.pop("language")
        return data
