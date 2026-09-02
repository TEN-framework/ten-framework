import asyncio
import json
from unittest.mock import AsyncMock, patch

import pytest
from client import SpekoASRClient, SpekoRouterError, websocket_url


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

    def __aiter__(self):
        return self

    async def __anext__(self):
        if self.closed:
            raise StopAsyncIteration
        return await self.recv()

    async def close(self):
        self.closed = True


def make_client(events, disconnects):
    return SpekoASRClient(
        api_key="test-key",
        base_url="https://router.speko.dev",
        configure={
            "type": "session.configure",
            "audio": {
                "encoding": "pcm_s16le",
                "sample_rate_hz": 16000,
                "channels": 1,
            },
        },
        ready_timeout_sec=1,
        finalize_timeout_sec=1,
        on_event=AsyncMock(side_effect=lambda event: events.append(event)),
        on_disconnect=AsyncMock(
            side_effect=lambda error: disconnects.append(error)
        ),
    )


@pytest.mark.asyncio
async def test_streams_audio_and_waits_for_final_transcript():
    ready = json.dumps(
        {
            "type": "session.ready",
            "request_id": "req_1",
            "route": {
                "provider": "deepgram",
                "model": "nova-3",
                "region": "us",
                "attempt_id": "att_1",
            },
        }
    )
    websocket = FakeWebSocket(ready)
    events = []
    disconnects = []
    client = make_client(events, disconnects)

    with patch(
        "client.websockets.connect", AsyncMock(return_value=websocket)
    ) as connect:
        await client.connect()
        await client.send_audio(b"\x00\x01")
        commit_task = asyncio.create_task(client.commit())
        await asyncio.sleep(0)
        await websocket.messages.put(
            json.dumps({"type": "transcript.delta", "text": "hel"})
        )
        await websocket.messages.put(
            json.dumps({"type": "transcript.final", "text": "hello"})
        )
        await commit_task

    assert connect.await_args.args[0] == "wss://router.speko.dev/v1/stt/stream"
    headers = connect.await_args.kwargs["additional_headers"]
    assert headers["Authorization"] == "Bearer test-key"
    assert headers["Idempotency-Key"]
    assert json.loads(websocket.sent[0])["type"] == "session.configure"
    assert websocket.sent[1] == b"\x00\x01"
    assert json.loads(websocket.sent[2]) == {"type": "input.commit"}
    assert [event["type"] for event in events] == [
        "session.ready",
        "transcript.delta",
        "transcript.final",
    ]
    assert disconnects == []
    await client.close()


@pytest.mark.asyncio
async def test_terminal_error_is_normalized():
    ready = json.dumps(
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
    websocket = FakeWebSocket(ready)
    events = []
    disconnects = []
    client = make_client(events, disconnects)
    with patch("client.websockets.connect", AsyncMock(return_value=websocket)):
        await client.connect()
        await websocket.messages.put(
            json.dumps(
                {
                    "type": "error",
                    "error": {
                        "code": "provider_unavailable",
                        "message": "try later",
                        "hint": "retry",
                        "retryable": True,
                    },
                }
            )
        )
        for _ in range(20):
            if disconnects:
                break
            await asyncio.sleep(0)

    assert isinstance(disconnects[0], SpekoRouterError)
    assert disconnects[0].code == "provider_unavailable"
    assert disconnects[0].retryable is True
    assert client.is_ready is False


def test_websocket_url_preserves_base_path():
    assert websocket_url("http://localhost:8080/router/", "/v1/stt/stream") == (
        "ws://localhost:8080/router/v1/stt/stream"
    )
