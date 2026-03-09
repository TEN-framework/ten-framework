import json
import uuid
from dataclasses import asdict, dataclass, is_dataclass
from typing import Any


def generate_event_id() -> str:
    return str(uuid.uuid4())


@dataclass
class UnknownMessage:
    raw: dict[str, Any]


@dataclass
class ErrorMessage:
    error: str
    raw: dict[str, Any]


@dataclass
class SessionCreated:
    session_id: str
    raw: dict[str, Any]


@dataclass
class SessionUpdated:
    session_id: str
    raw: dict[str, Any]


@dataclass
class InputTranscriptDelta:
    item_id: str
    delta: str
    raw: dict[str, Any]


@dataclass
class InputTranscriptCompleted:
    item_id: str
    transcript: str
    raw: dict[str, Any]


@dataclass
class InputTranscriptFailed:
    item_id: str
    error: str
    raw: dict[str, Any]


@dataclass
class ResponseCreated:
    response_id: str
    raw: dict[str, Any]


@dataclass
class ResponseDone:
    response_id: str
    status: str
    raw: dict[str, Any]


@dataclass
class ResponseTextDelta:
    response_id: str
    item_id: str
    delta: str
    raw: dict[str, Any]


@dataclass
class ResponseTextDone:
    response_id: str
    item_id: str
    text: str
    raw: dict[str, Any]


@dataclass
class ResponseAudioDelta:
    response_id: str
    item_id: str
    delta: str
    raw: dict[str, Any]


@dataclass
class ResponseAudioDone:
    response_id: str
    item_id: str
    raw: dict[str, Any]


@dataclass
class SpeechStarted:
    response_id: str
    raw: dict[str, Any]


@dataclass
class SpeechStopped:
    audio_end_ms: int
    raw: dict[str, Any]


@dataclass
class FunctionCallArgumentsDone:
    call_id: str
    name: str
    arguments: str
    raw: dict[str, Any]


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


def _int_value(data: dict[str, Any], *paths: tuple[str, ...]) -> int:
    for path in paths:
        value = _nested_value(data, *path)
        if value is None:
            continue
        try:
            return int(value)
        except (TypeError, ValueError):
            continue
    return 0


def _type_name(data: dict[str, Any]) -> str:
    type_name = _string_value(
        data,
        ("type",),
        ("event",),
        ("action",),
        ("header", "name"),
    )
    return type_name.lower().replace("-", ".").replace("_", ".")


def parse_server_message(message: str) -> Any:
    data = json.loads(message)
    type_name = _type_name(data)

    if type_name in {"session.created", "session.ready"}:
        return SessionCreated(
            session_id=_string_value(data, ("session", "id"), ("session_id",)),
            raw=data,
        )
    if type_name in {"session.updated", "session.started"}:
        return SessionUpdated(
            session_id=_string_value(data, ("session", "id"), ("session_id",)),
            raw=data,
        )
    if type_name in {
        "conversation.item.input.audio.transcription.delta",
        "input.transcript.delta",
        "transcript.delta",
    }:
        return InputTranscriptDelta(
            item_id=_string_value(data, ("item_id",), ("item", "id")),
            delta=_string_value(data, ("delta",), ("transcript", "delta")),
            raw=data,
        )
    if type_name in {
        "conversation.item.input.audio.transcription.completed",
        "input.transcript.completed",
        "transcript.completed",
    }:
        return InputTranscriptCompleted(
            item_id=_string_value(data, ("item_id",), ("item", "id")),
            transcript=_string_value(
                data, ("transcript",), ("text",), ("content",)
            ),
            raw=data,
        )
    if type_name in {
        "conversation.item.input.audio.transcription.failed",
        "input.transcript.failed",
        "transcript.failed",
    }:
        return InputTranscriptFailed(
            item_id=_string_value(data, ("item_id",), ("item", "id")),
            error=_string_value(
                data, ("error", "message"), ("error",), ("message",)
            ),
            raw=data,
        )
    if type_name in {"response.created", "response.started"}:
        return ResponseCreated(
            response_id=_string_value(
                data, ("response", "id"), ("response_id",)
            ),
            raw=data,
        )
    if type_name in {"response.done", "response.completed"}:
        return ResponseDone(
            response_id=_string_value(
                data, ("response", "id"), ("response_id",)
            ),
            status=_string_value(
                data, ("response", "status"), ("status",)
            ),
            raw=data,
        )
    if type_name in {"response.text.delta", "output.text.delta"}:
        return ResponseTextDelta(
            response_id=_string_value(data, ("response_id",)),
            item_id=_string_value(data, ("item_id",), ("item", "id")),
            delta=_string_value(data, ("delta",), ("text",)),
            raw=data,
        )
    if type_name in {"response.text.done", "output.text.done"}:
        return ResponseTextDone(
            response_id=_string_value(data, ("response_id",)),
            item_id=_string_value(data, ("item_id",), ("item", "id")),
            text=_string_value(data, ("text",), ("content",)),
            raw=data,
        )
    if type_name in {
        "response.audio.delta",
        "output.audio.delta",
        "audio.delta",
    }:
        return ResponseAudioDelta(
            response_id=_string_value(data, ("response_id",)),
            item_id=_string_value(data, ("item_id",), ("item", "id")),
            delta=_string_value(data, ("delta",), ("audio",)),
            raw=data,
        )
    if type_name in {"response.audio.done", "output.audio.done", "audio.done"}:
        return ResponseAudioDone(
            response_id=_string_value(data, ("response_id",)),
            item_id=_string_value(data, ("item_id",), ("item", "id")),
            raw=data,
        )
    if type_name in {
        "input.audio.buffer.speech.started",
        "speech.started",
        "vad.started",
    }:
        return SpeechStarted(
            response_id=_string_value(data, ("response_id",)),
            raw=data,
        )
    if type_name in {
        "input.audio.buffer.speech.stopped",
        "speech.stopped",
        "vad.stopped",
    }:
        return SpeechStopped(
            audio_end_ms=_int_value(data, ("audio_end_ms",), ("audio_end",)),
            raw=data,
        )
    if type_name in {
        "response.function.call.arguments.done",
        "function.call.arguments.done",
        "tool.call",
    }:
        arguments = _nested_value(data, "arguments")
        if isinstance(arguments, dict):
            arguments = json.dumps(arguments)
        return FunctionCallArgumentsDone(
            call_id=_string_value(data, ("call_id",), ("tool_call_id",)),
            name=_string_value(data, ("name",), ("tool", "name")),
            arguments=arguments if isinstance(arguments, str) else "",
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

    return UnknownMessage(raw=data)


def to_json(message: Any) -> str:
    if is_dataclass(message):
        payload = asdict(message)
    else:
        payload = message
    return json.dumps(payload)


@dataclass
class InputAudioBufferAppend:
    audio: str
    type: str = "input_audio_buffer.append"
    event_id: str = generate_event_id()


@dataclass
class ResponseCreate:
    type: str = "response.create"
    event_id: str = generate_event_id()


@dataclass
class SessionUpdate:
    session: dict[str, Any]
    type: str = "session.update"
    event_id: str = generate_event_id()


@dataclass
class ItemCreate:
    item: dict[str, Any]
    type: str = "conversation.item.create"
    event_id: str = generate_event_id()

