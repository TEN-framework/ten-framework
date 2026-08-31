import asyncio
import json
from unittest.mock import AsyncMock, patch

import pytest
from client import (
    SpekoRouterError,
    SpekoTTSClient,
    SpekoTTSEventType,
)


class FakeWebSocket:
    def __init__(self, *messages):
        self.messages = asyncio.Queue()
        for message in messages:
            self.messages.put_nowait(message)
        self.sent = []
        self.closed = False

    async def send(self, value):
        self.sent.append(value)

    async def recv(self):
        return await self.messages.get()

    async def close(self):
        self.closed = True


def make_client():
    return SpekoTTSClient(
        api_key="test-key",
        base_url="https://router.speko.dev",
        configure={
            "type": "session.configure",
            "audio": {
                "encoding": "pcm_s16le",
                "sample_rate_hz": 24000,
                "channels": 1,
            },
        },
        ready_timeout_sec=1,
        receive_timeout_sec=1,
    )


@pytest.mark.asyncio
async def test_append_commit_audio_and_usage():
    websocket = FakeWebSocket(
        json.dumps(
            {
                "type": "session.ready",
                "request_id": "req_1",
                "route": {
                    "provider": "cartesia",
                    "model": "sonic-3",
                    "region": "us",
                    "attempt_id": "att_1",
                },
            }
        ),
        json.dumps({"type": "utterance.started", "sequence": 1}),
        b"pcm-audio",
        json.dumps({"type": "usage.updated", "usage": {"characters": 5}}),
        json.dumps({"type": "utterance.done", "sequence": 1}),
        json.dumps({"type": "session.closed", "usage": {"characters": 5}}),
    )
    client = make_client()
    with patch("client.websockets.connect", AsyncMock(return_value=websocket)):
        await client.connect()
        events = [event async for event in client.stream_text("hello")]

    assert [event.type for event in events] == [
        SpekoTTSEventType.TTFB,
        SpekoTTSEventType.AUDIO,
        SpekoTTSEventType.USAGE,
        SpekoTTSEventType.DONE,
    ]
    assert events[1].value == b"pcm-audio"
    assert client.usage == {"characters": 5}
    assert json.loads(websocket.sent[1]) == {
        "type": "input.append",
        "text": "hello",
    }
    assert json.loads(websocket.sent[2]) == {"type": "input.commit"}
    await client.close()


@pytest.mark.asyncio
async def test_cancel_sends_protocol_cancel_before_close():
    websocket = FakeWebSocket(
        json.dumps(
            {
                "type": "session.ready",
                "request_id": "req_1",
                "route": {
                    "provider": "x",
                    "model": "y",
                    "region": "us",
                    "attempt_id": "att_1",
                },
            }
        )
    )
    client = make_client()
    with patch("client.websockets.connect", AsyncMock(return_value=websocket)):
        await client.connect()
        await client.cancel()

    sent_types = [json.loads(value)["type"] for value in websocket.sent]
    assert sent_types == [
        "session.configure",
        "input.cancel",
        "session.close",
    ]
    assert websocket.closed is True


@pytest.mark.asyncio
async def test_router_error_keeps_retryable_flag():
    websocket = FakeWebSocket(
        json.dumps(
            {
                "type": "session.ready",
                "request_id": "req_1",
                "route": {
                    "provider": "x",
                    "model": "y",
                    "region": "us",
                    "attempt_id": "att_1",
                },
            }
        ),
        json.dumps(
            {
                "type": "error",
                "error": {
                    "code": "rate_limited",
                    "message": "slow down",
                    "hint": "retry",
                    "retryable": True,
                },
            }
        ),
    )
    client = make_client()
    with patch("client.websockets.connect", AsyncMock(return_value=websocket)):
        await client.connect()
        with pytest.raises(SpekoRouterError) as caught:
            _ = [event async for event in client.stream_text("hello")]

    assert caught.value.code == "rate_limited"
    assert caught.value.retryable is True


@pytest.mark.asyncio
async def test_close_surfaces_terminal_router_error():
    websocket = FakeWebSocket(
        json.dumps(
            {
                "type": "session.ready",
                "request_id": "req_1",
                "route": {
                    "provider": "x",
                    "model": "y",
                    "region": "us",
                    "attempt_id": "att_1",
                },
            }
        ),
        json.dumps({"type": "utterance.started", "sequence": 1}),
        b"pcm",
        json.dumps({"type": "utterance.done", "sequence": 1}),
        json.dumps(
            {
                "type": "error",
                "error": {
                    "code": "provider_error",
                    "message": "settlement failed",
                    "hint": "retry",
                    "retryable": True,
                },
            }
        ),
    )
    client = make_client()
    with patch("client.websockets.connect", AsyncMock(return_value=websocket)):
        await client.connect()
        _ = [event async for event in client.stream_text("hello")]
        with pytest.raises(SpekoRouterError) as caught:
            await client.close()

    assert caught.value.code == "provider_error"
    assert websocket.closed is True
