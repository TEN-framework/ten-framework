# ABOUTME: Parses update_configs agent_text payloads for Soniox RT STT.
# ABOUTME: Builds chunked WebSocket agent_text frames under the per-message size limit.
from __future__ import annotations

from typing import Any

AGENT_TEXT_WS_TYPE = "agent_text"


def parse_agent_text_update(
    params: dict[str, Any] | None,
) -> tuple[str, bool] | None:
    if not params:
        return None
    segment = params.get("agent_text")
    if not isinstance(segment, dict):
        return None
    raw_text = segment.get("text", "")
    text = raw_text if isinstance(raw_text, str) else str(raw_text)
    end_of_segment = bool(segment.get("end_of_segment", False))
    return text, end_of_segment


def chunk_agent_text(
    text: str,
    max_chars: int,
    end_of_segment: bool,
) -> list[tuple[str, bool]]:
    limit = max(max_chars, 1)
    if not text:
        return [("", end_of_segment)]
    chunks: list[tuple[str, bool]] = []
    start = 0
    while start < len(text):
        end = min(start + limit, len(text))
        piece = text[start:end]
        is_last = end >= len(text)
        chunks.append((piece, end_of_segment and is_last))
        start = end
    return chunks


def build_agent_text_frame(text: str, end_of_segment: bool) -> dict[str, Any]:
    return {
        "type": AGENT_TEXT_WS_TYPE,
        "text": text,
        "end_of_segment": end_of_segment,
    }
