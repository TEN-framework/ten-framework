from pydantic import BaseModel
from typing import Literal, Optional, Union, Dict, Any
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

class SessionReadyEvent(AgentEventBase):
    """Event triggered when the session is ready."""
    type: Literal["data"] = "data"
    name: Literal["mllm_server_session_ready"] = "mllm_server_session_ready"
    metadata: Dict[str, Any]

class ServerInterruptEvent(AgentEventBase):
    """Event triggered when the server is interrupted."""
    type: Literal["data"] = "data"
    name: Literal["mllm_server_interrupt"] = "mllm_server_interrupt"
    metadata: Dict[str, Any]

class InputTranscriptEvent(AgentEventBase):
    """Event triggered when MLLM request transcript is received (partial or final)."""
    type: Literal["data"] = "data"
    name: Literal["mllm_server_input_transcript"] = "mllm_server_input_transcript"
    content: Optional[str] = None
    delta: Optional[str] = None
    final: bool
    metadata: Dict[str, Any]


class OutputTranscriptEvent(AgentEventBase):
    """Event triggered when LLM returns a streaming response."""
    type: Literal["data"] = "data"
    name: Literal["mllm_server_output_transcript"] = "mllm_server_output_transcript"
    delta: str
    content: str
    is_final: bool
    metadata: Dict[str, Any]

class FunctionCallEvent(AgentEventBase):
    """Event triggered when a function call is made by the MLLM server."""
    type: Literal["data"] = "data"
    name: Literal["mllm_server_function_call"] = "mllm_server_function_call"
    call_id: str
    function_name: str
    arguments: str

# ==== Unified Event Union ====

AgentEvent = Union[
    UserJoinedEvent,
    UserLeftEvent,
    ToolRegisterEvent,
    InputTranscriptEvent,
    OutputTranscriptEvent,
    SessionReadyEvent,
    ServerInterruptEvent,
    FunctionCallEvent
]
