from dataclasses import dataclass, field
from typing import Any, Dict, Optional


@dataclass
class AgentEvent:
    """Base class for agent events"""
    type: str
    data: Any = None
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class ASRResultEvent(AgentEvent):
    """Event received from ASR when speech is recognized"""
    type: str = "asr_result"
    text: str = ""
    final: bool = False
    confidence: float = 0.0


@dataclass
class LLMResponseEvent(AgentEvent):
    """Event received from LLM when response is generated"""
    type: str = "llm_response"
    text: str = ""
    delta: str = ""
    is_final: bool = False


@dataclass
class ToolRegisterEvent(AgentEvent):
    """Event received when a tool is registered"""
    type: str = "tool_register"
    tool: Dict[str, Any] = field(default_factory=dict)
    source: str = ""


@dataclass
class UserJoinedEvent(AgentEvent):
    """Event when a user joins the conversation"""
    type: str = "user_joined"
    user_id: str = ""
    session_id: str = ""


@dataclass
class UserLeftEvent(AgentEvent):
    """Event when a user leaves the conversation"""
    type: str = "user_left"
    user_id: str = ""
    session_id: str = ""