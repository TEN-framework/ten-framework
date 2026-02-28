#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from enum import Enum
import json
from typing import Any, List, Literal, Optional, TypeAlias, Union
from pydantic import BaseModel, HttpUrl

from .types import LLMToolMetadata


class TTSWord(BaseModel):
    word: str = ""
    start_ms: int = -1  # start time of the word, milliseconds since epoch
    duration_ms: int = -1  # duration of the word, in milliseconds


class TTSTextInput(BaseModel):
    request_id: str
    text: str
    text_input_end: bool = False
    metadata: dict[str, Any] = {}  # additional metadata for the transcription


class TTSTextResult(BaseModel):
    request_id: str
    text: str
    start_ms: int
    duration_ms: int
    words: list[TTSWord] | None = None
    text_result_end: bool = False
    metadata: dict[str, Any] = {}  # additional metadata for the transcription


class TTSFlush(BaseModel):
    flush_id: str
    metadata: dict[str, Any] = {}  # additional metadata for the flush operation


class ASRWord(BaseModel):
    word: str
    start_ms: int
    duration_ms: int
    stable: bool


class ASRResult(BaseModel):
    id: str | None = None
    text: str
    final: bool
    start_ms: int
    duration_ms: int
    language: str
    words: list[ASRWord] | None = None
    metadata: dict[str, Any] = {}  # additional metadata for the transcription


class TTS2HttpResponseEventType(Enum):
    """Event types for TTS2 HTTP responses."""
    RESPONSE = 1  # Audio data chunk received
    END = 2  # Stream ended normally
    ERROR = 3  # General error occurred
    INVALID_KEY_ERROR = 4  # API key authentication failed
    FLUSH = 5  # Flush/cancel event


"""
===========LLM Input and Output Models================
"""
# ----------- Message Content Types -----------

class TextContent(BaseModel):
    type: Literal["text"]
    text: str


class ImageURL(BaseModel):
    url: str  # or data:image/png;base64,...
    detail: Optional[Literal["auto", "low", "high"]] = "auto"


class ImageContent(BaseModel):
    type: Literal["image_url"]
    image_url: ImageURL


MessageContent = Union[TextContent, ImageContent]


# ----------- Normal Context Message -----------

class LLMMessageContent(BaseModel):
    # "system", "user", or "assistant" role
    role: Literal["system", "user", "assistant"]

    # content can be plain text or structured content list
    content: Union[str, List[MessageContent]]


# ----------- Custom Tool Call Message (your structure) -----------

class LLMMessageFunctionCall(BaseModel):
    type: Literal["function_call"]  # Custom message type to signal a tool call
    id: str                         # Unique ID for the tool call message
    call_id: str                    # ID to track this call across response
    name: str                       # Name of the function to call
    arguments: str                  # JSON string with input parameters
    role: Literal["assistant"] = "assistant"  # Role of the message sender


# ----------- Tool Response Message (custom output) -----------

class LLMMessageFunctionCallOutput(BaseModel):
    type: Literal["function_call_output"]  # Custom type for tool result message
    call_id: str                           # Must match the call_id from function_call
    output: str                            # JSON string of result or plain string
    role: Literal["tool"] = "tool"  # Role of the message sender


# ----------- Union for all supported message types -----------

LLMMessage = Union[
    LLMMessageContent,
    LLMMessageFunctionCall,
    LLMMessageFunctionCallOutput
]

class LLMRequest(BaseModel):
    """
    Model for LLM input data.
    This model is used to define the structure of the input data for LLM operations.
    """
    request_id: str
    model: Optional[str] = None
    messages: list[LLMMessage]
    streaming: Optional[bool] = True
    tools: Optional[list[LLMToolMetadata]] = None
    parameters: Optional[dict[str, Any]] = None
    prompt: Optional[str] = None

class LLMRequestAbort(BaseModel):
    """
    Model for LLM abort request.
    This model is used to define the structure of the request to abort an ongoing LLM operation.
    """
    request_id: str

class LLMRequestRetrievePrompt(BaseModel):
    """
    Model for LLM retrieve prompt request.
    This model is used to define the structure of the request to retrieve the prompt for an ongoing LLM operation.
    """
    request_id: str

class EventType(str, Enum):
    MESSAGE_CONTENT_DELTA = "message_content_delta"
    MESSAGE_CONTENT_DONE = "message_content_done"
    MESSAGE_REASONING_DELTA = "message_reasoning_delta"
    MESSAGE_REASONING_DONE = "message_reasoning_done"
    TOOL_CALL_CONTENT = "tool_call_content"

class LLMResponse(BaseModel):
    """
    Model for LLM output data.
    This model is used to define the structure of the output data returned by LLM operations.
    """
    response_id: str
    created: Optional[int] = None

class LLMResponseMessageDelta(LLMResponse):
    """
    Model for a single message in LLM output.
    This model is used to define the structure of messages returned by the LLM.
    """
    role: str
    content: Optional[str] = None
    delta: Optional[str] = None
    type: EventType = EventType.MESSAGE_CONTENT_DELTA

