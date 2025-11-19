from pydantic import BaseModel
from typing import Literal, Union, Dict, Any, Optional
from ten_ai_base.types import LLMToolMetadata


# ==== Base Event ====


class AgentEventBase(BaseModel):
    """Base class for all agent-level events."""

    type: Literal["cmd", "data"]
    name: str


# ==== CMD Events ====


class UserJoinedEvent(AgentEventBase):
    """Event triggered when a user joins the session."""

    type: Literal["cmd"] = "cmd"
    name: Literal["on_user_joined"] = "on_user_joined"


class UserLeftEvent(AgentEventBase):
    """Event triggered when a user leaves the session."""

    type: Literal["cmd"] = "cmd"
    name: Literal["on_user_left"] = "on_user_left"


class ToolRegisterEvent(AgentEventBase):
    """Event triggered when a tool is registered by the user."""

    type: Literal["cmd"] = "cmd"
    name: Literal["tool_register"] = "tool_register"
    tool: LLMToolMetadata
    source: str


# ==== DATA Events ====


class ASRResultEvent(AgentEventBase):
    """Event triggered when ASR result is received (partial or final)."""

    type: Literal["data"] = "data"
    name: Literal["asr_result"] = "asr_result"
    text: str
    final: bool
    metadata: Dict[str, Any]


class LLMResponseEvent(AgentEventBase):
    """Event triggered when LLM returns a streaming response."""

    type: Literal["message", "reasoning"] = "message"
    name: Literal["llm_response"] = "llm_response"
    delta: str
    text: str
    is_final: bool


class RTMMessageEvent(AgentEventBase):
    """Event triggered when an RTM message is received from a user."""

    type: Literal["data"] = "data"
    name: Literal["rtm_message_event"] = "rtm_message_event"
    channelName: str
    message: dict[str, Any]
    publisher: str
    customType: Optional[str] = None
    timestamp: int = 0


# ==== Unified Event Union ====

AgentEvent = Union[
    UserJoinedEvent,
    UserLeftEvent,
    ToolRegisterEvent,
    ASRResultEvent,
    LLMResponseEvent,
    RTMMessageEvent,
]
