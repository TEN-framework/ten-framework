"""Pure protocol helpers for the Agora Mind TEN bridge."""

from __future__ import annotations

from datetime import datetime, timezone
import json
from typing import Any


def utc_timestamp(value: Any = None) -> str:
    """Return an ISO-8601 UTC timestamp suitable for pure-text input."""

    if isinstance(value, str) and value.strip():
        return value
    return (
        datetime.now(timezone.utc)
        .isoformat(timespec="milliseconds")
        .replace("+00:00", "Z")
    )


def pure_text_input(text: str, created_at: Any = None) -> dict[str, str]:
    """Build the input object expected by Agora Mind's pure-text format."""

    return {"created_at": utc_timestamp(created_at), "text": text}


def speech_final_from_payload(payload: dict[str, Any]) -> bool:
    """Return whether an ASR final closes the current speech interval."""

    if payload.get("speech_final") is True:
        return True
    metadata = payload.get("metadata")
    asr_info = metadata.get("asr_info") if isinstance(metadata, dict) else None
    if isinstance(asr_info, dict) and "speech_final" in asr_info:
        return asr_info.get("speech_final") is True
    return "speech_final" not in payload


def asr_final_key(
    payload: dict[str, Any], text: str
) -> tuple[int, int, str] | None:
    """Identify a final within the graph's single ASR audio timeline."""

    start_ms = payload.get("start_ms")
    duration_ms = payload.get("duration_ms")
    if any(
        not isinstance(value, int) or isinstance(value, bool)
        for value in (start_ms, duration_ms)
    ):
        return None
    if start_ms < 0 or duration_ms <= 0:
        return None
    return start_ms, duration_ms, text


def session_id_from_response(response_json: str) -> int | None:
    """Extract a session ID from a successful ``session.created`` response."""

    try:
        response = json.loads(response_json)
    except (TypeError, ValueError):
        return None
    if not isinstance(response, dict):
        return None
    session_id = response.get("session_id")
    if isinstance(session_id, bool):
        return None
    if isinstance(session_id, int) and session_id > 0:
        return session_id
    return None


def input_sequence_from_response(response_json: str, session_id: int) -> int:
    """Validate the acceptance receipt before counting a submitted input."""
    response = json.loads(response_json)
    if not isinstance(response, dict):
        raise ValueError("invalid input acceptance receipt")
    sequence = response.get("input_sequence")
    if (
        response.get("type") != "session.user_input.accepted"
        or response.get("session_id") != session_id
        or not _positive_integer(sequence)
    ):
        raise ValueError("invalid input acceptance receipt")
    return sequence


def turn_from_event(
    event: Any, expected_session_id: int
) -> tuple[int, int, str] | None:
    """Read one normal top-level completion, including an empty reply."""
    if (
        not isinstance(event, dict)
        or event.get("type") != "session_turn_completed"
        or event.get("session_id") != expected_session_id
        or event.get("source") != {"type": "top_level"}
    ):
        return None
    sequence_range = event.get("input_sequence_range")
    if not isinstance(sequence_range, dict):
        raise ValueError("Turn omitted its input range")
    first, last = sequence_range.get("first"), sequence_range.get("last")
    if not all(_positive_integer(value) for value in (first, last)):
        raise ValueError("Turn has an invalid input range")
    if first > last or event.get("stop_reason") is not None:
        raise ValueError("Turn did not complete normally")
    result = event.get("result")
    if not isinstance(result, list):
        raise ValueError("Turn omitted its result")
    text = "".join(
        item["text"]
        for item in result
        if isinstance(item, dict)
        and item.get("type") == "text"
        and isinstance(item.get("text"), str)
    )
    return first, last, text


def _positive_integer(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool) and value > 0