class LLMResponseMessageDone(LLMResponse):
    """
    Model for a message indicating the end of a response.
    This model is used to signal that the LLM has finished sending messages.
    """
    type: EventType = EventType.MESSAGE_CONTENT_DONE
    role: str
    content: Optional[str] = None


class LLMResponseReasoningDelta(LLMResponse):
    """
    Model for a single message in LLM output.
    This model is used to define the structure of messages returned by the LLM.
    """
    role: str
    content: Optional[str] = None
    delta: Optional[str] = None
    type: EventType = EventType.MESSAGE_REASONING_DELTA

class LLMResponseReasoningDone(LLMResponse):
    """
    Model for a message indicating the end of a response.
    This model is used to signal that the LLM has finished sending messages.
    """
    type: EventType = EventType.MESSAGE_REASONING_DONE
    role: str
    content: Optional[str] = None

class LLMResponseToolCall(LLMResponse):
    """
    Model for a tool call in LLM output.
    This model is used to define the structure of tool calls returned by the LLM.
    """
    id: str
    tool_call_id: str
    name: str
    type: EventType = EventType.TOOL_CALL_CONTENT
    arguments: Optional[dict[str, Any]] = None

class LLMResponseRetrievePrompt(BaseModel):
    """
    Model for a prompt in LLM output.
    This model is used to define the structure of the prompt returned by the LLM.
    """
    prompt: str


def parse_llm_response(unparsed_string: str) -> LLMResponse:
    data = json.loads(unparsed_string)

    # Dynamically select the correct message class based on the `type` field, using from_dict
    if data["type"] == EventType.MESSAGE_CONTENT_DELTA:
        return LLMResponseMessageDelta.model_validate(data)
    elif data["type"] == EventType.TOOL_CALL_CONTENT:
        return LLMResponseToolCall.model_validate(data)
    elif data["type"] == EventType.MESSAGE_CONTENT_DONE:
        return LLMResponseMessageDone.model_validate(data)
    elif data["type"] == EventType.MESSAGE_REASONING_DELTA:
        return LLMResponseReasoningDelta.model_validate(data)
    elif data["type"] == EventType.MESSAGE_REASONING_DONE:
        return LLMResponseReasoningDone.model_validate(data)

    raise ValueError(f"Unknown message type: {data['type']}")


"""
===========MLLM Input and Output Models================
"""
class MLLMClientMessageItem(BaseModel):
    """
    Model for MLLM messages.
    This model is used to define the structure of messages sent to and received from MLLM.
    """
    role: Literal["user", "assistant"]
    content: str

class MLLMClientSendMessageItem(BaseModel):
    """
    Model for sending messages to MLLM.
    This model is used to define the structure of messages sent to MLLM.
    """
    message: MLLMClientMessageItem

class MLLMClientSetMessageContext(BaseModel):
    """
    Model for setting the context messages in MLLM.
    This model is used to define the structure of context messages sent to MLLM.
    """
    messages: list[MLLMClientMessageItem] = []

class MLLMClientCreateResponse(BaseModel):
    """
    Model for creating a response in MLLM.
    This model is used to define the structure of the response creation request sent to MLLM.
    """
    pass

class MLLMClientRegisterTool(BaseModel):
    """
    Model for registering tools with MLLM.
    This model is used to define the structure of tool registration data.
    """
    tool: LLMToolMetadata

class MLLMClientFunctionCallOutput(BaseModel):
    """
    Model for MLLM function call output.
    This model is used to define the structure of function call outputs sent by the MLLM client.
    """
    call_id: str
    output: str

class MLLMServerSessionReady(BaseModel):
    """
    Model for MLLM server session ready event.
    This model is used to define the structure of the session ready event sent by the MLLM server.
    """
    metadata: dict[str, Any] = {}

class MLLMServerInterrupt(BaseModel):
    """ Model for MLLM server start of speech event.
    This model is used to define the structure of the start of speech event sent by the MLLM server.
    """
    metadata: dict[str, Any] = {}

class MLLMServerInputTranscript(BaseModel):
    """
    Model for MLLM request transcript.
    This model is used to define the structure of the transcript data for MLLM requests.
    """
    content: str = ""
    delta: str = ""
    final: bool = False
    metadata: dict[str, Any] = {}


class MLLMServerOutputTranscript(BaseModel):
    """
    Model for MLLM response transcript.
    This model is used to define the structure of the transcript data for MLLM responses.
    """
    content: str = ""
    delta: str = ""
    final: bool = False
    metadata: dict[str, Any] = {}

class MLLMServerFunctionCall(BaseModel):
    """
    Model for MLLM server function call.
    This model is used to define the structure of function calls made by the MLLM server.
    """
    call_id: str
    name: str
    arguments: str
    metadata: dict[str, Any] = {}