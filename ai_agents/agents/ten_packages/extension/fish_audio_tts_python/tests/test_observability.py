import asyncio
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

from fish_audio_tts_python.config import FishAudioTTSConfig
from fish_audio_tts_python.fish_audio_tts import (
    EVENT_TTS_END,
    EVENT_TTS_RESPONSE,
    FishAudioTTSClient,
)


class FakeSession:
    def __init__(self, events: list[str] | None = None) -> None:
        self.events = events if events is not None else []
        self._client = SimpleNamespace(
            headers={},
            event_hooks={"response": []},
        )

    async def tts(self, **_kwargs):
        self.events.append("tts")
        yield b"audio"

    async def close(self) -> None:
        return None


def test_traceparent_is_sent_and_response_headers_are_logged():
    async def run_test() -> None:
        session = FakeSession()
        ten_env = MagicMock()

        with patch(
            "fish_audio_tts_python.fish_audio_tts.AsyncWebSocketSession",
            return_value=session,
        ):
            client = FishAudioTTSClient(
                FishAudioTTSConfig(api_key="test-key"), ten_env
            )

        stream = client.get("hello")
        assert await stream.__anext__() == (
            b"audio",
            EVENT_TTS_RESPONSE,
        )
        assert await stream.__anext__() == (None, EVENT_TTS_END)

        traceparent = session._client.headers["traceparent"]
        version, trace_id, span_id, flags = traceparent.split("-")
        assert version == "00"
        assert len(trace_id) == 32
        assert len(span_id) == 16
        assert flags == "01"

        response = SimpleNamespace(
            headers={
                "x-fishaudio-datacenter": "us-east-1",
                "x-request-id": "request-123",
            }
        )
        await session._client.event_hooks["response"][0](response)

        ten_env.log_info.assert_called_once()
        log_message = ten_env.log_info.call_args.args[0]
        assert "us-east-1" in log_message
        assert trace_id in log_message
        assert "x-request-id" in log_message

        await client.clean()

    asyncio.run(run_test())


def test_request_start_callback_runs_before_sdk_tts():
    async def run_test() -> None:
        events: list[str] = []
        session = FakeSession(events)
        ten_env = MagicMock()

        with patch(
            "fish_audio_tts_python.fish_audio_tts.AsyncWebSocketSession",
            return_value=session,
        ):
            client = FishAudioTTSClient(
                FishAudioTTSConfig(api_key="test-key"),
                ten_env,
                on_request_start=lambda: events.append("request_start"),
            )

        stream = client.get("hello")
        await stream.__anext__()

        assert events == ["request_start", "tts"]
        await client.clean()

    asyncio.run(run_test())
