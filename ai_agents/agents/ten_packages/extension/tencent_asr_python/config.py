from pydantic import BaseModel, Field
from enum import Enum
from pathlib import Path
from .utils import encrypting_serializer
from .tencent_asr_client import RequestParams


class TencentASRConfig(BaseModel):
    class FinalizeMode(Enum):
        DISCONNECT = "disconnect"
        MUTE_PKG = "mute_pkg"
        VENDOR_DEFINED = "vendor_defined"

    app_id: str = Field(default="", description="Tencent ASR app id", min_length=1)
    secret_key: str = Field(
        default="", description="Tencent ASR secret key", min_length=1
    )
    params: RequestParams = Field(..., description="Tencent ASR params")
    finalize_mode: FinalizeMode = Field(
        default=FinalizeMode.VENDOR_DEFINED, description="Tencent ASR finalize mode"
    )
    # only used when finalize_mode is MUTE_PKG
    mute_pkg_duration_ms: int = Field(
        default=800, description="Tencent ASR mute pkg duration in milliseconds"
    )
    dump: bool = Field(default=False, description="Tencent ASR dump")
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent / "tencent_asr_in.pcm"),
        description="Tencent ASR dump path",
    )
    keep_alive_interval: int | None = Field(
        default=None, description="Tencent ASR keep alive interval in seconds"
    )
    log_level: str = Field(
        default="INFO", description="Tencent ASR log level"
    )

    _encrypt_serializer = encrypting_serializer(
        "app_id",
        "secret_key",
    )
