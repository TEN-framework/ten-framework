#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Unit tests for the AssemblyAI v3 WebSocket client (no network)."""

import asyncio
import json
from typing import Any, Dict, Optional
from urllib.parse import parse_qs, urlparse

import pytest
from websockets.protocol import State

from ..recognition import (
    AssemblyAIConnectionError,
    AssemblyAIWSRecognition,
    AssemblyAIWSRecognitionCallback,
)


class _FakeTenEnv:
    def log_debug(self, *args, **kwargs) -> None:
        pass

    def log_info(self, *args, **kwargs) -> None:
        pass

    def log_warn(self, *args, **kwargs) -> None:
        pass

    def log_error(self, *args, **kwargs) -> None:
        pass


class _RecordingCallback(AssemblyAIWSRecognitionCallback):
    def __init__(self) -> None:
        self.opened: list = []
        self.results: list = []
        self.events: list = []
        self.errors: list = []
        self.closes: list = []

    async def on_open(self, session_id: str, configuration: Dict[str, Any]):
        self.opened.append((session_id, configuration))

    async def on_result(self, message_data: Dict[str, Any]):
        self.results.append(message_data)

    async def on_event(self, message_data: Dict[str, Any]):
        self.events.append(message_data)

    async def on_error(self, error_msg: str, error_code: Optional[str] = None):
        self.errors.append((error_msg, error_code))

    async def on_close(self, code: int, reason: str):
        self.closes.append((code, reason))


class _FakeWebSocket:
    """Records outbound messages; iteration yields queued inbound messages."""

    def __init__(self, inbound=None, state: State = State.OPEN) -> None:
        self.sent: list = []
        self.state = state
        self._inbound = list(inbound or [])
        self.closed = False

    async def send(self, message) -> None:
        self.sent.append(message)

    async def close(self) -> None:
        self.closed = True
        self.state = State.CLOSED

    def __aiter__(self):
        return self

    async def __anext__(self):
        if not self._inbound:
            raise StopAsyncIteration
        item = self._inbound.pop(0)
        if isinstance(item, BaseException):
            raise item
        return item


def _make(config=None, callback=None, ws=None):
    callback = callback or _RecordingCallback()
    recognition = AssemblyAIWSRecognition(
        api_key="fake_key",
        ws_url="wss://streaming.assemblyai.com/v3/ws",
        ten_env=_FakeTenEnv(),
        config=config or {},
        callback=callback,
    )
    if ws is not None:
        recognition.websocket = ws
        recognition.is_started = True
    return recognition, callback


def _sent_json(ws: _FakeWebSocket, index: int = -1) -> dict:
    return json.loads(ws.sent[index])


# --------------------------------------------------------------------------
# URL building
# --------------------------------------------------------------------------


def test_url_encodes_scalars_lists_and_booleans_like_the_official_sdk():
    recognition, _ = _make(
        config={
            "speech_model": "universal-3-5-pro",
            "sample_rate": 16000,
            "format_turns": True,
            "continuous_partials": False,
            "vad_threshold": 0.3,
            "keyterms_prompt": ["TEN Framework", "AssemblyAI"],
            "language_codes": ["en", "es"],
            "prompt": "Support call about billing & refunds?",
        }
    )

    url = recognition._build_websocket_url()
    parsed = urlparse(url)
    query = parse_qs(parsed.query, keep_blank_values=True)

    assert parsed.scheme == "wss"
    assert parsed.path == "/v3/ws"
    assert query["speech_model"] == ["universal-3-5-pro"]
    assert query["sample_rate"] == ["16000"]
    assert query["format_turns"] == ["true"]
    assert query["continuous_partials"] == ["false"]
    assert query["vad_threshold"] == ["0.3"]
    assert json.loads(query["keyterms_prompt"][0]) == [
        "TEN Framework",
        "AssemblyAI",
    ]
    assert json.loads(query["language_codes"][0]) == ["en", "es"]
    assert query["prompt"] == ["Support call about billing & refunds?"]


def test_url_omits_none_values_and_never_leaks_the_api_key():
    recognition, _ = _make(config={"sample_rate": 16000, "mode": None})

    url = recognition._build_websocket_url()
    query = parse_qs(urlparse(url).query)

    assert "mode" not in query
    assert "fake_key" not in url
    assert "token" not in query


# --------------------------------------------------------------------------
# Client -> server messages (exact casing matters: wrong type closes session)
# --------------------------------------------------------------------------


def test_force_endpoint_sends_pascal_case_message():
    ws = _FakeWebSocket()
    recognition, _ = _make(ws=ws)

    asyncio.run(recognition.force_endpoint())

    assert _sent_json(ws) == {"type": "ForceEndpoint"}


def test_update_configuration_sends_pascal_case_message_with_fields():
    ws = _FakeWebSocket()
    recognition, _ = _make(ws=ws)

    asyncio.run(
        recognition.send_update_configuration(
            {"agent_context": "What is your email?", "min_turn_silence": 500}
        )
    )

    assert _sent_json(ws) == {
        "type": "UpdateConfiguration",
        "agent_context": "What is your email?",
        "min_turn_silence": 500,
    }


