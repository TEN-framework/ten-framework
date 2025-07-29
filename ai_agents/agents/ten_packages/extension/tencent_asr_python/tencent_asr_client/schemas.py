"""
Tencent ASR WebSocket API Schemas

This module contains the schemas for the Tencent ASR WebSocket API.

The schemas are defined using Pydantic.

The schemas are used to validate the data received from the Tencent ASR WebSocket API.

ref: https://cloud.tencent.com/document/product/1093/48982
"""

from typing import Generic, TypeVar, Any
from enum import IntEnum
from pydantic import BaseModel, Field, ConfigDict, computed_field, field_validator
from typing_extensions import Literal
import urllib.parse
import random
import time
import hmac
import hashlib
import base64
import uuid
from .log import get_logger

ResultType = TypeVar("ResultType")

class ResponseData(BaseModel, Generic[ResultType]):
    code: int
    message: str

    # Optional fields
    voice_id: str | None = None
    message_id: str | None = None
    result: ResultType | None = None
    final: bool | None = None

    model_config = ConfigDict(extra="allow")


class Word(BaseModel):
    """Word structure for RecoginizeResult.word_list"""

    word: str = Field(description="Content of the word")
    start_time: int = Field(description="Start time of the word in the audio stream")
    end_time: int = Field(description="End time of the word in the audio stream")
    stable_flag: int = Field(
        description="Stable flag of the word, 0 means may change, 1 means stable"
    )


class RecoginizeResult(BaseModel):
    """Recognition result structure"""
    class SliceType(IntEnum):
        """Recognition result type"""

        START = 0  # Start of a speech segment
        PROCESSING = 1  # Speech recognition in progress, non-stable result
        END = 2  # End of a speech segment, stable result


    slice_type: SliceType = Field(
        description="Recognition result type: 0-start, 1-processing, 2-end"
    )
    index: int = Field(
        description="Index of current speech segment in the audio stream, starting from 0"
    )
    start_time: int = Field(
        description="Start time of current speech segment in the audio stream"
    )
    end_time: int = Field(
        description="End time of current speech segment in the audio stream"
    )
    voice_text_str: str = Field(
        description="Text result of current speech segment, UTF8 encoded"
    )
    word_size: int = Field(default=0, description="Number of words in current speech segment")
    word_list: list[Word] = Field(default_factory=list, description="Word list of current speech segment")
    emotion_type: int | None = Field(default=None, description="Emotion type")
    speaker_info: str | None = Field(default=None, description="Speaker info")

    model_config = ConfigDict(extra="allow")


