import asyncio
import json
from unittest.mock import AsyncMock

from qwen_asr_python.client import QwenASRClient, QwenProtocolError
from websockets.protocol import State


class FakeWebSocket:
    def __init__(self, incoming):
        self.incoming = asyncio.Queue()
        for item in incoming:
            self.incoming.put_nowait(item)
        self.sent = []
        self.state = State.OPEN

    async def send(self, payload):
        self.sent.append(payload)
        if isinstance(payload, str) and json.loads(payload).get("finish"):
            await self.incoming.put(
                json.dumps({"header": {"event": "task-finished"}, "payload": {}})
            )

    async def recv(self):
        return await self.incoming.get()

    async def close(self):
        self.state = State.CLOSED


def test_client_waits_for_task_started_before_audio(monkeypatch):
    async def run():
        fake = FakeWebSocket(
            [
                json.dumps({"header": {"event": "task-started"}, "payload": {}}),
            ]
        )
        callbacks = []
        client = QwenASRClient(
            "wss://example.test",
            "secret",
            lambda task_id: {"run": task_id},
            lambda task_id: {"finish": task_id},
            lambda event: _append(callbacks, event),
            lambda reason: _append(callbacks, {"close": reason}),
            timeout_s=1,
        )
        monkeypatch.setattr(
            "qwen_asr_python.client.websockets.connect", AsyncMock(return_value=fake)
        )
        await client.connect()
        assert client.task_ready
        await client.send_audio(b"pcm")
        await client.finish_task()
        assert json.loads(fake.sent[0]) == {"run": client.task_id}
        assert fake.sent[1] == b"pcm"
        assert json.loads(fake.sent[2]) == {"finish": client.task_id}
        await client.close()

    asyncio.run(run())


async def _append(items, value):
    items.append(value)


def test_client_reports_task_failure(monkeypatch):
    async def run():
        fake = FakeWebSocket(
            [
                json.dumps(
                    {
                        "header": {"event": "task-failed"},
                        "payload": {"error": {"code": "bad", "message": "no"}},
                    }
                )
            ]
        )
        client = QwenASRClient(
            "wss://example.test",
            "secret",
            lambda task_id: {},
            lambda task_id: {},
            lambda event: asyncio.sleep(0),
            lambda reason: asyncio.sleep(0),
            timeout_s=1,
        )
        monkeypatch.setattr(
            "qwen_asr_python.client.websockets.connect", AsyncMock(return_value=fake)
        )
        try:
            await client.connect()
        except QwenProtocolError as exc:
            assert exc.code == "bad"
        else:
            raise AssertionError("expected task failure")

    asyncio.run(run())
