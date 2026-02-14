from ..extension import OpenclawGatewayToolExtension


def test_extract_agent_phase_with_error() -> None:
    phase = OpenclawGatewayToolExtension._extract_agent_phase(
        {
            "data": {
                "phase": "running",
                "name": "planner",
                "error": "timeout",
            }
        }
    )
    assert phase == "running · planner: timeout"


def test_extract_chat_message_text_nested_content() -> None:
    text = OpenclawGatewayToolExtension._extract_chat_message_text(
        {
            "message": {
                "content": [
                    {"type": "text", "text": "line1"},
                    {"type": "text", "text": "line2"},
                ]
            }
        }
    )
    assert text == "line1\nline2"


def test_extract_chat_timestamp_iso_string() -> None:
    ts = OpenclawGatewayToolExtension._extract_chat_timestamp(
        {"timestamp": "2026-02-14T12:30:00Z"}
    )
    assert isinstance(ts, int)
    assert ts > 0
