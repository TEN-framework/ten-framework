#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from pydantic import BaseModel
from enum import Enum, IntEnum
from typing import Any

class MetadataKey(str, Enum):
    SESSION_ID = "session_id"
    TURN_ID = "turn_id"

class ModuleType(str, Enum):
    ASR = "asr"
    LLM = "llm"
    TTS = "tts"
    MLLM = "mllm"
    AVATAR = "avatar"
    TURN = "turn"

class ModuleMetricKey(str, Enum):
    ASR_TTFW = "ttfw"   # time to first word
    ASR_TTLW = "ttlw"   # time to last word
    ASR_CONNECT_DELAY = "connect_delay"   # time cost to connect to asr server
    ASR_ACTUAL_SEND = "actual_send"   # audio duration sent to asr server
    ASR_ACTUAL_SEND_DELTA = "actual_send_delta"  # audio duration sent since last report
    ASR_VENDOR_METRICS = "vendor_metrics"   # vendor specific metrics
    TTS_TTFB = "ttfb"   # time to first byte
    LLM_TTFT = "ttft"   # time to first token
    LLM_TTFS = "ttfs"   # time to first sentence

class ModuleErrorCode(str, Enum):
    OK = 0

    # After a fatal error occurs, the module will stop all operations.
    FATAL_ERROR = -1000

    # After a non-fatal error occurs, the module itself will continue to retry.
    NON_FATAL_ERROR = 1000


class ModuleErrorVendorInfo(BaseModel):
    vendor: str = ""    # vendor name
    code: str = ""      # vendor's original error code
    message: str = ""   # vendor's original error message

class ModuleError(BaseModel):
    id: str = ""        # uuid
    module: str = ""    # module type
    code: int = 0
    message: str = ""
    vendor_info: ModuleErrorVendorInfo | None = None
    metadata: dict[str, Any] = {}


class ModuleVendorException(Exception):
    def __init__(self, error: ModuleErrorVendorInfo) -> None:
        super().__init__(error.message)
        self.error = error

    def __str__(self) -> str:
        return f"ModuleVendorException: {self.error.message} (code: {self.error.code}, vendor: {self.error.vendor})"

class ModuleMetrics(BaseModel):
    id: str = ""        # uuid
    module: str = ""    # module type
    vendor: str = ""    # vendor name
    metrics: dict[str, Any] = {}   # key-value pair metrics, e.g. {"ttfb": 100, "ttfs": 200}
    metadata: dict[str, Any] = {}

class ErrorMessage(BaseModel):
    object: str = "message.error"
    module: str = ""
    message: str = ""
    turn_id: int = 0
    code: int = 0

class ErrorMessageVendorInfo(BaseModel):
    object: str = "message.error.vendor_info"
    vendor: str = ""
    code: int = 0
    message: str = ""

class MetricsMessage(BaseModel):
    object: str = "message.metrics"
    module: str = ""
    metric_name: str = ""
    turn_id: int = 0
    latency_ms: int = 0

class TTSAudioEndReason(IntEnum):
    REQUEST_END = 1
    INTERRUPTED = 2
    ERROR = 3
