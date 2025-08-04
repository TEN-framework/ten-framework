"""
OpenAI ASR WebSocket API Schemas

This module contains the schemas for the OpenAI ASR WebSocket API.

The schemas are defined using Pydantic.

The schemas are used to validate the data received from the OpenAI ASR WebSocket API.

ref: 

https://platform.openai.com/docs/guides/speech-to-text#streaming-the-transcription-of-an-ongoing-audio-recording

https://platform.openai.com/docs/guides/realtime?use-case=transcription#connect-with-websockets

!!! this is a beta api, the schemas are not stable !!!
"""

from typing import Generic, TypeVar
from pydantic import BaseModel, ConfigDict
from typing_extensions import Literal

from openai.types.beta.realtime.transcription_session_update_param import SessionTurnDetection, SessionInputAudioTranscription, SessionInputAudioNoiseReduction

SessionType = TypeVar("SessionType")

class Session(BaseModel, Generic[SessionType]):
    type: str
    event_id: str | None = None
    session: SessionType

class Error(BaseModel):
    type: str
    code: str
    message: str
    param: str | None = None
    model_config = ConfigDict(extra="allow")


class TranscriptionSessionUpdateParam(BaseModel):
    input_audio_format: Literal["pcm16", "g711_ulaw", "g711_alaw"]
    input_audio_transcription: SessionInputAudioTranscription
    turn_detection: SessionTurnDetection | None = None
    input_audio_noise_reduction: SessionInputAudioNoiseReduction | None = None
    include: list[str] | None = None
    client_secret: str | None = None

# for openai beta realtime api(wss), we must connect with the server first,
# then send the transcription session update param to the server.
# so we need to define a schema for the transcription session update param.
TranscriptionParam = TranscriptionSessionUpdateParam

class TranscriptionResultDelta(BaseModel):
    """
    {"type":"conversation.item.input_audio_transcription.delta","event_id":"event_BzGq0z8Y99Ft4976EKDXD","item_id":"item_BzGpxdtFv1RUd0Iidvo05","content_index":0,"delta":"hello"}
    """
    type: Literal["conversation.item.input_audio_transcription.delta"]
    event_id: str
    item_id: str
    content_index: int
    delta: str
    model_config = ConfigDict(extra="allow")

class TranscriptionResultCompleted(BaseModel):
    """
    # for whisper-1
    {"type":"conversation.item.input_audio_transcription.completed","event_id":"event_BzGq0czBf4Cx0nTakZNvh","item_id":"item_BzGpxdtFv1RUd0Iidvo05","content_index":0,"transcript":"Hello world","usage":{"type":"duration","seconds":2}}

    # for gpt-4o-transcribe, gpt-4o-mini-transcribe
    {"type":"conversation.item.input_audio_transcription.completed","event_id":"event_BzJ5XSUIVRnWMINGAOERU","item_id":"item_BzJ5TXNPJWWEB80n8MTCn","content_index":0,"transcript":"4月13日，中国台北选手。","usage":{"type":"tokens","total_tokens":63,"input_tokens":51,"input_token_details":{"text_tokens":28,"audio_tokens":23},"output_tokens":12}}

    """
    class Usage(BaseModel):
        type: str
        seconds: float | None = None
        total_tokens: int | None = None
        input_tokens: int | None = None
        output_tokens: int | None = None
        model_config = ConfigDict(extra="allow")

    type: Literal["conversation.item.input_audio_transcription.completed"]
    event_id: str
    item_id: str
    content_index: int
    transcript: str
    usage: Usage | None = None
    model_config = ConfigDict(extra="allow")

class TranscriptionResultCommitted(BaseModel):
    """
    {"type":"input_audio_buffer.committed","event_id":"event_BzIyYo5dVYD4EymLRxOeK","previous_item_id":null,"item_id":"item_BzIyOo8QFAyacGMS5qVRU"}
    """
    type: Literal["input_audio_buffer.committed"]
    event_id: str
    previous_item_id: str | None = None
    item_id: str
    model_config = ConfigDict(extra="allow")