def test_close_sends_terminate_before_closing_the_socket():
    ws = _FakeWebSocket()
    recognition, _ = _make(ws=ws)

    asyncio.run(recognition.close())

    assert _sent_json(ws, 0) == {"type": "Terminate"}
    assert ws.closed is True
    assert recognition.is_connected() is False


# --------------------------------------------------------------------------
# Server -> client messages
# --------------------------------------------------------------------------


def test_begin_message_reports_session_id_and_configuration_via_on_open():
    ws = _FakeWebSocket()
    recognition, callback = _make(ws=ws)
    recognition.is_started = False

    asyncio.run(
        recognition._handle_message(
            json.dumps(
                {
                    "type": "Begin",
                    "id": "sess-123",
                    "expires_at": 1700000000,
                    "configuration": {"model": "universal-3-5-pro"},
                }
            )
        )
    )

    assert callback.opened == [("sess-123", {"model": "universal-3-5-pro"})]
    assert recognition.session_id == "sess-123"
    assert recognition.is_connected() is True


def test_turn_message_is_routed_to_on_result():
    recognition, callback = _make()
    turn = {"type": "Turn", "transcript": "hello", "end_of_turn": True}

    asyncio.run(recognition._handle_message(json.dumps(turn)))

    assert callback.results == [turn]
    assert callback.errors == []


@pytest.mark.parametrize(
    "message_type",
    ["SpeechStarted", "Heartbeat", "SpeakerRevision", "Termination", "Future"],
)
def test_informational_messages_are_events_not_errors(message_type):
    recognition, callback = _make()

    asyncio.run(recognition._handle_message(json.dumps({"type": message_type})))

    assert callback.errors == []
    assert [e["type"] for e in callback.events] == [message_type]


def test_malformed_json_is_reported_as_error():
    recognition, callback = _make()

    asyncio.run(recognition._handle_message("{not json"))

    assert len(callback.errors) == 1
    assert callback.errors[0][1] is None


# --------------------------------------------------------------------------
# Connection lifecycle
# --------------------------------------------------------------------------


def test_connection_closed_reports_code_and_reason_via_on_close():
    from websockets.exceptions import ConnectionClosed
    from websockets.frames import Close

    ws = _FakeWebSocket(
        inbound=[ConnectionClosed(Close(3006, "invalid message type"), None)]
    )
    recognition, callback = _make(ws=ws)

    asyncio.run(recognition._message_handler())

    assert callback.closes == [(3006, "invalid message type")]
    assert callback.errors == []
    assert recognition.is_connected() is False


def test_abnormal_closure_without_close_frame_reports_1006():
    from websockets.exceptions import ConnectionClosed

    ws = _FakeWebSocket(inbound=[ConnectionClosed(None, None)])
    recognition, callback = _make(ws=ws)

    asyncio.run(recognition._message_handler())

    assert callback.closes[0][0] == 1006


def test_handshake_rejection_raises_connection_error_with_http_status():
    from websockets.exceptions import InvalidStatus
    from websockets.http11 import Response
    from websockets.datastructures import Headers

    recognition, _ = _make()
    response = Response(401, "Unauthorized", Headers())

    async def fake_connect(*args, **kwargs):
        raise InvalidStatus(response)

    with pytest.raises(AssemblyAIConnectionError) as exc_info:
        asyncio.run(recognition.start(connect=fake_connect))

    assert exc_info.value.code == "401"
    assert "401" in str(exc_info.value)
    assert recognition.is_connected() is False


def test_client_initiated_close_does_not_report_on_close():
    """A close we asked for must not look like a vendor disconnect (which
    would trigger the extension's reconnect path)."""
    from websockets.exceptions import ConnectionClosed
    from websockets.frames import Close

    ws = _FakeWebSocket(inbound=[ConnectionClosed(Close(1000, ""), None)])
    recognition, callback = _make(ws=ws)

    async def scenario():
        await recognition.close()
        await recognition._message_handler()

    asyncio.run(scenario())

    assert callback.closes == []


def test_server_close_stops_the_audio_consumer_task():
    """A server-initiated close must not leave the consumer loop spinning."""
    from websockets.exceptions import ConnectionClosed
    from websockets.frames import Close

    ws = _FakeWebSocket(
        inbound=[ConnectionClosed(Close(3005, "cancelled"), None)]
    )
    recognition, _ = _make(ws=ws)

    async def scenario():
        recognition._consumer_task = asyncio.create_task(
            recognition._consume_and_send()
        )
        await asyncio.sleep(0)
        assert not recognition._consumer_task.done()
        await recognition._message_handler()
        for _ in range(5):
            await asyncio.sleep(0)
        return recognition._consumer_task.done()

    assert asyncio.run(scenario()) is True


def test_url_log_line_redacts_secret_looking_params():
    logs = []

    class _LoggingEnv(_FakeTenEnv):
        def log_info(self, msg, **kwargs):
            logs.append(msg)

    recognition = AssemblyAIWSRecognition(
        api_key="fake_key",
        ten_env=_LoggingEnv(),
        config={"sample_rate": 16000, "token": "temp-token-value"},
        callback=_RecordingCallback(),
    )

    url = recognition._build_websocket_url()

    assert "temp-token-value" in url
    assert all("temp-token-value" not in line for line in logs)
