import json
from dataclasses import asdict, dataclass, is_dataclass
from typing import Any, Literal


@dataclass
class UnknownMessage:
    raw: dict[str, Any]


@dataclass
class ErrorMessage:
    error: str
    raw: dict[str, Any]


@dataclass
class AuthSuccess:
    call_id: str
    agent_id: str
    agent_type: str
    raw: dict[str, Any]


@dataclass
class AuthError:
    message: str
    raw: dict[str, Any]


@dataclass
class BridgeStatus:
    state: str
    call_id: str
    raw: dict[str, Any]


@dataclass
class TranscriptMessage:
    text: str
    final: bool
    direction: Literal["input", "output"]
    raw: dict[str, Any]


EXPLICIT_TRANSCRIPT_DIRECTIONS: dict[str, Literal["input", "output"]] = {
    "asr": "input",
    "tts": "output",
}


@dataclass
class BinaryAudioMessage:
    audio: bytes


def _nested_value(data: dict[str, Any], *keys: str) -> Any:
    current: Any = data
    for key in keys:
        if not isinstance(current, dict):
            return None
        current = current.get(key)
    return current


def _string_value(data: dict[str, Any], *paths: tuple[str, ...]) -> str:
    for path in paths:
        value = _nested_value(data, *path)
        if value is None:
            continue
        if isinstance(value, str):
            return value
        return str(value)
    return ""


def _type_name(data: dict[str, Any]) -> str:
    type_name = _string_value(
        data,
        ("type",),
        ("event",),
        ("action",),
        ("header", "name"),
    )
    return type_name.lower().replace("-", "_").replace(".", "_")


def _extract_text(data: dict[str, Any]) -> str:
    for path in (
        ("text",),
        ("content",),
        ("message",),
        ("transcript",),
        ("data", "text"),
        ("data", "content"),
        ("payload", "text"),
        ("payload", "content"),
    ):
        value = _string_value(data, path)
        if value:
            return value
    return ""


def _infer_direction(type_name: str, data: dict[str, Any]) -> str | None:
    combined = f"{type_name} {_string_value(data, ('role',), ('speaker',), ('source',))}".lower()
    if any(token in combined for token in ("agent", "assistant", "bot", "output", "response")):
        return "output"
    if any(token in combined for token in ("user", "input", "customer")):
        return "input"
    return None


def _infer_final(type_name: str, data: dict[str, Any]) -> bool:
    if "final" in data and isinstance(data["final"], bool):
        return data["final"]
    if "is_final" in data and isinstance(data["is_final"], bool):
        return data["is_final"]
    return any(token in type_name for token in ("final", "done", "completed"))


def parse_server_message(message: str) -> Any:
    data = json.loads(message)
    type_name = _type_name(data)

    if type_name == "auth_success":
        user = _nested_value(data, "user")
        user = user if isinstance(user, dict) else {}
        return AuthSuccess(
            call_id=_string_value(data, ("call_id",), ("user", "call_id")),
            agent_id=_string_value(user, ("agent_id",)),
            agent_type=_string_value(user, ("agent_type",)),
            raw=data,
        )

    if type_name == "auth_error":
        return AuthError(
            message=_string_value(data, ("message",), ("error", "message")),
            raw=data,
        )

    if type_name == "bridge_status":
        return BridgeStatus(
            state=_string_value(data, ("upstream", "state"), ("state",)),
            call_id=_string_value(data, ("upstream", "call_id"), ("call_id",)),
            raw=data,
        )

    if "error" in data:
        error = data["error"]
        if isinstance(error, dict):
            return ErrorMessage(
                error=_string_value(error, ("message",), ("code",)),
                raw=data,
            )
        return ErrorMessage(error=str(error), raw=data)

    text = _extract_text(data)
    explicit_direction = EXPLICIT_TRANSCRIPT_DIRECTIONS.get(type_name)
    if text and explicit_direction is not None:
        return TranscriptMessage(
            text=text,
            final=_infer_final(type_name, data) or True,
            direction=explicit_direction,
            raw=data,
        )

    direction = _infer_direction(type_name, data)
    if text and direction is not None:
        return TranscriptMessage(
            text=text,
            final=_infer_final(type_name, data),
            direction=direction,
            raw=data,
        )

    return UnknownMessage(raw=data)


def to_json(message: Any) -> str:
    if is_dataclass(message):
        payload = asdict(message)
    else:
        payload = message
    return json.dumps(payload)
