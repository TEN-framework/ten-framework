import asyncio
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

import ormsgpack

from fish_audio_tts_python.config import FishAudioTTSConfig
from fish_audio_tts_python.fish_audio_tts import (
    EVENT_TTS_END,
    EVENT_TTS_INVALID_KEY_ERROR,
    EVENT_TTS_RESPONSE,
    FishAudioTTSClient,
)


class FakeSession:
    def __init__(self, events: list[str]) -> None:
        self.events = events
        self.response = SimpleNamespace(
            headers={
                "x-fishaudio-datacenter": "us-east-1",
                "x-request-id": "request-123",
            }
        )
        self.sent_messages: list[dict] = []
        self.received = False

    async def send(self, message: bytes) -> None:
        self.sent_messages.append(ormsgpack.unpackb(message))

    async def recv(self) -> bytes:
        if not self.received:
            self.received = True
            return ormsgpack.packb({"event": "audio", "audio": b"audio"})
        return ormsgpack.packb({"event": "finish", "reason": "stop"})

    async def close(self) -> None:
        self.events.append("close")


class FakeConnect:
    def __init__(self, session: FakeSession, events: list[str]) -> None:
        self.session = session
        self.events = events

    async def __aenter__(self) -> FakeSession:
        self.events.append("connect")
        return self.session

    async def __aexit__(self, *_args) -> None:
        await self.session.close()

    async def close(self) -> None:
        return None


def test_traceparent_is_sent_and_response_headers_are_logged():
    async def run_test() -> None:
        events: list[str] = []
        session = FakeSession(events)
        ten_env = MagicMock()

        connector = patch(
            "fish_audio_tts_python.fish_audio_tts.connect",
            return_value=FakeConnect(session, events),
        )
        connector.start()
        try:
            client = FishAudioTTSClient(
                FishAudioTTSConfig(api_key="test-key"),
                ten_env,
                on_request_start=lambda: events.append("request_start"),
            )

            stream = client.get("hello")
            assert await stream.__anext__() == (
                b"audio",
                EVENT_TTS_RESPONSE,
            )
            assert await stream.__anext__() == (None, EVENT_TTS_END)
        finally:
            connector.stop()

        traceparent = client._traceparent
        version, trace_id, span_id, flags = traceparent.split("-")
        assert version == "00"
        assert len(trace_id) == 32
        assert len(span_id) == 16
        assert flags == "01"

        ten_env.log_info.assert_called_once()
        log_message = ten_env.log_info.call_args.args[0]
        assert "us-east-1" in log_message
        assert trace_id in log_message
        assert "x-request-id" in log_message
        assert events[:2] == ["request_start", "connect"]
        assert session.sent_messages[0]["event"] == "start"
        assert session.sent_messages[1] == {"event": "text", "text": "hello"}
        assert session.sent_messages[2] == {"event": "stop"}

        await client.clean()

    asyncio.run(run_test())


def test_request_start_callback_runs_before_sdk_tts():
    async def run_test() -> None:
        events: list[str] = []
        session = FakeSession(events)
        ten_env = MagicMock()

        connector = patch(
            "fish_audio_tts_python.fish_audio_tts.connect",
            return_value=FakeConnect(session, events),
        )
        connector.start()
        try:
            client = FishAudioTTSClient(
                FishAudioTTSConfig(api_key="test-key"),
                ten_env,
                on_request_start=lambda: events.append("request_start"),
            )

            stream = client.get("hello")
            await stream.__anext__()
        finally:
            connector.stop()

        assert events[:2] == ["request_start", "connect"]
        await client.clean()

    asyncio.run(run_test())


def test_payment_required_handshake_is_reported_as_invalid_key():
    async def run_test() -> None:
        ten_env = MagicMock()
        connector = patch(
            "fish_audio_tts_python.fish_audio_tts.connect",
            side_effect=RuntimeError("<Response [402 Payment Required]>"),
        )
        connector.start()
        try:
            client = FishAudioTTSClient(
                FishAudioTTSConfig(api_key="test-key"), ten_env
            )
            event = await client.get("hello").__anext__()
        finally:
            connector.stop()

        assert event[1] == EVENT_TTS_INVALID_KEY_ERROR
        assert b"Payment Required" in event[0]

    asyncio.run(run_test())