class RequestParams(BaseModel):
    """Request parameters for Tencent ASR WebSocket API"""

    class VoiceFormat(IntEnum):
        PCM = 1
        SPEEX = 4
        SILK = 6
        MP3 = 8
        OPUS = 10
        WAV = 12
        M4A = 14
        AAC = 16

        def __str__(self):
            return str(self.value)

    # not used in request query params
    endpoint: str = Field(description="Tencent ASR WebSocket API endpoint", default="asr.cloud.tencent.com/asr/v2")

    # Required parameters
    secretid: str = Field(description="Tencent Cloud registered account secret ID")
    timestamp: int = Field(description="Current UNIX timestamp in seconds", default_factory=lambda: int(time.time()))
    expired: int = Field(
        description="Signature expiration time UNIX timestamp in seconds",
        default_factory=lambda data: data["timestamp"] + 24 * 60 * 60,
    )
    nonce: int = Field(
        description="Random positive integer, max 10 digits",
        ge=0,
        le=9999999999,
        default_factory=lambda data: random.randint(0, 9999999999),
    )
    engine_model_type: str = Field(default="16k_zh", description="Engine model type")
    voice_id: str = Field(description="Global unique identifier for audio stream", default_factory=lambda: str(uuid.uuid4()))

    # Optional parameters
    voice_format: VoiceFormat | None = Field(
        default=None,
        description="Audio encoding format: 1-pcm, 4-speex, 6-silk, 8-mp3, 10-opus, 12-wav, 14-m4a, 16-aac. default: 4(speex)",
    )
    needvad: int | None = Field(default=None, description="VAD switch: 0-off, 1-on. default: 0")
    hotword_id: str | None = Field(default=None, description="Hot word table ID")
    customization_id: str | None = Field(
        default=None, description="Self-learning model ID"
    )
    filter_dirty: int | None = Field(
        default=None,
        description="Dirty word filter: 0-no filter, 1-filter, 2-replace with '*'. default: 0",
    )
    filter_modal: int | None = Field(
        default=None,
        description="Modal word filter: 0-no filter, 1-partial filter, 2-strict filter. default: 0",
    )
    filter_punc: int | None = Field(
        default=None,
        description="Punctuation filter: 0-no filter, 1-filter sentence ending periods. default: 0",
    )
    filter_empty_result: int | None = Field(
        default=None,
        description="Empty result callback: 0-callback empty results, 1-no callback. default: 1",
    )
    convert_num_mode: int | None = Field(
        default=None,
        description="Arabic number conversion: 0-no conversion, 1-smart conversion, 3-math number conversion. default: 1",
    )
    word_info: int | None = Field(
        default=None,
        description="Word level timestamp: 0-no display, 1-display without punctuation, 2-display with punctuation. default: 0",
    )
    vad_silence_time: int | None = Field(
        default=None,
        description="VAD silence detection threshold in ms, range 240-2000. default: 1000. needvad=1 is required",
    )
    max_speak_time: int | None = Field(
        default=None, description="Force sentence break in ms, range 5000-90000. default: 60000"
    )
    noise_threshold: float | None = Field(
        default=None, 
        ge=-1,
        le=1,
        description="Noise parameter threshold, range [-1,1]. default: 0.0"
    )
    hotword_list: str | None = Field(
        default=None, description="Temporary hot word table"
    )
    input_sample_rate: int | None = Field(
        default=None, description="Input sample rate, only supports 8000 for PCM format"
    )
    emotion_recognition: int | None = Field(
        default=None,
        description="Emotion recognition: 0-off, 1-on without tags, 2-on with tags. default: 0",
    )
    replace_text_id: str | None = Field(
        default=None, description="Replace vocabulary table ID"
    )

    @field_validator('hotword_list', mode='after')  
    @classmethod
    def _validate_hotword_list(cls, value: str) -> str | None:
        if len(value) == 0:
            return None
        _words = value.split(",")
        _words = [word.split("|") for word in _words]
        _words = [(word[0], int(word[1])) for word in _words]

        for word in _words:
            if len(word[0]) == 0 or len(word[0])>30:
                raise ValueError(f"invalid hotword: {word}")
            if not((word[1] >=1 and word[1] <= 11) or word[1] == 100):
                raise ValueError(f"invalid hotword weight: {word[0]}|{word[1]}")
        return value

    def _query_params_without_signature(self) -> dict[str, Any]:
        # update timestamp and expired
        self.timestamp = int(time.time())
        self.expired = self.timestamp + 24 * 60 * 60
        query = self.model_dump(exclude_none=True, exclude={"endpoint"})

        logger = get_logger()
        logger.debug(f"query params: {query}")
        return query

    def query_params(self, app_id: str, secret_key: str) -> str:
        endpoint = self.endpoint.lstrip("wss://").rstrip("/")
        endpoint = f"{endpoint}/{app_id}"
        query_dict = self._query_params_without_signature()
        query_dict = dict(sorted(query_dict.items(), key=lambda d: d[0]))
        query = urllib.parse.urlencode(query_dict)
        signstr = f"{endpoint}?{query}"
        hmacstr = hmac.new(
            secret_key.encode("utf-8"), signstr.encode("utf-8"), hashlib.sha1
        ).digest()
        signature = base64.b64encode(hmacstr).decode("utf-8")
        query_dict["signature"] = signature

        logger = get_logger()
        logger.debug(f"query params with signature: {query_dict}")

        return urllib.parse.urlencode(query_dict)

    def uri(self, app_id:str, secret_key: str) -> str:
        endpoint = self.endpoint.lstrip("wss://").rstrip("/")
        full_url = f"wss://{endpoint}/{app_id}?{self.query_params(app_id, secret_key)}"

        logger = get_logger()
        logger.debug(f"uri: {full_url}")

        return full_url
